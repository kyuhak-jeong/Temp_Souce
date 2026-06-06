#include "svmCore.hpp"
#include "svmContext.hpp"
#include "svmShaderString.hpp"
#include "io_platform.h"

// ============================================================================
// Global State
// ============================================================================

volatile sig_atomic_t            g_quit         = 0;
volatile sig_atomic_t            g_cleaned      = 0;

static sanContextUPtr            s_vcontext     = nullptr;
static std::vector<TEXTURE_INFO> s_srcImages;
static GLuint                    s_texObj[SVM_CAMERAS_NUM + ADD_CAMERAS_NUM] = { 0 };
static bool                      s_lcaWarningOn = false;
static int                       s_alertThresh  = 0;

// Mouse/touch gesture state

static constexpr float kMouseScrollZoomScale = 3.0f;

struct GestureState
{
    bool      pointerDown     = false;
    bool      twoFingerActive = false;
    float     prevSpan        = 0.0f;
    glm::vec2 lastPos         = { 0.0f, 0.0f };
};

static GestureState s_gesture;

// ============================================================================
// Context Lifecycle
// ============================================================================

static void DestroyViewContext()
{
    if (s_vcontext == nullptr) return;
    s_vcontext->clearView();
    auto* raw = s_vcontext.release();
    delete raw;
    s_vcontext = nullptr;

    const int totalCams = SVM_CAMERAS_NUM + ADD_CAMERAS_NUM;
    for (int i = 0; i < totalCams; i++)
    {
        if (s_texObj[i] != 0) { glDeleteTextures(1, &s_texObj[i]); s_texObj[i] = 0; }
    }

    s_srcImages.clear();
}

static void EnsureViewContext()
{
    if (s_vcontext != nullptr) return;

    s_srcImages.clear();
    s_srcImages.reserve(SVM_CAMERAS_NUM + ADD_CAMERAS_NUM);

    for (int i = 0; i < SVM_CAMERAS_NUM; i++)
    {
        const std::string path = std::string(_SOURCE_IMAGES_PATH_) + "/src" + std::to_string(i) + ".jpg";
        s_srcImages.push_back(sanFromFile::read_texture_from_file(path));
        printf("Loaded %s\n", path.c_str());
    }
    for (int i = 0; i < ADD_CAMERAS_NUM; i++)
    {
        const std::string path = std::string(_SOURCE_IMAGES_PATH_) + "/add" + std::to_string(i) + ".jpg";
        s_srcImages.push_back(sanFromFile::read_texture_from_file(path));
        printf("Loaded %s\n", path.c_str());
    }

    const int totalCams = SVM_CAMERAS_NUM + ADD_CAMERAS_NUM;
    for (int i = 0; i < totalCams; i++)
    {
        const TEXTURE_INFO& tex = s_srcImages[i];

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &s_texObj[i]);
        glBindTexture(GL_TEXTURE_2D, s_texObj[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, tex.color_format, tex.width, tex.height,
                     0, tex.color_format, GL_UNSIGNED_BYTE, tex.texture.ptr());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    s_vcontext = sanContext::Create(&s_texObj[0], &s_texObj[1], &s_texObj[2], &s_texObj[3], &s_texObj[4]);

    if (s_vcontext == nullptr)
        throw logger.svm_fatal("C20xx005", "Context::Create() $failed to create sanContext");
}

// ============================================================================
// Cleanup
// ============================================================================

static void ProgramCleanup()
{
    if (g_cleaned != 0) { printf("Program already cleaned up.\n"); return; }

    DestroyViewContext();

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
    ProgramCleanup();
    exit(signal);
}

// ============================================================================
// Warning Processing
// ============================================================================

static void ProcessSvmWarning(std::vector<SVM_OBJECT>& frame_objects)
{
    if (frame_objects.empty() == false)
    {
        for (const auto& obj : frame_objects)
        {
            const bool isBsisZone = (obj.primary_zone_type == RIGHT_BSIS_TOP_WARNING_ZONE    ||
                                     obj.primary_zone_type == RIGHT_BSIS_BOT_WARNING_ZONE    ||
                                     obj.primary_zone_type == LEFT_BSIS_TOP_WARNING_ZONE     ||
                                     obj.primary_zone_type == LEFT_BSIS_BOT_WARNING_ZONE     ||
                                     obj.primary_zone_type == RIGHT_BSIS_TOP_MONITORING_ZONE ||
                                     obj.primary_zone_type == RIGHT_BSIS_MID_MONITORING_ZONE ||
                                     obj.primary_zone_type == RIGHT_BSIS_BOT_MONITORING_ZONE ||
                                     obj.primary_zone_type == LEFT_BSIS_TOP_MONITORING_ZONE  ||
                                     obj.primary_zone_type == LEFT_BSIS_MID_MONITORING_ZONE  ||
                                     obj.primary_zone_type == LEFT_BSIS_BOT_MONITORING_ZONE);
            const bool isLcaAlert = (obj.alert_level == ALERT_LCA);

            if (isBsisZone == true && isLcaAlert == true)
            {
                s_lcaWarningOn = true;
                s_alertThresh  = 0;
            }
        }
    }
    else
    {
        if (++s_alertThresh > 30) { s_lcaWarningOn = false; s_alertThresh = 0; }
    }

    if (s_vcontext->m_vehicleSignal.m_vehicle_velocity < 30) s_lcaWarningOn = false;
}

// ============================================================================
// Input → Context Translation
// ============================================================================

static void HandleKeyEvent(const APP::IO::InputEvent& event)
{
    if (event.type != APP::IO::InputEvent::Type::KEY) return;

    const APP::IO::KeyEvent& key = event.key;
    if (key.action != APP::IO::KeyAction::DOWN && key.action != APP::IO::KeyAction::MULTIPLE) return;

    auto& sig = s_vcontext->m_vehicleSignal;
    auto& xml = s_vcontext->m_pxml;

    auto clampSteering = [&](float delta)
    {
        sig.m_steering_angle = max(xml->m_vehicle_spec.min_steering_angle,
                                   min(xml->m_vehicle_spec.max_steering_angle,
                                       sig.m_steering_angle + delta));
        sig.m_wheel_angle = (sig.m_steering_angle >= 0.0f)
                          ? s_vcontext->m_leftAngleMap [sig.m_steering_angle]
                          : s_vcontext->m_rightAngleMap[sig.m_steering_angle];
    };

    switch (key.keyCode)
    {
        case APP::IO::KeyCode::ESCAPE:     g_quit = 1;                                                          break;

        case APP::IO::KeyCode::DPAD_UP:    xml->m_layout[1].view_mode = CAMVIEW3D_FRONT;                        break;
        case APP::IO::KeyCode::DPAD_RIGHT: xml->m_layout[1].view_mode = CAMVIEW3D_RIGHT;                        break;
        case APP::IO::KeyCode::DPAD_DOWN:  xml->m_layout[1].view_mode = CAMVIEW3D_REAR;                         break;
        case APP::IO::KeyCode::DPAD_LEFT:  xml->m_layout[1].view_mode = CAMVIEW3D_LEFT;                         break;
        case APP::IO::KeyCode::W:          xml->m_layout[1].view_mode = CAMVIEW2D_FRONT;                        break;
        case APP::IO::KeyCode::D:          xml->m_layout[1].view_mode = CAMVIEW2D_RIGHT;                        break;
        case APP::IO::KeyCode::S:          xml->m_layout[1].view_mode = CAMVIEW2D_REAR;                         break;
        case APP::IO::KeyCode::A:          xml->m_layout[1].view_mode = CAMVIEW2D_LEFT;                         break;

        case APP::IO::KeyCode::NUM_1:      sig.m_trigger.turn_signal = TURN_SIGNAL_OFF;                         break;
        case APP::IO::KeyCode::NUM_2:      sig.m_trigger.turn_signal = TURN_SIGNAL_LEFT;                        break;
        case APP::IO::KeyCode::NUM_3:      sig.m_trigger.turn_signal = TURN_SIGNAL_RIGHT;                       break;
        case APP::IO::KeyCode::NUM_4:      sig.m_trigger.turn_signal = TURN_SIGNAL_EMERGENCY;                   break;

        case APP::IO::KeyCode::NUM_5:      sig.m_trigger.gear = GEAR_PARKING;                                   break;
        case APP::IO::KeyCode::NUM_6:      sig.m_trigger.gear = GEAR_NEUTRAL;                                   break;
        case APP::IO::KeyCode::NUM_7:      sig.m_trigger.gear = GEAR_DRIVING;                                   break;
        case APP::IO::KeyCode::NUM_8:      sig.m_trigger.gear = GEAR_REVERSE;                                   break;

        case APP::IO::KeyCode::NUM_9:      clampSteering(-25.0f);                                               break;
        case APP::IO::KeyCode::NUM_0:      clampSteering(+25.0f);                                               break;

        case APP::IO::KeyCode::MINUS:      sig.m_vehicle_velocity = max(0.0f,   sig.m_vehicle_velocity - 5.0f); break;
        case APP::IO::KeyCode::EQUAL:      sig.m_vehicle_velocity = min(150.0f, sig.m_vehicle_velocity + 5.0f); break;

        default: break;
    }
}

// Orbit / zoom helpers

static void ApplyOrbit(float dAzimPx, float dElevPx)
{
    CameraInput input;
    input.orbitAzimuth   = dAzimPx;
    input.orbitElevation = dElevPx;
    s_vcontext->m_pscene_animator->pushCameraInput(input);
}

static void ApplyZoom(float spanDeltaPx)
{
    CameraInput input;
    input.zoomDelta = spanDeltaPx;
    s_vcontext->m_pscene_animator->pushCameraInput(input);
}

static float TouchSpan(const APP::IO::MotionEvent& e)
{
    const float dx = e.pointers[1].x - e.pointers[0].x;
    const float dy = e.pointers[1].y - e.pointers[0].y;
    return std::sqrt(dx * dx + dy * dy);
}

static void HandleMotionEvent(const APP::IO::InputEvent& event)
{
    if (event.type != APP::IO::InputEvent::Type::MOTION) return;
    if (s_vcontext == nullptr) return;

    const APP::IO::MotionEvent& me = event.motion;

    // Ensure interactive mode is on so the animator accepts pushCameraInput
    if (s_vcontext->m_pscene_animator->isInteractive() == false)
        s_vcontext->m_pscene_animator->setInteractive(true);

    // Mouse scroll → zoom
    if (me.isMouse() == true && me.action == APP::IO::MotionAction::SCROLL)
    {
        ApplyZoom(-me.scrollY * kMouseScrollZoomScale);
        return;
    }

    // Mouse drag → orbit
    if (me.isMouse() == true)
    {
        if (me.action == APP::IO::MotionAction::DOWN)
        {
            s_gesture.lastPos     = { me.x, me.y };
            s_gesture.pointerDown = true;
            return;
        }
        if (me.action == APP::IO::MotionAction::MOVE && s_gesture.pointerDown == true)
        {
            ApplyOrbit(me.x - s_gesture.lastPos.x, me.y - s_gesture.lastPos.y);
            s_gesture.lastPos = { me.x, me.y };
            return;
        }
        if (me.action == APP::IO::MotionAction::UP ||
            me.action == APP::IO::MotionAction::CANCEL)
        {
            s_gesture.pointerDown = false;
            return;
        }
        return;
    }

    // Touch: two-finger pinch → zoom
    if (me.pointerCount >= 2)
    {
        s_gesture.pointerDown = false;
        const float span = TouchSpan(me);
        if (me.action == APP::IO::MotionAction::POINTER_DOWN ||
            s_gesture.twoFingerActive == false)
        {
            s_gesture.prevSpan        = span;
            s_gesture.twoFingerActive = true;
            return;
        }
        if (me.action == APP::IO::MotionAction::MOVE)
        {
            ApplyZoom(span - s_gesture.prevSpan);
            s_gesture.prevSpan = span;
        }
        if (me.action == APP::IO::MotionAction::POINTER_UP ||
            me.action == APP::IO::MotionAction::UP         ||
            me.action == APP::IO::MotionAction::CANCEL)
        {
            s_gesture.twoFingerActive = false;
        }
        return;
    }

    // Touch: single-finger → orbit
    s_gesture.twoFingerActive = false;
    if (me.action == APP::IO::MotionAction::DOWN)
    {
        s_gesture.lastPos     = { me.x, me.y };
        s_gesture.pointerDown = true;
        return;
    }
    if (me.action == APP::IO::MotionAction::MOVE && s_gesture.pointerDown == true)
    {
        ApplyOrbit(me.x - s_gesture.lastPos.x, me.y - s_gesture.lastPos.y);
        s_gesture.lastPos = { me.x, me.y };
        return;
    }
    if (me.action == APP::IO::MotionAction::UP ||
        me.action == APP::IO::MotionAction::CANCEL)
    {
        s_gesture.pointerDown = false;
    }
}

static void HandleInputEvent(const APP::IO::InputEvent& event)
{
    if (s_vcontext == nullptr) return;
    HandleKeyEvent(event);
    HandleMotionEvent(event);
}

// ============================================================================
// Render
// ============================================================================

static void RenderView()
{
    if (s_vcontext == nullptr) return;

    s_vcontext->m_pmrt->setMRT();
    s_vcontext->clearView();

    glm::mat4 vm0 = s_vcontext->m_pscene_animator->get_view_matrix_of_virual_camera(
                              (VIEWMODE)s_vcontext->m_pxml->m_layout[0].view_mode);
    glm::mat4 vm1 = s_vcontext->m_pscene_animator->get_view_matrix_of_virual_camera(
                              (VIEWMODE)s_vcontext->m_pxml->m_layout[1].view_mode);

    for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
    {
        s_vcontext->m_pbowl->drawLayout0(camID, vm0);
        s_vcontext->m_pbowl->drawLayout1(camID, vm1);
    }

    if (s_vcontext->m_pcamView  != nullptr)  s_vcontext->m_pcamView->drawLayout1();
    if (s_vcontext->m_pvbc      != nullptr) { s_vcontext->m_pvbc->drawLayout0(vm0);  s_vcontext->m_pvbc->drawLayout1(vm1);  }
    if (s_vcontext->m_ppgs      != nullptr) { s_vcontext->m_ppgs->drawLayout0(vm0);  s_vcontext->m_ppgs->drawLayout1(vm1);  }
    if (s_vcontext->m_pdgs      != nullptr) { s_vcontext->m_pdgs->drawLayout0(vm0);  s_vcontext->m_pdgs->drawLayout1(vm1);  }
    if (s_vcontext->m_pmobs     != nullptr) { s_vcontext->m_pmobs->drawLayout0(vm0); s_vcontext->m_pmobs->drawLayout1(vm1); }

    if (s_vcontext->m_pxml->m_activation.od == true && s_vcontext->m_pod != nullptr)
    {
        vector<ORG_OBJECT> od_objs;
        vector<ORG_OBJECT> pure    = s_vcontext->m_pod->m_ptracker->refineObjects(od_objs, 0.90f);
        vector<EXT_OBJECT> frame   = s_vcontext->m_pod->m_ptracker->Run(pure);
        vector<SVM_OBJECT> svm_obj = s_vcontext->m_pod->convertQuadrantCoords(frame);
        vector<SVM_OBJECT> filled  = s_vcontext->m_pod->extractObjectInformation(svm_obj);
        vector<SVM_OBJECT> alert   = s_vcontext->m_pod->applyAlertPolicy(filled);
        vector<SVM_OBJECT> unique  = s_vcontext->m_pod->removeDuplicateObjects(alert,
                                         s_vcontext->m_pxml->m_od_parameter.duplicate_object_iou_threshold);
        ProcessSvmWarning(unique);

        s_vcontext->m_pod->drawLayout0(vm0, unique);
        s_vcontext->m_pod->drawLayout1_3D(vm1, unique);
        s_vcontext->m_pod->drawLayout1_2D(alert);
    }

    if (s_vcontext->m_pmodel_animator != nullptr)
    {
        s_vcontext->m_pmodel_animator->drawLayout0(vm0);
        s_vcontext->m_pmodel_animator->drawLayout1(vm1);
    }

    if (s_vcontext->m_posd != nullptr)
    {
        s_vcontext->m_posd->m_dynamic_render_map.clear();
        s_vcontext->m_posd->update_dynamic_osd();
        s_vcontext->m_posd->m_dynamic_render_map.insert({"text_warning_msg", 1.0f});
        if (s_vcontext->m_vehicleSignal.m_trigger.turn_signal == TURN_SIGNAL_LEFT  && s_lcaWarningOn == true)
            s_vcontext->m_posd->m_dynamic_render_map.insert({"sign_warning_left",  1.0f});
        else if (s_vcontext->m_vehicleSignal.m_trigger.turn_signal == TURN_SIGNAL_RIGHT && s_lcaWarningOn == true)
            s_vcontext->m_posd->m_dynamic_render_map.insert({"sign_warning_right", 1.0f});
        s_vcontext->m_posd->drawLayouts();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, WND_WIDTH, WND_HEIGHT);
    s_vcontext->m_pmrt->UpdateBufferData(nullptr, nullptr);
    s_vcontext->m_pmrt->RenderMRT();
    usleep(33000);
}

// ============================================================================
// Entry Point
// ============================================================================

int main(int /*argc*/, char** /*argv*/)
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

        if (platform.initialize("SVM Viewer", WND_WIDTH, WND_HEIGHT) == false)
            throw logger.svm_fatal("C20xx001", "Platform::initialize() $failed");

        platform.setInputCallback(HandleInputEvent);

        EnsureViewContext();
        printf("Start viewer\n");

        while (g_quit == 0 && platform.shouldClose() == false)
        {
            platform.pollInput();
            platform.beginFrame();
            RenderView();
            platform.endFrame();
        }

        glFinish();
    }
    catch (string msg)
    {
        logger.record_message(msg);
    }

    ProgramCleanup();
    return 0;
}
