/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : calib_ui_process.cpp                                        *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#include "app_vars.h"
#include "io_platform.h"
#include "logger.h"
#include "calib_ui_process.hpp"
#include "app_calib.h"
#include "svmCalibContext.hpp"

/*--------------------------------------------------------------------------*
  GLOBAL VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

CalibUiProcess        g_CalibUiProcess;
GMainLoop*            g_pCalibUiMainLoop    = nullptr;
GMainContext*         g_pCalibUiMainContext = nullptr;
volatile sig_atomic_t CALIB_UI_CLEANED      = 0;

/*--------------------------------------------------------------------------*
  STATIC VARIABLES
 *--------------------------------------------------------------------------*/

static APP::AppCalib*      s_calibApp    = nullptr;
static GstElement*         s_gstPipeline = nullptr;
static bool                s_gstRunning  = false;
static sanCalibContextUPtr s_calibCtx    = nullptr;
static bool                s_exitSent    = false;

/*--------------------------------------------------------------------------*
  EXTERN VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

extern const char* Goto_CleanUp;
extern const char* Goto_SvmUi;
extern const char* Goto_CalibUi;

extern void Write_Webserver(const char* write_buffer);
extern void showLoadingScreen();
/*--------------------------------------------------------------------------*
  FUNCTION IMPLEMENTATIONS
 *--------------------------------------------------------------------------*/

void open_Calib_Ui_Process(void) { g_CalibUiProcess.Create(); }

void CALIB_handleTerminalCommand(std::string cmd)
{
    if (s_calibApp != nullptr) s_calibApp->handleTerminalCommand(cmd);
}

void CALIB_handleInputEvent(const APP::IO::InputEvent& event)
{
    if (s_calibApp == nullptr)                                return;
    if (event.type == APP::IO::InputEvent::Type::KEY)         s_calibApp->handleKeyEvent(event.key);
    else if (event.type == APP::IO::InputEvent::Type::MOTION) s_calibApp->handleMotionEvent(event.motion);
}

/*--------------------------------------------------------------------------*
  GStreamer stream pipeline helpers
 *--------------------------------------------------------------------------*/

static std::mutex s_streamMutex;

static gboolean OnNewSample(GstElement* sink, gpointer /*user_data*/)
{
    std::lock_guard<std::mutex> lock(s_streamMutex);
    if (s_calibApp == nullptr || s_gstRunning == false) return GST_FLOW_OK;

    GstSample* sample = nullptr;
    g_signal_emit_by_name(sink, "pull-sample", &sample, nullptr);
    if (sample == nullptr) return GST_FLOW_ERROR;

    GstBuffer* buf = gst_sample_get_buffer(sample);
    GstMemory* mem = (buf != nullptr) ? gst_buffer_get_memory(buf, 0) : nullptr;

    if (mem != nullptr && gst_is_gl_memory(mem) == TRUE)
    {
        s_calibApp->setCapturePanelTexture(((GstGLMemory*)mem)->tex_id);
        gst_memory_unref(mem);
    }
    else if (mem != nullptr)
    {
        gst_memory_unref(mem);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static void StopGstPipeline()
{
    if (s_gstPipeline == nullptr) return;
    gst_element_set_state(s_gstPipeline, GST_STATE_NULL);
    gst_element_get_state(s_gstPipeline, nullptr, nullptr, GST_SECOND * 3);
    gst_object_unref(s_gstPipeline);
    s_gstPipeline = nullptr;
    s_gstRunning  = false;
    if (s_calibApp != nullptr) s_calibApp->setStreamPipelineRunning(false);
    LOG_APP_INFO("CalibUiProcess: Stream pipeline stopped");
}

static void StartGstPipeline(const std::string& pipelineStr)
{
    StopGstPipeline();
    if (pipelineStr.empty() == true) return;

    GError* err   = nullptr;
    s_gstPipeline = gst_parse_launch(pipelineStr.c_str(), &err);
    if (err != nullptr)
    {
        LOG_APP_ERRORF("CalibUiProcess: GStreamer parse error: %s", err->message);
        g_error_free(err);
        s_gstPipeline = nullptr;
        return;
    }

    GstElement* sink = gst_bin_get_by_name(GST_BIN(s_gstPipeline), "streamsink");
    if (sink != nullptr)
    {
        g_object_set(G_OBJECT(sink), "drop", TRUE, "max-buffers", 1, nullptr);
        g_object_set(G_OBJECT(sink), "sync", FALSE, nullptr);
        g_object_set(G_OBJECT(sink), "emit-signals", TRUE, nullptr);
        g_signal_connect(sink, "new-sample", G_CALLBACK(OnNewSample), nullptr);
        gst_object_unref(sink);
    }

    // Propagate the shared GL context so glvideomixer can use it
    GstContext* glCtx = APP::IO::Platform::getInstance().getGstContext();
    if (glCtx != nullptr) gst_element_set_context(GST_ELEMENT(s_gstPipeline), glCtx);

    gst_element_set_state(s_gstPipeline, GST_STATE_PLAYING);
    s_gstRunning = true;
    if (s_calibApp != nullptr) s_calibApp->setStreamPipelineRunning(true);
    LOG_APP_INFO("CalibUiProcess: Stream pipeline started");
}

/*--------------------------------------------------------------------------*
  Capture thread — runs one v4l2src pipeline per camera, saves src<N>.jpg
 *--------------------------------------------------------------------------*/

static int s_captureThreadIndices[4] = {0, 1, 2, 3};

static gpointer CaptureThreadFunc(gpointer data)
{
    int  idx = *static_cast<int*>(data);
    char pipeStr[512];
    snprintf(pipeStr, sizeof(pipeStr),
        "v4l2src device=/dev/video%d num-buffers=5 "
        "! video/x-raw,width=1920,height=1080,framerate=30/1 "
        "! jpegenc ! multifilesink location=%s/src%d.jpg",
        idx, _SOURCE_IMAGES_PATH_.c_str(), idx);

    GstElement* pipeline = gst_parse_launch(pipeStr, nullptr);
    if (pipeline == nullptr)
    {
        LOG_APP_ERRORF("CalibUiProcess: Capture pipeline %d parse error", idx);
        return nullptr;
    }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    sleep(3);
    sync();
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_element_get_state(pipeline, nullptr, nullptr, GST_SECOND * 3);
    gst_object_unref(pipeline);
    LOG_APP_INFOF("CalibUiProcess: Camera %d captured", idx);
    return nullptr;
}

static void RunCaptureThreads()
{
    GThread* threads[4] = {nullptr, nullptr, nullptr, nullptr};
    for (int i = 0; i < 4; i++) threads[i] = g_thread_new(nullptr, CaptureThreadFunc, &s_captureThreadIndices[i]);
    for (int i = 0; i < 4; i++) if (threads[i] != nullptr) g_thread_join(threads[i]);
    if (system("sync")) {}
    LOG_APP_INFO("CalibUiProcess: All cameras captured");
}

/*--------------------------------------------------------------------------*
  AutoCalib context helpers
 *--------------------------------------------------------------------------*/

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
        LOG_APP_INFOF("CalibUiProcess: Loaded %s", path.c_str());
    }
    for (int i = 0; i < ADD_CAMERAS_NUM; i++)
    {
        const std::string path = std::string(_SOURCE_IMAGES_PATH_) + "/add" + std::to_string(i) + ".jpg";
        srcImages.push_back(sanFromFile::read_image(path, IMREAD_COLOR));
        LOG_APP_INFOF("CalibUiProcess: Loaded %s", path.c_str());
    }

    s_calibCtx = sanCalibContext::Create(
        srcImages[0].ptr(), srcImages[1].ptr(),
        srcImages[2].ptr(), srcImages[3].ptr(),
        srcImages[4].ptr());

    if (s_calibCtx == nullptr)
        throw std::runtime_error("CalibUiProcess: sanCalibContext::Create() failed");
}

/*--------------------------------------------------------------------------*
  Menu → context intent translation
  Reads the currently selected auto-calib row + camera tab from the menu,
  then issues the appropriate request*() calls on the context.
  This is the only place view mode / cam index / adjust mode are written.
 *--------------------------------------------------------------------------*/

static void SyncMenuIntentToContext()
{
    if (s_calibApp == nullptr || s_calibCtx == nullptr) return;

    const int pendingCam = s_calibApp->getPendingAdjCam();
    if (pendingCam >= 0)
    {
        s_calibApp->clearPendingAdjCam();
        s_calibCtx->requestViewMode(FEATURES_VIEW);
        s_calibCtx->requestCamIndex(pendingCam);
        s_calibApp->startAdjustmentForCam(pendingCam);
    }

    const int desiredMode = s_calibApp->getSelectedCalibViewMode();
    const int desiredCam  = s_calibApp->getSelectedCalibCamIdx();

    s_calibCtx->requestViewMode(desiredMode);

    if (desiredMode == FEATURES_VIEW)
    {
        s_calibCtx->requestCamIndex(desiredCam);
        s_calibCtx->requestAdjustMode(s_calibApp->isCalibAdjMode());
    }
    else
    {
        s_calibCtx->requestAdjustMode(false);
    }
}

/*--------------------------------------------------------------------------*
  AutoCalib render — called each frame when auto_calib sub-menu is active
 *--------------------------------------------------------------------------*/

static void RenderAutoCalib()
{
    if (s_calibApp == nullptr) return;

    // Context refresh requested (vehicle change or new capture)
    if (s_calibApp->needsContextRefresh() == true)
    {
        LOG_APP_INFO("CalibUiProcess: Reloading calib context");
        DestroyCalibContext();
        s_calibApp->clearContextRefresh();
    }

    EnsureCalibContext();

    SyncMenuIntentToContext();

    const CalibState* state = s_calibCtx->getState();
    if (state->enterAdjustMode == true)
    {
        const APP::UI::Vec2 pos = s_calibApp->getPendingPointPos();
        if (pos.x >= 0.f)
        {
            s_calibCtx->setPointPosition(XY{pos.x, pos.y});
            s_calibApp->clearPendingPointPos();
        }

        if (s_calibApp->isPointConfirmed() == true)
        {
            s_calibCtx->advanceSelectedPoint();
            s_calibApp->clearPointConfirmed();
        }
    }

    s_calibCtx->getMRT()->setMRT();
    s_calibCtx->clearView();
    s_calibCtx->run();

    state = s_calibCtx->getState();
    const int vm  = state->viewMode;
    const int cam = state->camIndex;
    s_calibCtx->notifyStepComplete(vm, (vm == FEATURES_VIEW) ? cam : -1);

    if (state->stepDenied == true)
        s_calibCtx->clearStepDenied();

    s_calibApp->syncCalibState(*s_calibCtx->getState());

    s_calibApp->bindCalibFb();

    sanMRT* mrt = s_calibCtx->getMRT();
    if (mrt != nullptr)
    {
        glViewport(0, 0, (GLsizei)WND_WIDTH, (GLsizei)WND_HEIGHT);
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

    s_calibApp->unbindCalibFb();
}

/*--------------------------------------------------------------------------*
  Calib render callback — called once per frame from the main loop
 *--------------------------------------------------------------------------*/

static void CalibRenderCallback()
{
    if (s_calibApp == nullptr) return;

    // Stream pipeline management
    if (s_calibApp->isStreamPipelineUpdated() == true)
    {
        const std::string& pipe = s_calibApp->getStreamPipeline();
        if (pipe.empty() == true) StopGstPipeline();
        else                      StartGstPipeline(pipe);
        s_calibApp->markStreamPipelineApplied();
    }

    // Capture request
    if (APP::capture_requested == true && s_gstRunning == false)
    {
        RunCaptureThreads();
        APP::capture_requested = false;
        APP::capture_taken     = true;
        s_calibApp->setCaptureContextRefresh();
    }

    if (s_calibApp->getSelectedCalibViewMode() >= 0)
    {
        try
        {
            RenderAutoCalib();
        }
        catch (const std::exception& e)
        {
            LOG_APP_ERRORF("CalibUiProcess: AutoCalib render error: %s", e.what());
            DestroyCalibContext();
        }
    }
    else
    {
        // Destroy context when leaving AutoCalib view to free GPU/CPU memory
        if (s_calibCtx != nullptr) DestroyCalibContext();
    }
}

/*--------------------------------------------------------------------------*
  Main Loop Callback
 *--------------------------------------------------------------------------*/

static gboolean CalibMainLoopCallback(gpointer user_data)
{
    if (s_calibApp == nullptr || APP::main_canvas == nullptr) return TRUE;

    if (s_calibApp->shouldExit() == true)
    {
        GMainLoop* loop = static_cast<GMainLoop*>(user_data);
        if (loop != nullptr) g_main_loop_quit(loop);
        return FALSE;
    }

    // When the menu is closed (back/exit at root), send Goto_SvmUi once then quit
    if (s_calibApp->isMenuVisible() == false)
    {
        if (s_exitSent == false)
        {
            StopGstPipeline();
            Write_Webserver(Goto_SvmUi);
            s_exitSent = true;
            s_calibApp->requestExit();
        }
        return TRUE;
    }

    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
    int width = 0, height = 0;
    platform.getDisplaySize(width, height);

    platform.beginFrame();
    CalibRenderCallback();
    s_calibApp->update(1.0f / 25.0f);
    s_calibApp->render(width, height);
    platform.endFrame();

    return TRUE;
}

/*--------------------------------------------------------------------------*
  Calib UI thread
 *--------------------------------------------------------------------------*/

static void* Calib_Ui_Thread(void* /*pArg*/)
{
    LOG_APP_SECTION("Calib UI Thread Starting");

    if (APP::app_ready == true) APP::app_ready = false;

    APP::IO::Platform& platform = APP::IO::Platform::getInstance();
    if (platform.makeGLContextCurrent() == false)
    {
        LOG_APP_ERROR("Calib UI Thread: Failed to make GL context current");
        CALIB_UI_CLEANED = 1;
        return nullptr;
    }

    // Create Calib app
    s_calibApp = new APP::AppCalib();
    s_calibApp->setSharedCanvas(APP::main_canvas);

    if (s_calibApp->initialize() == false)
    {
        LOG_APP_ERROR("Calib UI Thread: AppCalib::initialize() failed");
        delete s_calibApp;
        s_calibApp       = nullptr;
        CALIB_UI_CLEANED = 1;
        platform.releaseGLContext();
        return nullptr;
    }
    LOG_APP_SUCCESS("AppCalib initialized");

    // Create main loop
    g_pCalibUiMainContext = g_main_context_new();
    g_pCalibUiMainLoop    = g_main_loop_new(g_pCalibUiMainContext, FALSE);

    GSource* src = g_timeout_source_new(40); // 25 fps
    g_source_set_callback(src, CalibMainLoopCallback, g_pCalibUiMainLoop, nullptr);
    g_source_attach(src, g_pCalibUiMainContext);
    g_main_context_unref(g_pCalibUiMainContext);
    g_pCalibUiMainContext = nullptr;
    g_source_unref(src);
    src = nullptr;

    LOG_APP_INFO("Calib UI Thread: Entering main loop");
    s_exitSent = false;
    g_main_loop_run(g_pCalibUiMainLoop);

    // Show loading screen
    showLoadingScreen();

    // Cleanup
    StopGstPipeline();
    DestroyCalibContext();

    if (g_pCalibUiMainLoop != nullptr)
    {
        g_main_loop_unref(g_pCalibUiMainLoop);
        g_pCalibUiMainLoop = nullptr;
    }

    if (s_calibApp != nullptr)
    {
        s_calibApp->cleanup();
        delete s_calibApp;
        s_calibApp = nullptr;
    }

    APP::main_canvas->resetForAppSwitch();
    CALIB_UI_CLEANED = 1;
    platform.releaseGLContext();

    LOG_APP_SUCCESS("Calib UI Thread Stopped");
    return nullptr;
}

/*--------------------------------------------------------------------------*
  CalibUiProcess class
 *--------------------------------------------------------------------------*/

CalibUiProcess::CalibUiProcess() : m_CloseTick(NOTUSED), m_CloseWaitSec(10), m_timeCount(0) {}
CalibUiProcess::~CalibUiProcess() {}

void CalibUiProcess::Create(S32 _param1, S32 _param2, const char* _pTitle, SWinMain* _pParent)
{
    create_mainwin(const_cast<CHAR*>(_pTitle), _param1, _param2, _pParent);
}

void CalibUiProcess::Time_Callback()
{
    if (m_timeCount >= 10) { if (system("sync")) {} m_timeCount = 0; }
    m_timeCount++;
}

void CalibUiProcess::Swm_Slickey(SMessage& _msg)
{
    LOG_APP_INFOF("Calib UI slickey=%d", _msg.m_parameter1);
    switch (_msg.m_parameter1)
    {
        case 1:  send_message(SWM_CLOSE, CLEANUP_PROCESS);  break;
        case 3:  send_message(SWM_CLOSE, SVM_UI_PROCESS);   break;
        case 4:  send_message(SWM_CLOSE, CALIB_UI_PROCESS); break;
        default: send_message(SWM_CLOSE);                   break;
    }
}

void CalibUiProcess::pre_callback_procedure(SMessage& _msg)
{
    SMainWin::pre_callback_procedure(_msg);

    if (_msg.m_message == SWM_INIT)
    {
        m_CloseWaitSec   = 10;
        m_CloseTick      = (m_CloseWaitSec != 0) ? create_tick(this, m_CloseWaitSec) : NOTUSED;
        m_timeCount      = 0;
        CALIB_UI_CLEANED = 0;
        tTaskCreate(&m_Task, Calib_Ui_Thread, &m_tAttrTask, static_cast<ArgTaskFn>(nullptr), 1);
    }
}

void CalibUiProcess::callback_procedure(SMessage& _msg)
{
    switch (_msg.m_message)
    {
        case SWM_TICK:    Time_Callback();   break;
        case SWM_KEY:                        break;
        case SWM_SLICKEY: Swm_Slickey(_msg); break;
        default:                             break;
    }
}

void CalibUiProcess::post_callback_procedure(SMessage& _msg)
{
    SMainWin::post_callback_procedure(_msg);

    if (_msg.m_message != SWM_CLOSE) return;

    if (m_CloseTick != NOTUSED) { destroy_tick(m_CloseTick); m_CloseWaitSec = 0; }

    backup_process_id = CALIB_UI_PROCESS;

    if (s_calibApp != nullptr) { s_calibApp->requestExit(); usleep(60 * 1000); }

    if (APP::app_ready == false) APP::app_ready = true;

    if (g_pCalibUiMainLoop != nullptr) g_main_loop_quit(g_pCalibUiMainLoop);

    while (CALIB_UI_CLEANED == 0) { LOG_APP_INFO("Calib waiting for clean up"); usleep(1000 * 1000); }

    if (system("sync")) {}

    if (PROCESS_ID_END > _msg.m_parameter1) open_process_id((APP_PROCESS_ID)_msg.m_parameter1);
}
