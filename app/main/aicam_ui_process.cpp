/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : aicam_ui_process.cpp                                        *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#include "app_vars.h"
#include "io_platform.h"
#include "logger.h"
#include "aicam_ui_process.hpp"

#if defined(USE_FORKLIFT)
    #include "app_forklift.h"
#elif defined(USE_TAXI)
    #include "app_taxi.h"
#endif

/*--------------------------------------------------------------------------*
  GLOBAL VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

AiCamUiProcess          g_AiCamUiProcess;
GMainLoop*              g_pAiCamUiMainLoop    = nullptr;
GMainContext*           g_pAiCamUiMainContext = nullptr;
volatile sig_atomic_t   AICAM_UI_CLEANED      = 0;

/*--------------------------------------------------------------------------*
  STATIC VARIABLES
 *--------------------------------------------------------------------------*/

#if defined(USE_FORKLIFT)
    static APP::AppForklift* s_aiCamApp = nullptr;
#elif defined(USE_TAXI)
    static APP::AppTaxi*     s_aiCamApp = nullptr;
#endif

/*--------------------------------------------------------------------------*
  EXTERN VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

extern const char* Goto_CleanUp;
extern const char* Goto_AiCamUi;

extern void showLoadingScreen();
/*--------------------------------------------------------------------------*
  FUNCTION IMPLEMENTATIONS
 *--------------------------------------------------------------------------*/

void open_AiCam_Ui_Process(void) { g_AiCamUiProcess.Create(); }

void AI_handleTerminalCommand(std::string cmd)
{
    if (s_aiCamApp != nullptr) s_aiCamApp->handleTerminalCommand(cmd);
}

void AI_handleInputEvent(const APP::IO::InputEvent& event)
{
    if (s_aiCamApp == nullptr)                                return;
    if (event.type == APP::IO::InputEvent::Type::KEY)         s_aiCamApp->handleKeyEvent(event.key);
    else if (event.type == APP::IO::InputEvent::Type::MOTION) s_aiCamApp->handleMotionEvent(event.motion);
}

bool AI_isFileViewActive()
{
    #ifdef USE_DVR
        return (s_aiCamApp != nullptr && s_aiCamApp->isFileViewActive());
    #else
        return false;
    #endif
}

/*--------------------------------------------------------------------------*
  Main Loop Callback
 *--------------------------------------------------------------------------*/

static gboolean AiCamMainLoopCallback(gpointer user_data)
{
    if (s_aiCamApp == nullptr || APP::main_canvas == nullptr) return TRUE;
    
    if (s_aiCamApp->shouldExit())
    {
        GMainLoop* loop = static_cast<GMainLoop*>(user_data);
        if (loop != nullptr) g_main_loop_quit(loop);
        return FALSE;
    }
    
    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
    int width = 0, height = 0;
    platform.getDisplaySize(width, height);
    
    platform.beginFrame();
    
    s_aiCamApp->update(1.0f / 25.0f); // 25 fps
    s_aiCamApp->render(width, height);
    
    platform.endFrame();
    
    return TRUE;
}

/*--------------------------------------------------------------------------*
  Thread Function
 *--------------------------------------------------------------------------*/

void* AiCam_Ui_Thread(void* pArg)
{
    LOG_APP_SECTION("AICam UI Thread Starting");
    
    if (APP::app_ready == false) APP::app_ready = true;
    
    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
    
    if (platform.makeGLContextCurrent() == false)
    {
        LOG_APP_ERROR("AICam UI Thread: Failed to make GL context current");
        return nullptr;
    }
    
    // Create AICam app
    #if defined(USE_FORKLIFT)
        s_aiCamApp = new APP::AppForklift();
    #elif defined(USE_TAXI)
        s_aiCamApp = new APP::AppTaxi();
    #endif

    s_aiCamApp->setSharedCanvas(APP::main_canvas);
    
    if (s_aiCamApp->initialize() == false)
    {
        LOG_APP_ERROR("AICam UI Thread: AppAICam::initialize() failed");
        delete s_aiCamApp;
        s_aiCamApp = nullptr;
        AICAM_UI_CLEANED = 1;
        platform.releaseGLContext();
        return nullptr;
    }
    
    LOG_APP_SUCCESS("AppAICam initialized");
    
    // Create main loop
    g_pAiCamUiMainContext = g_main_context_new();
    g_pAiCamUiMainLoop = g_main_loop_new(g_pAiCamUiMainContext, FALSE);
    
    GSource* src = g_timeout_source_new(40); // 25 fps
    g_source_set_callback(src, AiCamMainLoopCallback, g_pAiCamUiMainLoop, nullptr);
    g_source_attach(src, g_pAiCamUiMainContext);
    
    if(g_pAiCamUiMainContext != nullptr)
    {
        g_main_context_unref(g_pAiCamUiMainContext);
        g_pAiCamUiMainContext = nullptr;
    }

    if(src != nullptr)
    {
        g_source_unref(src);
        src = nullptr;
    }
    
    // Run main loop
    LOG_APP_INFO("AiCam UI Thread: Entering main loop");
    g_main_loop_run(g_pAiCamUiMainLoop);
    
    // Show loading screen
    showLoadingScreen();
    
    // Cleanup
    if (g_pAiCamUiMainLoop != nullptr)
    {
        g_main_loop_unref(g_pAiCamUiMainLoop);
        g_pAiCamUiMainLoop = nullptr;
    }
    
    if (s_aiCamApp != nullptr)
    {
        s_aiCamApp->cleanup();
        delete s_aiCamApp;
        s_aiCamApp = nullptr;
    }
    
    APP::main_canvas->resetForAppSwitch();
    
    AICAM_UI_CLEANED = 1;
    
    platform.releaseGLContext();
    
    LOG_APP_SUCCESS("AICam UI Thread Stopped");
    return nullptr;
}

// ============================================================================
// AiCamUiProcess Class Implementation
// ============================================================================

AiCamUiProcess::AiCamUiProcess() : m_CloseTick(NOTUSED), m_CloseWaitSec(10), m_timeCount(0) {}
AiCamUiProcess::~AiCamUiProcess() {}

void AiCamUiProcess::Create(S32 _param1, S32 _param2, const char* _pTitle, SWinMain* _pParent)
{
    create_mainwin((CHAR*)_pTitle, _param1, _param2, _pParent);
}

void AiCamUiProcess::Time_Callback(void)
{
    if (m_timeCount >= 10) { if (system("sync")) {}; m_timeCount = 0; }
    m_timeCount++;
}

void AiCamUiProcess::Swm_Slickey(SMessage& _msg)
{
    LOG_APP_INFOF("AICam ui key=%d", _msg.m_parameter1);
    switch (_msg.m_parameter1)
    {
        case 1:  send_message(SWM_CLOSE, CLEANUP_PROCESS); break;
        case 5:  send_message(SWM_CLOSE, AICAM_UI_PROCESS);break;
        default: send_message(SWM_CLOSE);                  break;
    }
}

void AiCamUiProcess::pre_callback_procedure(SMessage& _msg)
{
    SMainWin::pre_callback_procedure(_msg);

    if (_msg.m_message == SWM_INIT)
    {
        m_CloseWaitSec = 10;
        m_CloseTick    = (m_CloseWaitSec != 0) ? create_tick(this, m_CloseWaitSec) : NOTUSED;
        m_timeCount    = 0;
        AICAM_UI_CLEANED = 0;
        tTaskCreate(&m_Task, AiCam_Ui_Thread, &m_tAttrTask, static_cast<ArgTaskFn>(nullptr), 1);
    }
}

void AiCamUiProcess::callback_procedure(SMessage& _msg)
{
    switch (_msg.m_message)
    {
        case SWM_TICK:    Time_Callback();       break;
        case SWM_KEY:                            break;
        case SWM_SLICKEY: Swm_Slickey(_msg);  break;
        default:                                 break;
    }
}

void AiCamUiProcess::post_callback_procedure(SMessage& _msg)
{
    SMainWin::post_callback_procedure(_msg);

    if (_msg.m_message != SWM_CLOSE) return;

    if (m_CloseTick != NOTUSED) { destroy_tick(m_CloseTick); m_CloseWaitSec = 0; }

    backup_process_id = AICAM_UI_PROCESS;

    if (s_aiCamApp != nullptr) { s_aiCamApp->requestExit(); usleep(60 * 1000); }

    if (APP::app_ready == true) APP::app_ready = false;

    if (g_pAiCamUiMainLoop != nullptr) g_main_loop_quit(g_pAiCamUiMainLoop);

    while (AICAM_UI_CLEANED == 0) { LOG_APP_INFO("AiCam waiting for clean up"); usleep(1000 * 1000); }

    if (system("sync")) {}

    if (PROCESS_ID_END > _msg.m_parameter1) open_process_id((APP_PROCESS_ID)_msg.m_parameter1);
}
