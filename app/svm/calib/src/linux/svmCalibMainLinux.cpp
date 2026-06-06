#include <math.h>
#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmCalibContext.hpp"
#include "io_platform.h"

// ============================================================================
// Global State
// ============================================================================

volatile sig_atomic_t      g_quit     = 0;
volatile sig_atomic_t      g_cleaned  = 0;

static sanCalibContextUPtr s_calibCtx = nullptr;

#define KEY_STEP (1)

static void DestroyCalibContext();

// ============================================================================
// Cleanup
// ============================================================================

static void CalibProgramCleanup()
{
    if (g_cleaned != 0)
    {
        printf("Program already cleaned up.\n");
        return;
    }

    DestroyCalibContext();

    glFinish();
    APP::IO::Platform::getInstance().shutdown();

    g_cleaned = 1;
    printf("Program cleaned up.\n");
}

static void sighandler(int signal)
{
    printf("\nCaught signal %d, setting flag to quit.\n", signal);
    g_quit = 1;
    usleep(1000 * 1000);
    CalibProgramCleanup();
    exit(signal);
}

// ============================================================================
// Context Lifecycle
// ============================================================================

static void DestroyCalibContext()
{
    if (s_calibCtx == nullptr) return;
    auto* raw = s_calibCtx.release();
    if (raw != nullptr) delete raw;
    s_calibCtx = nullptr;
}

static void EnsureCalibContext()
{
    if (s_calibCtx != nullptr) return;

    std::vector<cv::Mat> srcImages;
    srcImages.reserve(SVM_CAMERAS_NUM + ADD_CAMERAS_NUM);

    for (int i = 0; i < SVM_CAMERAS_NUM; i++)
    {
        const std::string path = std::string(_SOURCE_IMAGES_PATH_) + "/src" + std::to_string(i) + ".jpg";
        srcImages.push_back(sanFromFile::read_image(path, IMREAD_COLOR));
        printf("Loaded %s\n", path.c_str());
    }
    for (int i = 0; i < ADD_CAMERAS_NUM; i++)
    {
        const std::string path = std::string(_SOURCE_IMAGES_PATH_) + "/add" + std::to_string(i) + ".jpg";
        srcImages.push_back(sanFromFile::read_image(path, IMREAD_COLOR));
        printf("Loaded %s\n", path.c_str());
    }

    s_calibCtx = sanCalibContext::Create(
        srcImages[0].ptr(), srcImages[1].ptr(),
        srcImages[2].ptr(), srcImages[3].ptr(),
        srcImages[4].ptr());

    if (s_calibCtx == nullptr)
        throw logger.svm_fatal("C10xx005", "Context::Create() $failed to create sanCalibContext");
}

// ============================================================================
// Input → Context Translation
// ============================================================================

static void printState(const CalibState* s)
{
    cout << "view: " << s->viewMode;
    if (s->viewMode == (int)FEATURES_VIEW)
        cout << "  cam: " << s->camIndex
             << "  adj: " << s->enterAdjustMode
             << "  pt: "  << s->selectedPtIdx;
    cout << endl;
}

// ── Mouse state ───────────────────────────────────────────────────────────────

struct MouseState
{
    float x           = 0.0f;
    float y           = 0.0f;
    bool  pressed     = false;
    bool  dragActive  = false;
};

static MouseState s_mouse;

// Clamp pos to [0, WND_W] x [0, WND_H] and push it to the context.
static void applyMousePosition(sanCalibContext* ctx, float rawX, float rawY)
{
    const float cx = std::max(0.0f, std::min(rawX, static_cast<float>(WND_WIDTH)));
    const float cy = std::max(0.0f, std::min(rawY, static_cast<float>(WND_HEIGHT)));
    ctx->setPointPosition(XY{cx, cy});
}

static void HandleMouseDown(float x, float y)
{
    if (s_calibCtx == nullptr) return;

    const CalibState* s = s_calibCtx->getState();
    if (s->viewMode != (int)FEATURES_VIEW || s->enterAdjustMode == false) return;

    s_mouse.x         = x;
    s_mouse.y         = y;
    s_mouse.pressed   = true;
    s_mouse.dragActive = true;

    applyMousePosition(s_calibCtx.get(), x, y);
}

static void HandleMouseMove(float x, float y)
{
    s_mouse.x = x;
    s_mouse.y = y;

    if (s_calibCtx == nullptr || s_mouse.dragActive == false) return;

    const CalibState* s = s_calibCtx->getState();
    if (s->viewMode != (int)FEATURES_VIEW || s->enterAdjustMode == false)
    {
        s_mouse.dragActive = false;
        return;
    }

    applyMousePosition(s_calibCtx.get(), x, y);
}

static void HandleMouseUp(float x, float y)
{
    if (s_calibCtx != nullptr && s_mouse.dragActive == true)
    {
        const CalibState* s = s_calibCtx->getState();
        if (s->viewMode == (int)FEATURES_VIEW && s->enterAdjustMode == true)
            applyMousePosition(s_calibCtx.get(), x, y);
    }

    s_mouse.pressed    = false;
    s_mouse.dragActive = false;
}

static void HandleInputEvent(const APP::IO::InputEvent& event)
{
    if (s_calibCtx == nullptr) return;

    // ── Mouse / touch ─────────────────────────────────────────────────────────
    if (event.type == APP::IO::InputEvent::Type::MOTION)
    {
        const APP::IO::MotionEvent& me = event.motion;
        switch (me.action)
        {
            case APP::IO::MotionAction::DOWN: HandleMouseDown(me.x, me.y); return;
            case APP::IO::MotionAction::MOVE: HandleMouseMove(me.x, me.y); return;
            case APP::IO::MotionAction::UP:   HandleMouseUp  (me.x, me.y); return;
            default: return;
        }
    }

    // ── Keyboard ──────────────────────────────────────────────────────────────
    if (event.type != APP::IO::InputEvent::Type::KEY) return;

    const APP::IO::KeyEvent& key = event.key;
    if (key.action != APP::IO::KeyAction::DOWN && key.action != APP::IO::KeyAction::MULTIPLE) return;

    const CalibState* s  = s_calibCtx->getState();
    const int         vm = s->viewMode;

    switch (key.keyCode)
    {
    // ── Quit ─────────────────────────────────────────────────────────────
    case APP::IO::KeyCode::ESCAPE:
        if (s->enterAdjustMode == true)
        {
            s_calibCtx->requestAdjustMode(false);
            cout << "Cancelled adjustment" << endl;
        }
        else
        {
            printf("The exit key was pressed.\n");
            g_quit = 1;
        }
        break;

    // ── Right arrow ───────────────────────────────────────────────────────
    case APP::IO::KeyCode::DPAD_RIGHT:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.x += KEY_STEP;
            s_calibCtx->setPointPosition(pos);
        }
        else if (vm == (int)FEATURES_VIEW)
        {
            const int nextCam = s->camIndex + 1;
            if (nextCam < SVM_CAMERAS_NUM)
                s_calibCtx->requestCamIndex(nextCam);
            else
            {
                s_calibCtx->requestViewMode(vm + 1);
                s_calibCtx->requestCamIndex(0);
            }
        }
        else if (vm < (int)LUTS_VIEW)
        {
            s_calibCtx->requestViewMode(vm + 1);
        }
        printState(s_calibCtx->getState());
        break;

    // ── Left arrow ────────────────────────────────────────────────────────
    case APP::IO::KeyCode::DPAD_LEFT:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.x -= KEY_STEP;
            s_calibCtx->setPointPosition(pos);
        }
        else if (vm == (int)FEATURES_VIEW)
        {
            const int prevCam = s->camIndex - 1;
            if (prevCam >= 0)
                s_calibCtx->requestCamIndex(prevCam);
            else
            {
                s_calibCtx->requestViewMode(vm - 1);
                s_calibCtx->requestCamIndex(0);
            }
        }
        else if (vm > (int)FISHEYE_VIEW)
        {
            s_calibCtx->requestViewMode(vm - 1);
        }
        printState(s_calibCtx->getState());
        break;

    // ── Up arrow ──────────────────────────────────────────────────────────
    case APP::IO::KeyCode::DPAD_UP:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.y -= KEY_STEP;
            s_calibCtx->setPointPosition(pos);
        }
        break;

    // ── Down arrow ────────────────────────────────────────────────────────
    case APP::IO::KeyCode::DPAD_DOWN:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.y += KEY_STEP;
            s_calibCtx->setPointPosition(pos);
        }
        break;

    // ── Enter ─────────────────────────────────────────────────────────────
    case APP::IO::KeyCode::ENTER:
        if (vm == (int)FEATURES_VIEW)
        {
            if (s->enterAdjustMode == false)
            {
                if (s_calibCtx->requestAdjustMode(true) == true)
                    cout << "Enter adjust mode — cam " << s->camIndex << ", pt 0" << endl;
                else
                    cout << "Adjust mode denied (step locked?)" << endl;
            }
            else
            {
                cout << "Confirm pt " << s->selectedPtIdx
                     << " @ (" << s->selectedPtPos.x
                     << ", "   << s->selectedPtPos.y << ")" << endl;
                s_calibCtx->advanceSelectedPoint();

                const CalibState* s2 = s_calibCtx->getState();
                if (s2->enterAdjustMode == true)
                    cout << "Next pt: " << s2->selectedPtIdx << endl;
                else
                {
                    cout << "Exit adjust mode — cam " << s->camIndex << endl;
                    s_calibCtx->notifyStepComplete((int)FEATURES_VIEW, s->camIndex);
                }
            }
        }
        break;

    default:
        break;
    }
}

// ============================================================================
// Render
// ============================================================================

static void RenderCalib()
{
    if (s_calibCtx == nullptr) return;

    sanMRT*           mrt   = s_calibCtx->getMRT();
    const CalibState* state = s_calibCtx->getState();

    mrt->setMRT();
    s_calibCtx->clearView();
    s_calibCtx->run();

    // Re-read state after run() — it may have been mutated.
    state = s_calibCtx->getState();
    const int vm  = state->viewMode;
    const int cam = state->camIndex;
    s_calibCtx->notifyStepComplete(vm, (vm == (int)FEATURES_VIEW) ? cam : -1);

    // stepDenied is set when a locked step was requested; clear it each frame.
    if (state->stepDenied == true)
        s_calibCtx->clearStepDenied();

    // Composite the offscreen MRT result onto the default framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, WND_WIDTH, WND_HEIGHT);
    mrt->UpdateBufferData(nullptr, nullptr);
    mrt->RenderMRT();

    // In adjust-mode overlay a magnified inset in the top-right quadrant.
    if (state->enterAdjustMode == true)
    {
        glViewport(
            (GLint)(3 * WND_WIDTH / 8), (GLint)(3 * WND_HEIGHT / 4),
            (GLsizei)(WND_WIDTH / 4),   (GLsizei)(WND_HEIGHT / 4));
        mrt->UpdateBufferData(nullptr, s_calibCtx->getContours()->m_magnifyingTexLUT);
        mrt->RenderMRT();
    }
}

// ============================================================================
// Main Loop — one iteration per frame
// ============================================================================

static bool CalibRunFrame()
{
    if (g_quit != 0) return false;

    APP::IO::Platform& platform = APP::IO::Platform::getInstance();

    try
    {
        platform.pollInput();
        platform.beginFrame();   // make EGL surface current before any GL work
        RenderCalib();
        platform.endFrame();     // swap buffers — must follow all GL calls
    }
    catch (const std::exception& e)
    {
        logger.record_message(e.what());
        DestroyCalibContext();
    }

    return g_quit == 0;
}

// ============================================================================
// Entry Point
// ============================================================================

int main(int argc, char** argv)
{
    signal(SIGINT,  sighandler);
    signal(SIGTERM, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGSEGV, sighandler);

    g_quit    = 0;
    g_cleaned = 0;

    try
    {
        APP::IO::Platform& platform = APP::IO::Platform::getInstance();

        if (platform.initialize("SVM Calibration", WND_WIDTH, WND_HEIGHT) == false)
            throw logger.svm_fatal("C10xx001", "Platform::initialize() $failed");

        platform.setInputCallback(HandleInputEvent);

        EnsureCalibContext();
        printf("Start calibration\n");
        printState(s_calibCtx->getState());

        printf("Entering main loop\n");
        while (CalibRunFrame() == true && platform.shouldClose() == false) {}

        glFinish();
    }
    catch (string msg)
    {
        logger.record_message(msg);
    }

    CalibProgramCleanup();
    logger.record_message(logger.svm_inform("********", "<<calibration ended>>\n").what());
    return 0;
}
