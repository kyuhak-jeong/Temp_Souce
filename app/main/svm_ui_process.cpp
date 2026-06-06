/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : svm_ui_process.cpp                                          *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#include "app_vars.h"
#include "io_platform.h"
#include "logger.h"
#include "svm_ui_process.hpp"
#include "app_svm.h"
#include "svmContext.hpp"

/*--------------------------------------------------------------------------
  GLOBAL VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

SvmUiProcess          g_SvmUiProcess;
GMainLoop*            g_pSvmUiMainLoop    = nullptr;
GMainContext*         g_pSvmUiMainContext = nullptr;
volatile sig_atomic_t SVM_UI_CLEANED      = 0;

/*--------------------------------------------------------------------------
  STATIC VARIABLES
 *--------------------------------------------------------------------------*/

static APP::AppSvm*   s_svmApp       = nullptr;
static sanContextUPtr s_vcontext     = nullptr;
static bool           s_lcaWarningOn = false;
static int            s_alertThresh  = 0;

/*--------------------------------------------------------------------------
  EXTERN VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

extern const char* Goto_CleanUp;
extern const char* Goto_SvmUi;
extern const char* Goto_CalibUi;

extern void Write_Webserver(const char* write_buffer);
extern void showLoadingScreen();

/*--------------------------------------------------------------------------
  FUNCTION IMPLEMENTATIONS
 *--------------------------------------------------------------------------*/

void open_Svm_Ui_Process(void) { g_SvmUiProcess.Create(); }

void SVM_handleTerminalCommand(std::string cmd)
{
    if (s_svmApp != nullptr) s_svmApp->handleTerminalCommand(cmd);
}

void SVM_handleInputEvent(const APP::IO::InputEvent& event)
{
    if (s_svmApp == nullptr)                                  return;
    if (event.type == APP::IO::InputEvent::Type::KEY)         s_svmApp->handleKeyEvent(event.key);
    else if (event.type == APP::IO::InputEvent::Type::MOTION) s_svmApp->handleMotionEvent(event.motion);
}

bool SVM_isFileViewActive()
{
    #ifdef USE_DVR
        return (s_svmApp != nullptr && s_svmApp->isFileViewActive());
    #else
        return false;
    #endif
}

void SVM_Cleanup()
{
    if (SVM_UI_CLEANED != 0) { printf("  Viewer already cleaned up.\n"); return; }

    s_vcontext.reset();
    SVM_UI_CLEANED = 1;
    printf("  Viewer cleaned up.\n");
}

/*--------------------------------------------------------------------------
  Warning processing
 *--------------------------------------------------------------------------*/

static void ProcessSvmWarning(std::vector<SVM_OBJECT>& frame_objects)
{
    memset(APP::svm_warning_flag, 0, sizeof(APP::svm_warning_flag));

    if (frame_objects.empty() == false)
    {
        for (const auto& obj : frame_objects)
        {
            const bool isMoisWarn  = (obj.primary_zone_type == MOIS_WARNING_ZONE ||
                                      obj.primary_zone_type == CENTER_REAR_WARNING_ZONE);
            const bool isMoisMon   = (obj.primary_zone_type == MOIS_MONITORING_ZONE);
            const bool isBsisWarn  = (obj.primary_zone_type == RIGHT_BSIS_TOP_WARNING_ZONE  ||
                                      obj.primary_zone_type == RIGHT_BSIS_BOT_WARNING_ZONE  ||
                                      obj.primary_zone_type == LEFT_BSIS_TOP_WARNING_ZONE   ||
                                      obj.primary_zone_type == LEFT_BSIS_BOT_WARNING_ZONE);
            const bool isBsisMon   = (obj.primary_zone_type == RIGHT_BSIS_TOP_MONITORING_ZONE ||
                                      obj.primary_zone_type == RIGHT_BSIS_MID_MONITORING_ZONE ||
                                      obj.primary_zone_type == RIGHT_BSIS_BOT_MONITORING_ZONE ||
                                      obj.primary_zone_type == LEFT_BSIS_TOP_MONITORING_ZONE  ||
                                      obj.primary_zone_type == LEFT_BSIS_MID_MONITORING_ZONE  ||
                                      obj.primary_zone_type == LEFT_BSIS_BOT_MONITORING_ZONE);
            const bool isLevelAlert = (obj.alert_level == ALERT_LEVEL1 || obj.alert_level == ALERT_LEVEL2);
            const bool isLcaAlert   = (obj.alert_level == ALERT_LCA);
            const bool notPerson    = (obj.classID != 1);

            if (isMoisWarn)
            {
                if (isLevelAlert && notPerson)
                    { APP::svm_warning_flag[W_MOBS][W_MOIS_WARNING]++; APP::svm_warning_flag[W_BTO][W_MOIS_WARNING]++; }
                else if (isLcaAlert)
                    APP::svm_warning_flag[W_LCA][W_MOIS_WARNING]++;
            }
            else if (isMoisMon)
            {
                if (isLevelAlert && notPerson) APP::svm_warning_flag[W_MOBS][W_MOIS_MONITORING]++;
                else if (isLcaAlert)           APP::svm_warning_flag[W_LCA][W_MOIS_MONITORING]++;
            }
            else if (isBsisWarn)
            {
                if (isLevelAlert && notPerson) APP::svm_warning_flag[W_MOBS][W_BSIS_WARNING]++;
                else if (isLcaAlert)           APP::svm_warning_flag[W_LCA][W_BSIS_WARNING]++;
            }
            else if (isBsisMon)
            {
                if (isLevelAlert && notPerson) APP::svm_warning_flag[W_MOBS][W_BSIS_MONITORING]++;
                else if (isLcaAlert)           APP::svm_warning_flag[W_LCA][W_BSIS_MONITORING]++;
            }
        }

        APP::svm_warning_flag[W_MOBS][W_RAISED] =
            (APP::svm_warning_flag[W_MOBS][W_MOIS_WARNING]    > 0 ||
             APP::svm_warning_flag[W_MOBS][W_MOIS_MONITORING] > 0 ||
             APP::svm_warning_flag[W_MOBS][W_BSIS_WARNING]    > 0) ? 1 : 0;

        if (APP::svm_warning_flag[W_LCA][W_BSIS_WARNING]    > 0 ||
            APP::svm_warning_flag[W_LCA][W_BSIS_MONITORING] > 0)
        {
            APP::svm_warning_flag[W_LCA][W_RAISED] = 1;
            s_lcaWarningOn = true;
        }
        else
        {
            APP::svm_warning_flag[W_LCA][W_RAISED] = 0;
        }

        APP::svm_warning_flag[W_BTO][W_RAISED] =
            (APP::svm_warning_flag[W_BTO][W_MOIS_WARNING] > 0) ? 1 : 0;
    }
    else
    {
        APP::svm_warning_flag[W_MOBS][W_RAISED] = 0;
        APP::svm_warning_flag[W_LCA][W_RAISED]  = 0;
        APP::svm_warning_flag[W_BTO][W_RAISED]  = 0;
        if (++s_alertThresh > 30) { s_lcaWarningOn = false; s_alertThresh = 0; }
    }

    if (s_vcontext->m_vehicleSignal.m_vehicle_velocity < 30) s_lcaWarningOn = false;
}

static bool isInteractiveViewMode(VIEWMODE mode)
{
    return (mode == CAMVIEW3D_FRONT || mode == CAMVIEW3D_RIGHT ||
            mode == CAMVIEW3D_REAR  || mode == CAMVIEW3D_LEFT  ||
            mode == TOPVIEW3D);
}

/*--------------------------------------------------------------------------
  SVM render (called once per frame from the main loop callback)
 *--------------------------------------------------------------------------*/

static void SvmRenderCallback()
{
    if (s_vcontext == nullptr)
    {
        s_vcontext = sanContext::Create(&APP::cam_tex_ids[0], &APP::cam_tex_ids[1],
                                        &APP::cam_tex_ids[2], &APP::cam_tex_ids[3],
                                        &APP::cam_tex_ids[4]);
        if (s_vcontext == nullptr)
            throw logger.svm_fatal("C20xx005", "Context::Create() $failed to create sanContext");

        if (s_svmApp != nullptr) s_svmApp->setSvmAnimator(s_vcontext->getAnimator());
    }

    clock_t clock_time = clock();

    // ── Sync menu settings into context ───────────────────────────────────
    M_MENU_SVM& menu        = APP::appConf.menu_svm;
    bool        xml_updated = false;

    s_vcontext->m_pxml->m_activation.vbc  = menu.activation[0];
    s_vcontext->m_pxml->m_activation.pgs  = menu.activation[1];
    s_vcontext->m_pxml->m_activation.dgs  = menu.activation[2];
    s_vcontext->m_pxml->m_activation.od   = menu.activation[3];
    s_vcontext->m_pxml->m_activation.mobs = menu.activation[4];

    s_vcontext->m_pxml->m_view_setting.default_view =
        (menu.view_setting[0] == 0) ? (int)VIEWMODE::CAMVIEW2D_FRONT : (int)VIEWMODE::CAMVIEW3D_FRONT;
    s_vcontext->m_pxml->m_view_setting.right_turn =
        (menu.view_setting[1] == 0) ? (int)VIEWMODE::CAMVIEW2D_RIGHT : (int)VIEWMODE::CAMVIEW3D_RIGHT;
    s_vcontext->m_pxml->m_view_setting.rear_gear =
        (menu.view_setting[2] == 0) ? (int)VIEWMODE::CAMVIEW2D_REAR  : (int)VIEWMODE::CAMVIEW3D_REAR;
    s_vcontext->m_pxml->m_view_setting.left_turn =
        (menu.view_setting[3] == 0) ? (int)VIEWMODE::CAMVIEW2D_LEFT  : (int)VIEWMODE::CAMVIEW3D_LEFT;
    s_vcontext->m_pxml->m_view_setting.emergency =
        (menu.view_setting[4] == 0) ? (int)VIEWMODE::CAMVIEW3D_FRONT : (int)VIEWMODE::CAMVIEW3D_REAR;

    // Compute temp view for menu preview
    const auto& vs     = s_vcontext->m_pxml->m_view_setting;
    int temp_view      = (menu.selected_viewIdx == 0) ? vs.default_view
                       : (menu.selected_viewIdx == 1) ? vs.right_turn
                       : (menu.selected_viewIdx == 2) ? vs.rear_gear
                       : (menu.selected_viewIdx == 3) ? vs.left_turn
                       : vs.emergency;

    if (s_vcontext->m_pscene_animator->isInteractive() == false)
    {
        for (int i = 0; i < 4; ++i)
        {
            s_vcontext->m_pxml->m_vcam[i].pos.x = menu.vcam[i][0];
            s_vcontext->m_pxml->m_vcam[i].pos.y = menu.vcam[i][1];
            s_vcontext->m_pxml->m_vcam[i].pos.z = menu.vcam[i][2];
            s_vcontext->m_pxml->m_vcam[i].ori.x = menu.vcam[i][3];
            s_vcontext->m_pxml->m_vcam[i].ori.y = menu.vcam[i][4];
            s_vcontext->m_pxml->m_vcam[i].ori.z = menu.vcam[i][5];
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        s_vcontext->m_pxml->m_rcam[i].camview_offset.hleft  = (int)menu.rcam[i][0];
        s_vcontext->m_pxml->m_rcam[i].camview_offset.hright = (int)menu.rcam[i][1];
        s_vcontext->m_pxml->m_rcam[i].camview_offset.vtop   = (int)menu.rcam[i][2];
        s_vcontext->m_pxml->m_rcam[i].camview_offset.vbot   = (int)menu.rcam[i][3];
    }
    xml_updated = true;

    // ── CAN data ──────────────────────────────────────────────────────────
    #ifdef USE_CAN
        const OBDData obd = APP::g_obd.read();
        s_vcontext->m_vehicleSignal.m_trigger.gear        = (GEAR) obd.gearPos;
        s_vcontext->m_vehicleSignal.m_trigger.turn_signal = (TURN_SIGNAL) obd.turnSignal;
        s_vcontext->m_vehicleSignal.m_vehicle_velocity    = std::max(0.0f, std::min(150.0f, (float) obd.speedKmh));
        s_vcontext->m_vehicleSignal.m_steering_angle      = std::max(-550.0f, std::min(550.0f, (float) obd.steeringAngle));
        s_vcontext->m_vehicleSignal.m_wheel_angle = (s_vcontext->m_vehicleSignal.m_steering_angle >= 0.0f)
            ? s_vcontext->m_leftAngleMap[s_vcontext->m_vehicleSignal.m_steering_angle]
            : s_vcontext->m_rightAngleMap[s_vcontext->m_vehicleSignal.m_steering_angle];
        s_vcontext->updateVehicleStateFromSignal();
    #endif

    // ── View layout ───────────────────────────────────────────────────────
    s_vcontext->updateViewLayout();
    if (menu.visible == true) s_vcontext->m_pxml->m_layout[1].view_mode = (VIEWMODE)temp_view;

    // ── Interactive camera bridge ─────────────────────────────────────────
    if (s_svmApp != nullptr)
    {
        const LAYOUT&  lay1       = s_vcontext->m_pxml->m_layout[1];
        const VIEWMODE activeMode = (VIEWMODE)lay1.view_mode;
        const bool     adjActive  = s_svmApp->isV3dAdjustmentActive();
        const bool     wantInteract = (isInteractiveViewMode(activeMode) && menu.visible == false) || adjActive;

        s_svmApp->setLayout1Rect(lay1.x, lay1.y, lay1.width, lay1.height);

        if (s_vcontext->m_pscene_animator->isInteractive() != wantInteract)
        {
            s_vcontext->m_pscene_animator->setInteractive(wantInteract);
            if (adjActive == false) s_svmApp->setInteractiveMode(wantInteract);
        }
        if (wantInteract == true && adjActive == false)
        {
            s_vcontext->m_pscene_animator->setActiveViewMode(activeMode);
            s_vcontext->m_pscene_animator->pushCameraInput(s_svmApp->takeCameraInput());
        }
    }

    // ── Render to MRT ─────────────────────────────────────────────────────
    if (s_vcontext->m_pmrt != nullptr) s_vcontext->m_pmrt->setMRT();

    s_vcontext->clearView();

    glm::mat4 vm0 = s_vcontext->m_pscene_animator->get_view_matrix_of_virual_camera(
                        (VIEWMODE)s_vcontext->m_pxml->m_layout[0].view_mode);
    glm::mat4 vm1 = s_vcontext->m_pscene_animator->get_view_matrix_of_virual_camera(
                        (VIEWMODE)s_vcontext->m_pxml->m_layout[1].view_mode);

    for (int cam = 0; cam < SVM_CAMERAS_NUM; ++cam)
    {
        if (s_vcontext->m_pbowl == nullptr) break;
        s_vcontext->m_pbowl->drawLayout0(cam, vm0);
        s_vcontext->m_pbowl->drawLayout1(cam, vm1);
    }

    // ── OD (Object Detection) ─────────────────────────────────────────────
    std::vector<ORG_OBJECT> od_objs;
    if (s_vcontext->m_pxml->m_activation.od == true)
    {
        for (int camIdx = 0; camIdx < SVM_CAMERAS_NUM; ++camIdx)
        {
            // printf("detected %ld in cam %d\n", APP::detected_objs[camIdx].size(), camIdx);

            for (const auto& bb : APP::detected_objs[camIdx])
            {
                if (bb.classId > 5) continue; // person, bicycle, motorcycle, car, bus, truck
                od_objs.emplace_back(camIdx, bb.classId,
                    static_cast<int>(bb.x      * IMG_WIDTH),
                    static_cast<int>(bb.y      * IMG_HEIGHT),
                    static_cast<int>(bb.width  * IMG_WIDTH),
                    static_cast<int>(bb.height * IMG_HEIGHT),
                    clock_time);
            }
        }
    }

    if (s_vcontext->m_pcamView != nullptr)  s_vcontext->m_pcamView->drawLayout1(xml_updated);
    if (s_vcontext->m_pvbc     != nullptr) { s_vcontext->m_pvbc->drawLayout0(vm0);  s_vcontext->m_pvbc->drawLayout1(vm1); }
    if (s_vcontext->m_ppgs     != nullptr) { s_vcontext->m_ppgs->drawLayout0(vm0);  s_vcontext->m_ppgs->drawLayout1(vm1); }
    if (s_vcontext->m_pdgs     != nullptr) { s_vcontext->m_pdgs->drawLayout0(vm0);  s_vcontext->m_pdgs->drawLayout1(vm1); }
    if (s_vcontext->m_pmobs    != nullptr) { s_vcontext->m_pmobs->drawLayout0(vm0); s_vcontext->m_pmobs->drawLayout1(vm1); }

    if (s_vcontext->m_pxml->m_activation.od == true && s_vcontext->m_pod != nullptr)
    {
        vector<ORG_OBJECT> pure    = s_vcontext->m_pod->m_ptracker->refineObjects(od_objs, 0.90f);
        vector<EXT_OBJECT> frame   = s_vcontext->m_pod->m_ptracker->Run(pure);
        vector<SVM_OBJECT> svm_obj = s_vcontext->m_pod->convertQuadrantCoords(frame);
        vector<SVM_OBJECT> filled  = s_vcontext->m_pod->extractObjectInformation(svm_obj);
        vector<SVM_OBJECT> alert   = s_vcontext->m_pod->applyAlertPolicy(filled);
        vector<SVM_OBJECT> unique  = s_vcontext->m_pod->removeDuplicateObjects(alert,
                                         s_vcontext->m_pxml->m_od_parameter.duplicate_object_iou_threshold);

        // printf("org %ld, pure %ld, frame %ld, svm_obj %ld, filled %ld, alert %ld, unique %ld\n", 
        //         od_objs.size(), pure.size(), frame.size(), svm_obj.size(), filled.size(), alert.size(), unique.size());

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

    if (s_vcontext->m_ptextRenderer != nullptr)
    {
        glViewport(0, 0, WND_WIDTH, WND_HEIGHT);
        glm::mat4 mvp_cv = glm::ortho(0.0f, (float)WND_WIDTH, (float)WND_HEIGHT, 0.0f);

        const std::string txtVel  = to_mystring(s_vcontext->m_vehicleSignal.m_vehicle_velocity, 0, 3, '\0');
        const glm::vec3   velSize = s_vcontext->m_ptextRenderer->calculateTextSize(txtVel, 1.0f);
        const glm::vec3   botLeft(30.0f, 1050.0f, 0.0f);
        const glm::vec3   velPos  = botLeft - glm::vec3(0.0f, velSize.y, 0.0f);

        constexpr float   unitScale = 0.5f;
        const std::string txtUnit   = "Km/h";
        const glm::vec3   unitSize  = s_vcontext->m_ptextRenderer->calculateTextSize(txtUnit, unitScale);
        const glm::vec3   unitPos(botLeft.x + velSize.x + 10.0f, botLeft.y - unitSize.y, 0.0f);

        s_vcontext->m_ptextRenderer->renderText(txtVel,  velPos,  TEXT_RENDER_MODE::TEXT_CV, mvp_cv);
        s_vcontext->m_ptextRenderer->renderText(txtUnit, unitPos, TEXT_RENDER_MODE::TEXT_CV, mvp_cv, unitScale);
    }

    // ── Composite MRT result into SVM framebuffer ─────────────────────────
    if (s_svmApp != nullptr && s_vcontext->m_pmrt != nullptr)
    {
        s_svmApp->bindSvmFb();
        s_vcontext->m_pmrt->UpdateBufferData(nullptr, nullptr);
        s_vcontext->m_pmrt->RenderMRT();
        s_svmApp->unbindSvmFb();
    }
}

/*--------------------------------------------------------------------------
  Main Loop Callback
 *--------------------------------------------------------------------------*/

static gboolean SvmMainLoopCallback(gpointer user_data)
{
    if (s_svmApp == nullptr || APP::main_canvas == nullptr) return TRUE;

    if (s_svmApp->shouldExit() == true)
    {
        GMainLoop* loop = static_cast<GMainLoop*>(user_data);
        if (loop != nullptr) g_main_loop_quit(loop);
        return FALSE;
    }

    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
    int width = 0, height = 0;
    platform.getDisplaySize(width, height);

    platform.beginFrame();

    SvmRenderCallback();

    s_svmApp->pushSvmFbTexture();
    s_svmApp->update(1.0f / 25.0f);
    s_svmApp->render(width, height);

    platform.endFrame();

    return TRUE;
}

/*--------------------------------------------------------------------------
  Thread Function
 *--------------------------------------------------------------------------*/

static void* Svm_Ui_Thread(void* /*pArg*/)
{
    LOG_APP_SECTION("SVM UI Thread Starting");

    if (APP::app_ready == false) APP::app_ready = true;

    APP::IO::Platform& platform = APP::IO::Platform::getInstance();

    if (platform.makeGLContextCurrent() == false)
    {
        LOG_APP_ERROR("SVM UI Thread: Failed to make GL context current");
        return nullptr;
    }

    if (s_svmApp == nullptr) s_svmApp = new APP::AppSvm();
    s_svmApp->setSharedCanvas(APP::main_canvas);

    if (s_svmApp->initialize() == false)
    {
        LOG_APP_ERROR("SVM UI Thread: AppSvm::initialize() failed");
        delete s_svmApp;
        s_svmApp       = nullptr;
        SVM_UI_CLEANED = 1;
        platform.releaseGLContext();
        return nullptr;
    }

    LOG_APP_SUCCESS("AppSvm initialized");

    g_pSvmUiMainContext = g_main_context_new();
    g_pSvmUiMainLoop    = g_main_loop_new(g_pSvmUiMainContext, FALSE);

    GSource* src = g_timeout_source_new(40);   // 25 fps
    g_source_set_callback(src, SvmMainLoopCallback, g_pSvmUiMainLoop, nullptr);
    g_source_attach(src, g_pSvmUiMainContext);
    g_main_context_unref(g_pSvmUiMainContext);
    g_pSvmUiMainContext = nullptr;
    g_source_unref(src);
    src = nullptr;

    LOG_APP_INFO("SVM UI Thread: Entering main loop");
    g_main_loop_run(g_pSvmUiMainLoop);

    showLoadingScreen();

    if (g_pSvmUiMainLoop != nullptr) { g_main_loop_unref(g_pSvmUiMainLoop); g_pSvmUiMainLoop = nullptr; }

    if (s_svmApp != nullptr) { s_svmApp->cleanup(); delete s_svmApp; s_svmApp = nullptr; }

    SVM_Cleanup();

    APP::main_canvas->resetForAppSwitch();
    platform.releaseGLContext();

    LOG_APP_SUCCESS("SVM UI Thread Stopped");
    return nullptr;
}

// ============================================================================
// SvmUiProcess Class Implementation
// ============================================================================

SvmUiProcess::SvmUiProcess() : m_CloseTick(NOTUSED), m_CloseWaitSec(10), m_timeCount(0) {}
SvmUiProcess::~SvmUiProcess() {}

void SvmUiProcess::Create(S32 _param1, S32 _param2, const char* _pTitle, SWinMain* _pParent)
{
    create_mainwin((CHAR*)_pTitle, _param1, _param2, _pParent);
}

void SvmUiProcess::Time_Callback()
{
    if (m_timeCount >= 10) { if (system("sync")) {}; m_timeCount = 0; }
    m_timeCount++;
}

void SvmUiProcess::Swm_Slickey(SMessage& _msg)
{
    LOG_APP_INFOF("SVM UI slickey=%d", _msg.m_parameter1);
    switch (_msg.m_parameter1)
    {
        case 1:  send_message(SWM_CLOSE, CLEANUP_PROCESS);  break;
        case 3:  send_message(SWM_CLOSE, SVM_UI_PROCESS);   break;
        case 4:  send_message(SWM_CLOSE, CALIB_UI_PROCESS); break;
        default: send_message(SWM_CLOSE);                   break;
    }
}

void SvmUiProcess::pre_callback_procedure(SMessage& _msg)
{
    SMainWin::pre_callback_procedure(_msg);

    if (_msg.m_message == SWM_INIT)
    {
        m_CloseWaitSec = 10;
        m_CloseTick    = (m_CloseWaitSec != 0) ? create_tick(this, m_CloseWaitSec) : NOTUSED;
        m_timeCount    = 0;
        SVM_UI_CLEANED = 0;
        tTaskCreate(&m_Task, Svm_Ui_Thread, &m_tAttrTask, static_cast<ArgTaskFn>(nullptr), 1);
    }
}

void SvmUiProcess::callback_procedure(SMessage& _msg)
{
    switch (_msg.m_message)
    {
        case SWM_TICK:    Time_Callback();   break;
        case SWM_KEY:                        break;
        case SWM_SLICKEY: Swm_Slickey(_msg); break;
        default:                             break;
    }
}

void SvmUiProcess::post_callback_procedure(SMessage& _msg)
{
    SMainWin::post_callback_procedure(_msg);

    if (_msg.m_message != SWM_CLOSE) return;

    if (m_CloseTick != NOTUSED) { destroy_tick(m_CloseTick); m_CloseWaitSec = 0; }

    backup_process_id = SVM_UI_PROCESS;

    if (s_svmApp != nullptr) { s_svmApp->requestExit(); usleep(60 * 1000); }

    if (APP::app_ready == true) APP::app_ready = false;

    if (g_pSvmUiMainLoop != nullptr) g_main_loop_quit(g_pSvmUiMainLoop);

    while (SVM_UI_CLEANED == 0) { LOG_APP_INFO("SVM waiting for clean up"); usleep(1000 * 1000); }

    if (system("sync")) {}

    if (PROCESS_ID_END > _msg.m_parameter1) open_process_id((APP_PROCESS_ID)_msg.m_parameter1);
}
