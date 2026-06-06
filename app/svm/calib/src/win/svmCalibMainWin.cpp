#include <math.h>
#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmCalibContext.hpp"

// ============================================================================
// Global State
// ============================================================================

static sanCalibContextUPtr s_calibCtx = nullptr;

#define KEY_STEP    (1.0f)
#define CHANGES_POS (0.05f)
#define CHANGES_ORI (1.0f)

// ============================================================================
// GLFW Callbacks — forward declarations
// ============================================================================

void OnKeys       (GLFWwindow* window, int key, int scancode, int action, int mods);
void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
void OnMouseMove  (GLFWwindow* window, double xpos, double ypos);

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
        std::cout << "Loaded " << path << std::endl;
    }
    for (int i = 0; i < ADD_CAMERAS_NUM; i++)
    {
        const std::string path = std::string(_SOURCE_IMAGES_PATH_) + "/add" + std::to_string(i) + ".jpg";
        srcImages.push_back(sanFromFile::read_image(path, IMREAD_COLOR));
        std::cout << "Loaded " << path << std::endl;
    }

    s_calibCtx = sanCalibContext::Create(
        srcImages[0].ptr(), srcImages[1].ptr(),
        srcImages[2].ptr(), srcImages[3].ptr(),
        srcImages[4].ptr());

    if (s_calibCtx == nullptr)
        throw logger.svm_fatal("C10xx005", "Context::Create() $failed to create sanCalibContext");
}

// ============================================================================
// Helpers
// ============================================================================

static void printState(const CalibState* s)
{
    std::cout << "view: " << s->viewMode;
    if (s->viewMode == (int)FEATURES_VIEW)
        std::cout << "  cam: " << s->camIndex
                  << "  adj: " << s->enterAdjustMode
                  << "  pt: "  << s->selectedPtIdx;
    std::cout << std::endl;
}

// Clamp pos to [0, WND_W] x [0, WND_H] and push it to the context.
static void applyMousePosition(sanCalibContext* ctx, float rawX, float rawY)
{
    const float cx = std::max(0.0f, std::min(rawX, static_cast<float>(WND_WIDTH)));
    const float cy = std::max(0.0f, std::min(rawY, static_cast<float>(WND_HEIGHT)));
    ctx->setPointPosition(XY{cx, cy});
}

// ============================================================================
// Render
// ============================================================================

static void RenderCalib()
{
    if (s_calibCtx == nullptr) return;

    sanMRT* mrt = s_calibCtx->getMRT();

    mrt->setMRT();
    s_calibCtx->clearView();
    s_calibCtx->run();

    const CalibState* state = s_calibCtx->getState();
    const int vm  = state->viewMode;
    const int cam = state->camIndex;
    s_calibCtx->notifyStepComplete(vm, (vm == (int)FEATURES_VIEW) ? cam : -1);

    if (state->stepDenied == true)
        s_calibCtx->clearStepDenied();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, WND_WIDTH, WND_HEIGHT);
    mrt->UpdateBufferData(nullptr, nullptr);
    mrt->RenderMRT();

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
// Entry Point
// ============================================================================

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    GLFWwindow* hwnd = nullptr;

    try
    {
        // ── GLFW / GL init ───────────────────────────────────────────────
        if (glfwInit() == false)
            throw logger.svm_fatal("C10xx001", "glfwInit() $failed to initialize GLFW");

        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
        glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

        hwnd = glfwCreateWindow(WND_WIDTH, WND_HEIGHT, "SVM", nullptr, nullptr);
        if (hwnd == nullptr)
            throw logger.svm_fatal("C10xx002", "glfwCreateWindow() $failed to create GLFW Window");

        glfwMakeContextCurrent(hwnd);
        glfwSetKeyCallback        (hwnd, OnKeys);
        glfwSetMouseButtonCallback(hwnd, OnMouseButton);
        glfwSetCursorPosCallback  (hwnd, OnMouseMove);

        if (gl3wInit() != 0)
            throw logger.svm_fatal("C10xx003", "gl3wInit() $failed to initialize GL3W");

        if (gl3wIsSupported(4, 5) == false)
            throw logger.svm_fatal("C10xx004", "gl3wIsSupported() $not support OpenGL 4.5");

        // ── Context ──────────────────────────────────────────────────────
        EnsureCalibContext();

        glfwSetWindowUserPointer(hwnd, s_calibCtx.get());
        glfwSwapInterval(2); // 1: 60fps, 2: 30fps
        glfwShowWindow(hwnd);

        printf("Start calibration\n");
        printState(s_calibCtx->getState());

        // ── Render loop ──────────────────────────────────────────────────
        printf("Entering main loop\n");
        while (glfwWindowShouldClose(hwnd) == false)
        {
            glfwPollEvents();
            RenderCalib();
            glfwSwapBuffers(hwnd);
            glFinish();
        }
    }
    catch (runtime_error& e)
    {
        logger.record_message(e.what());
        std::cout << std::endl << e.what() << std::endl;
    }
    catch (...)
    {
        runtime_error unknown = logger.svm_fatal("C10xx006", __FUNCTION__ + string(" $unknown exception occurred"));
        logger.record_message(unknown.what());
        std::cout << std::endl << unknown.what() << std::endl;
    }

    DestroyCalibContext();
    logger.record_message(logger.svm_inform("********", "<<calibration ended>>\n").what());

    glfwDestroyWindow(hwnd);
    glfwTerminate();

    return 0;
}

// ============================================================================
// Input → Context Translation
// ============================================================================

void OnKeys(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;

    if (action != GLFW_PRESS) return;

    sanCalibContext* ctx = static_cast<sanCalibContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    const CalibState* s  = ctx->getState();
    const int         vm = s->viewMode;

    switch (key)
    {
    // ── Quit ─────────────────────────────────────────────────────────────
    case GLFW_KEY_ESCAPE:
        if (s->enterAdjustMode == true)
        {
            ctx->requestAdjustMode(false);
            std::cout << "Cancelled adjustment" << std::endl;
        }
        else
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        break;

    // ── Right arrow ───────────────────────────────────────────────────────
    case GLFW_KEY_RIGHT:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.x += KEY_STEP;
            ctx->setPointPosition(pos);
        }
        else if (vm == (int)FEATURES_VIEW)
        {
            ctx->notifyStepComplete((int)FEATURES_VIEW, s->camIndex);

            const int nextCam = s->camIndex + 1;
            if (nextCam < SVM_CAMERAS_NUM)
                ctx->requestCamIndex(nextCam);
            else
            {
                ctx->requestViewMode(vm + 1);
                ctx->requestCamIndex(0);
            }
        }
        else if (vm < (int)LUTS_VIEW)
        {
            ctx->requestViewMode(vm + 1);
        }
        printState(ctx->getState());
        break;

    // ── Left arrow ────────────────────────────────────────────────────────
    case GLFW_KEY_LEFT:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.x -= KEY_STEP;
            ctx->setPointPosition(pos);
        }
        else if (vm == (int)FEATURES_VIEW)
        {
            const int prevCam = s->camIndex - 1;
            if (prevCam >= 0)
                ctx->requestCamIndex(prevCam);
            else
            {
                ctx->requestViewMode(vm - 1);
                ctx->requestCamIndex(0);
            }
        }
        else if (vm > (int)FISHEYE_VIEW)
        {
            ctx->requestViewMode(vm - 1);
        }
        printState(ctx->getState());
        break;

    // ── Up arrow ──────────────────────────────────────────────────────────
    case GLFW_KEY_UP:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.y -= KEY_STEP;
            ctx->setPointPosition(pos);
        }
        break;

    // ── Down arrow ────────────────────────────────────────────────────────
    case GLFW_KEY_DOWN:
        if (s->enterAdjustMode == true)
        {
            XY pos = s->selectedPtPos;
            pos.y += KEY_STEP;
            ctx->setPointPosition(pos);
        }
        break;

    // ── Enter ─────────────────────────────────────────────────────────────
    case GLFW_KEY_ENTER:
        if (vm == (int)FEATURES_VIEW)
        {
            if (s->enterAdjustMode == false)
            {
                if (ctx->requestAdjustMode(true) == true)
                    std::cout << "Enter adjust mode - cam " << s->camIndex << ", pt 0" << std::endl;
                else
                    std::cout << "Adjust mode denied (step locked?)" << std::endl;
            }
            else
            {
                std::cout << "Confirm pt " << s->selectedPtIdx
                          << " @ (" << s->selectedPtPos.x
                          << ", "   << s->selectedPtPos.y << ")" << std::endl;
                ctx->advanceSelectedPoint();

                const CalibState* s2 = ctx->getState();
                if (s2->enterAdjustMode == true)
                    std::cout << "Next pt: " << s2->selectedPtIdx << std::endl;
                else
                {
                    std::cout << "Exit adjust mode - cam " << s->camIndex << std::endl;
                    ctx->notifyStepComplete((int)FEATURES_VIEW, s->camIndex);
                }
            }
        }
        break;

    // // ── Camera orientation / position (GRIDS_VIEW only) ──────────────────
    // case GLFW_KEY_J: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_ori.y -= CHANGES_ORI; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_L: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_ori.y += CHANGES_ORI; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_I: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_ori.x += CHANGES_ORI; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_K: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_ori.x -= CHANGES_ORI; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_U: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_ori.z -= CHANGES_ORI; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_O: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_ori.z += CHANGES_ORI; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_F: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_pos.x -= CHANGES_POS; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_H: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_pos.x += CHANGES_POS; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_T: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_pos.y += CHANGES_POS; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_G: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_pos.y -= CHANGES_POS; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_R: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_pos.z -= CHANGES_POS; ctx->requestKeyPressed(); } break;
    // case GLFW_KEY_Y: if (vm == (int)GRIDS_VIEW) { ctx->m_pcameras->m_camera_pos.z += CHANGES_POS; ctx->requestKeyPressed(); } break;

    default:
        break;
    }
}

void OnMouseButton(GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;

    sanCalibContext* ctx = static_cast<sanCalibContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    const CalibState* s = ctx->getState();
    if (s->viewMode != (int)FEATURES_VIEW) return;

    double xp = 0.0, yp = 0.0;
    glfwGetCursorPos(window, &xp, &yp);

    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            ctx->m_mousePrepos  = glm::vec2((float)xp, (float)yp);
            ctx->m_mousePressed = 1;

            if (s->enterAdjustMode == true)
                applyMousePosition(ctx, (float)xp, (float)yp);
        }
        else if (action == GLFW_RELEASE)
        {
            if (s->enterAdjustMode == true && ctx->m_mousePressed != 0)
                applyMousePosition(ctx, (float)xp, (float)yp);

            ctx->m_mousePressed = 0;
        }
    }
}

void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    sanCalibContext* ctx = static_cast<sanCalibContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    const CalibState* s = ctx->getState();
    if (s->viewMode != (int)FEATURES_VIEW || s->enterAdjustMode == false) return;
    if (ctx->m_mousePressed == 0) return;

    applyMousePosition(ctx, (float)xpos, (float)ypos);
}
