#include "svmContext.hpp"
#include "svmLogger.hpp"
#include "svmShaderString.hpp"
#include "svmWorld.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>

// ============================================================================
// GLFW Callbacks - forward declarations
// ============================================================================

static void OnKeys       (GLFWwindow* window, int key, int scancode, int action, int mods);
static void OnMouseButton(GLFWwindow* window, int button, int action, int mods);
static void OnMouseMove  (GLFWwindow* window, double xpos, double ypos);
static void OnMouseScroll(GLFWwindow* window, double xoffset, double yoffset);

// ============================================================================
// Global State
// ============================================================================

static sanContextUPtr s_vcontext     = nullptr;
static bool           s_lcaWarningOn = false;
static int            s_alertThresh  = 0;

// Mouse gesture state

static constexpr float kMouseScrollZoomScale = 5.0f;

struct GestureState
{
    bool      pointerDown = false;
    glm::vec2 lastPos     = { 0.0f, 0.0f };
};

static GestureState s_gesture;

// ============================================================================
// Context Lifecycle
// ============================================================================

static void DestroyViewContext()
{
    if (s_vcontext == nullptr) return;
    s_vcontext.reset();
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
// OD Test Input (Windows-only: replaces live RKNN inference)
// ============================================================================

static vector<ORG_OBJECT> GenerateInputForOD()
{
    vector<ORG_OBJECT> od_input_list;
    static int         frame_sequence = 0;
    static clock_t     clock_time     = clock();

#if (1) // for testing TTC
    clock_time += 33;

    {
        int x = 1200;
        int y = 200 + 1 * frame_sequence; // approaching
        frame_sequence = ++frame_sequence % 1000;
        od_input_list.push_back(ORG_OBJECT(0, x, y, 200, 100, clock_time));
    }

#elif (0) // for testing MOIS/BSIS
    clock_time += 33;
    od_input_list.push_back(ORG_OBJECT(0,  520, 410,  50,  50, clock_time));
    od_input_list.push_back(ORG_OBJECT(0,  250, 650,  50,  50, clock_time));
    od_input_list.push_back(ORG_OBJECT(0,  700, 400, 100, 100, clock_time));
    od_input_list.push_back(ORG_OBJECT(0,  550, 670,  95, 190, clock_time));
    od_input_list.push_back(ORG_OBJECT(0, 1300, 350,  95, 100, clock_time));
    od_input_list.push_back(ORG_OBJECT(0, 1600, 650,  50,  50, clock_time));
    od_input_list.push_back(ORG_OBJECT(0, 1100, 390,  95, 100, clock_time));
    od_input_list.push_back(ORG_OBJECT(0, 1300, 800,  50,  50, clock_time));
    od_input_list.push_back(ORG_OBJECT(1,  500, 700, 200, 200, clock_time));
    od_input_list.push_back(ORG_OBJECT(1,  700, 600, 100, 100, clock_time));
    od_input_list.push_back(ORG_OBJECT(1,  900, 380, 200, 200, clock_time));
    od_input_list.push_back(ORG_OBJECT(2,  400, 600, 200, 200, clock_time));
    od_input_list.push_back(ORG_OBJECT(2,  860, 910, 200, 100, clock_time));
    od_input_list.push_back(ORG_OBJECT(2, 1200, 600, 200, 200, clock_time));

#elif (0) // for testing duplicate objects
    clock_time += 33;
    od_input_list.push_back(ORG_OBJECT(0, 1520, 762, 50, 50, clock_time));
    od_input_list.push_back(ORG_OBJECT(0,  420, 680, 20, 20, clock_time));
    od_input_list.push_back(ORG_OBJECT(0, 1515, 670, 20, 30, clock_time));
    od_input_list.push_back(ORG_OBJECT(0,  575, 740, 40, 40, clock_time));

#elif (0)
    clock_time += 33;
    od_input_list.push_back(ORG_OBJECT(2, 470, 380, 180, 180, clock_time));
    od_input_list.push_back(ORG_OBJECT(1, 850, 265, 340, 450, clock_time));

#endif

    return od_input_list;
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

    // ── Camera textures ───────────────────────────────────────────────────
    for (GLuint camID = 0; camID < SVM_CAMERAS_NUM; camID++)
    {
        try
        {
            sanVABT::updateTexture(GL_TEXTURE0, *s_vcontext->m_pitxID[camID],
                                   s_vcontext->m_source_images[camID].ptr(),
                                   (int)s_vcontext->m_pxml->m_resolution.image.width,
                                   (int)s_vcontext->m_pxml->m_resolution.image.height,
                                   s_vcontext->m_source_images[camID].channels(), BINDING);
        }
        catch (exception& e)
        {
            const string tag = "(camID[" + to_string(camID) + "])";
            throw logger.svm_fatal("C20xx006",
                string("sanVABT::updateTexture() $cannot update source images as texture") + tag + delimiter(string(e.what())));
        }
        s_vcontext->m_pbowl->drawLayout0(camID, vm0);
        s_vcontext->m_pbowl->drawLayout1(camID, vm1);
    }

    for (GLuint camID = 0; camID < ADD_CAMERAS_NUM; camID++)
    {
        try
        {
            sanVABT::updateTexture(GL_TEXTURE0, *s_vcontext->m_paitxID[camID],
                                   s_vcontext->m_additional_images[camID].ptr(),
                                   (int)s_vcontext->m_pxml->m_resolution.image.width,
                                   (int)s_vcontext->m_pxml->m_resolution.image.height,
                                   s_vcontext->m_additional_images[camID].channels(), BINDING);
        }
        catch (exception& e)
        {
            const string tag = "(additional camID[" + to_string(camID) + "])";
            throw logger.svm_fatal("C20xx006",
                string("sanVABT::updateTexture() $cannot update additional images as texture") + tag + delimiter(string(e.what())));
        }
    }

    // ── Sub-systems ───────────────────────────────────────────────────────
    if (s_vcontext->m_pcamView  != nullptr)  s_vcontext->m_pcamView->drawLayout1();
    if (s_vcontext->m_pvbc      != nullptr) { s_vcontext->m_pvbc->drawLayout0(vm0);  s_vcontext->m_pvbc->drawLayout1(vm1);  }
    if (s_vcontext->m_ppgs      != nullptr) { s_vcontext->m_ppgs->drawLayout0(vm0);  s_vcontext->m_ppgs->drawLayout1(vm1);  }
    if (s_vcontext->m_pdgs      != nullptr) { s_vcontext->m_pdgs->drawLayout0(vm0);  s_vcontext->m_pdgs->drawLayout1(vm1);  }
    if (s_vcontext->m_pmobs     != nullptr) { s_vcontext->m_pmobs->drawLayout0(vm0); s_vcontext->m_pmobs->drawLayout1(vm1); }

    // ── OD ────────────────────────────────────────────────────────────────
    if (s_vcontext->m_pxml->m_activation.od == true && s_vcontext->m_pod != nullptr)
    {
        vector<ORG_OBJECT> od_objs = GenerateInputForOD();

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

    // ── Composite to screen ───────────────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, WND_WIDTH, WND_HEIGHT);
    s_vcontext->m_pmrt->UpdateBufferData(nullptr, nullptr);
    s_vcontext->m_pmrt->RenderMRT();
}

// ============================================================================
// Entry Point
// ============================================================================

int main(int /*argc*/, char** /*argv*/)
{
    GLFWwindow* hwnd       = nullptr;
    string      monitorMsg;

    try
    {
        // ── GLFW / GL init ────────────────────────────────────────────────
        if (glfwInit() == false)
            throw logger.svm_fatal("C20xx001", "glfwInit() $failed to initialize GLFW");

        glfwWindowHint(GLFW_OPENGL_PROFILE,        GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_RESIZABLE,             GL_FALSE);
        glfwWindowHint(GLFW_VISIBLE,               GL_FALSE);

        hwnd = glfwCreateWindow(WND_WIDTH, WND_HEIGHT, "SVM", nullptr, nullptr);
        if (hwnd == nullptr)
            throw logger.svm_fatal("C20xx002", "glfwCreateWindow() $failed to create GLFW Window");

        glfwMakeContextCurrent(hwnd);
        glfwSetKeyCallback        (hwnd, OnKeys);
        glfwSetMouseButtonCallback(hwnd, OnMouseButton);
        glfwSetCursorPosCallback  (hwnd, OnMouseMove);
        glfwSetScrollCallback     (hwnd, OnMouseScroll);

        if (gl3wInit() != 0)
            throw logger.svm_fatal("C20xx003", "gl3wInit() $failed to initialize GL3W");

        if (gl3wIsSupported(4, 5) == false)
            throw logger.svm_fatal("C20xx004", "gl3wIsSupported() $not support OpenGL 4.5");

        // ── Context ───────────────────────────────────────────────────────
        s_vcontext = sanContext::Create(nullptr, nullptr, nullptr, nullptr, nullptr);
        if (s_vcontext == nullptr)
            throw logger.svm_fatal("C20xx005", "Context::Create() $failed to create sanContext");

        glfwSetWindowUserPointer(hwnd, s_vcontext.get());
        glfwSwapInterval(2); // 1: 60fps, 2: 30fps
        glfwShowWindow(hwnd);

        printf("Start viewer\n");

        // ── Render loop ───────────────────────────────────────────────────
        while (glfwWindowShouldClose(hwnd) == false)
        {
            glfwPollEvents();
            RenderView();
            glfwSwapBuffers(hwnd);
            glFinish();
        }
    }
    catch (runtime_error& e)
    {
        logger.record_message(e.what());
        monitorMsg = e.what();
    }
    catch (...)
    {
        runtime_error unknown = logger.svm_fatal("C20xx007", __FUNCTION__ + string(" $unknown exception occurred"));
        logger.record_message(unknown.what());
        monitorMsg = unknown.what();
    }

    std::cout << std::endl << monitorMsg << std::endl;

    DestroyViewContext();

    #if (HAVE_TO_REMOVE_IN_RELEASE_VERSION)
        logger.record_message(logger.svm_inform("********", "<<viewer ended>>\n").what());
    #endif

    glfwDestroyWindow(hwnd);
    glfwTerminate();

    return 0;
}

// ============================================================================
// Input → Context Translation
// ============================================================================

static void OnKeys(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    sanContext* ctx = static_cast<sanContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    auto& sig = ctx->m_vehicleSignal;
    auto& xml = ctx->m_pxml;

    auto clampSteering = [&](float delta)
    {
        sig.m_steering_angle = max(xml->m_vehicle_spec.min_steering_angle,
                                   min(xml->m_vehicle_spec.max_steering_angle,
                                       sig.m_steering_angle + delta));
        sig.m_wheel_angle = (sig.m_steering_angle >= 0.0f)
                          ? ctx->m_leftAngleMap [sig.m_steering_angle]
                          : ctx->m_rightAngleMap[sig.m_steering_angle];
    };

    switch (key)
    {
        // ── Quit ─────────────────────────────────────────────────────────
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, GLFW_TRUE);                                  break;

        // ── View mode ─────────────────────────────────────────────────────
        case GLFW_KEY_UP:    xml->m_layout[1].view_mode = CAMVIEW3D_FRONT;                                  break;
        case GLFW_KEY_RIGHT: xml->m_layout[1].view_mode = CAMVIEW3D_RIGHT;                                  break;
        case GLFW_KEY_DOWN:  xml->m_layout[1].view_mode = CAMVIEW3D_REAR;                                   break;
        case GLFW_KEY_LEFT:  xml->m_layout[1].view_mode = CAMVIEW3D_LEFT;                                   break;
        case GLFW_KEY_W:     xml->m_layout[1].view_mode = CAMVIEW2D_FRONT;                                  break;
        case GLFW_KEY_D:     xml->m_layout[1].view_mode = CAMVIEW2D_RIGHT;                                  break;
        case GLFW_KEY_S:     xml->m_layout[1].view_mode = CAMVIEW2D_REAR;                                   break;
        case GLFW_KEY_A:     xml->m_layout[1].view_mode = CAMVIEW2D_LEFT;                                   break;
        case GLFW_KEY_E:     xml->m_layout[1].view_mode = CAMVIEW2D_ADD0;                                   break;

        // ── Turn signal ───────────────────────────────────────────────────
        case GLFW_KEY_1: sig.m_trigger.turn_signal = TURN_SIGNAL_OFF;                                       break;
        case GLFW_KEY_2: sig.m_trigger.turn_signal = TURN_SIGNAL_LEFT;                                      break;
        case GLFW_KEY_3: sig.m_trigger.turn_signal = TURN_SIGNAL_RIGHT;                                     break;
        case GLFW_KEY_4: sig.m_trigger.turn_signal = TURN_SIGNAL_EMERGENCY;                                 break;

        // ── Gear ─────────────────────────────────────────────────────────
        case GLFW_KEY_5: sig.m_trigger.gear = GEAR_PARKING;                                                 break;
        case GLFW_KEY_6: sig.m_trigger.gear = GEAR_NEUTRAL;                                                 break;
        case GLFW_KEY_7: sig.m_trigger.gear = GEAR_DRIVING;                                                 break;
        case GLFW_KEY_8: sig.m_trigger.gear = GEAR_REVERSE;                                                 break;

        // ── Steering ──────────────────────────────────────────────────────
        case GLFW_KEY_9:     clampSteering(-25.0f);                                                         break;
        case GLFW_KEY_0:     clampSteering(+25.0f);                                                         break;

        // ── Velocity ──────────────────────────────────────────────────────
        case GLFW_KEY_MINUS: sig.m_vehicle_velocity = max(0.0f,   sig.m_vehicle_velocity - 5.0f);           break;
        case GLFW_KEY_EQUAL: sig.m_vehicle_velocity = min(150.0f, sig.m_vehicle_velocity + 5.0f);           break;

        // ── Feature toggles ───────────────────────────────────────────────
        case GLFW_KEY_F1:
            if (action == GLFW_PRESS)
            {
                if (xml->m_activation.scene_animation == true)
                    xml->m_scene_animation_parameter.scene_animation_type =
                        (xml->m_scene_animation_parameter.scene_animation_type + 1) % 3;
                else
                    xml->m_scene_animation_parameter.scene_animation_type = 0;
            }
            break;
        case GLFW_KEY_F2: if (action == GLFW_PRESS) xml->m_activation.pgs                = !xml->m_activation.pgs;                break;
        case GLFW_KEY_F3: if (action == GLFW_PRESS) xml->m_activation.vbc                = !xml->m_activation.vbc;                break;
        case GLFW_KEY_F4: if (action == GLFW_PRESS) xml->m_activation.dgs                = !xml->m_activation.dgs;                break;
        case GLFW_KEY_F5: if (action == GLFW_PRESS) xml->m_activation.od                 = !xml->m_activation.od;                 break;
        case GLFW_KEY_F6: if (action == GLFW_PRESS) xml->m_activation.model_animation    = !xml->m_activation.model_animation;    break;
        case GLFW_KEY_F7: if (action == GLFW_PRESS) xml->m_activation.model_transparency = !xml->m_activation.model_transparency; break;
        case GLFW_KEY_F8: if (action == GLFW_PRESS) xml->m_activation.mobs               = !xml->m_activation.mobs;               break;

        default: break;
    }
}

static void OnMouseButton(GLFWwindow* window, int button, int action, int /*mods*/)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    sanContext* ctx = static_cast<sanContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    if (action == GLFW_PRESS)
    {
        double xp = 0.0, yp = 0.0;
        glfwGetCursorPos(window, &xp, &yp);
        s_gesture.lastPos     = { (float)xp, (float)yp };
        s_gesture.pointerDown = true;

        if (ctx->m_pscene_animator->isInteractive() == false)
            ctx->m_pscene_animator->setInteractive(true);
    }
    else if (action == GLFW_RELEASE)
    {
        s_gesture.pointerDown = false;
    }
}

static void OnMouseMove(GLFWwindow* window, double xpos, double ypos)
{
    if (s_gesture.pointerDown == false) return;

    sanContext* ctx = static_cast<sanContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    const glm::vec2 pos((float)xpos, (float)ypos);
    CameraInput input;
    input.orbitAzimuth   = pos.x - s_gesture.lastPos.x;
    input.orbitElevation = pos.y - s_gesture.lastPos.y;
    ctx->m_pscene_animator->pushCameraInput(input);
    s_gesture.lastPos = pos;
}

static void OnMouseScroll(GLFWwindow* window, double /*xoffset*/, double yoffset)
{
    sanContext* ctx = static_cast<sanContext*>(glfwGetWindowUserPointer(window));
    if (ctx == nullptr) return;

    if (ctx->m_pscene_animator->isInteractive() == false)
        ctx->m_pscene_animator->setInteractive(true);

    CameraInput input;
    input.zoomDelta = -(float)yoffset * kMouseScrollZoomScale;
    ctx->m_pscene_animator->pushCameraInput(input);
}
