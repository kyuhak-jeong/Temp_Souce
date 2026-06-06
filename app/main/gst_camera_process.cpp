/*--------------------------------------------------------------------------*
 *                                                                          *
 *  FILE NAME : gst_camera_process.cpp                                      *
 *                                                                          *
 *  Copyright(c) 2025 by SkyAutoNet.                                        *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#include "app_vars.h"
#include "io_platform.h"
#include "logger.h"
#include <gst_camera_process.hpp>

#ifdef USE_RKNN
    #include "process_detection.hpp"
#endif

#ifdef USE_DVR
    #include "dvr_manager.hpp"
    #include "qdvr_manager.hpp"
#endif

#ifndef USE_CAMERA
    #include "vpb_subtitle.h"
#endif

/*--------------------------------------------------------------------------*
  GLOBAL VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

GstCameraProcess                            g_GstCameraProcess;
GMainLoop*                                  g_pGstCameraMainLoop    = nullptr;
GMainContext*                               g_pGstCameraMainContext = nullptr;
volatile sig_atomic_t                       GST_CAMERA_CLEANED      = 0;

GstData                                     DATA_GST_CAM;

#ifdef USE_DVR
    #if USE_QDVR_MANAGER
        std::shared_ptr<QDvrManager>        g_DvrManager = nullptr;
    #else
        std::shared_ptr<DvrManager>         g_DvrManager = nullptr;
    #endif
    std::mutex                              g_dvrMutex;
#endif

/*--------------------------------------------------------------------------*
  EXTERN VARIABLE DECLARATIONS
 *--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*
  EXTERN FUNCTION DECALRATIONS
 *--------------------------------------------------------------------------*/
extern void gstPipelineCleanup(GstData &gData, std::string name = "");

#ifdef USE_SVM
    extern bool SVM_isFileViewActive();
#endif
#ifdef USE_AICAM
    extern bool AI_isFileViewActive();
#endif

/*--------------------------------------------------------------------------*
  FUNCTION PROTOTYPES
 *--------------------------------------------------------------------------*/
void gstCameraCleanup();

void initDVR();
void stopDVR();
void updateDVR();

void onChannelCountChanged(int newChannelCount);

/*--------------------------------------------------------------------------*
  FUNCTION IMPLEMENTATIONS
 *--------------------------------------------------------------------------*/

void open_Gst_Camera_Process(void) { g_GstCameraProcess.Create(); }

/*--------------------------------------------------------------------------*
Read Subtitle Callback
*--------------------------------------------------------------------------*/
#ifndef USE_CAMERA  // use recorded video instead of camera => read subtitle from recorded video

    std::mutex subtitle_update_mutex;
    static gboolean UpdateSubtitleInfo(GstElement *sink, gpointer user_data)
    {
        std::lock_guard<std::mutex> lock(subtitle_update_mutex);

        GstSample*  sample  = nullptr;
        GstBuffer*  buffer  = nullptr;
        GstMapInfo  map;
        
        g_signal_emit_by_name(sink, "pull-sample", &sample, NULL);

        if (sample == nullptr)
        {
            return GST_FLOW_ERROR;
        }

        buffer = gst_sample_get_buffer(sample);
        if (buffer == nullptr)
        {
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        if (gst_buffer_map(buffer, &map, GST_MAP_READ) == TRUE)
        {
            const gchar* subtitle_char = (const gchar *)map.data;
            if ((subtitle_char != nullptr) && (map.size > 0))
            {
                std::string subtitle_str(subtitle_char, map.size);
                
                // if (subtitle_str[0] == '[')
                // {
                //     subtitle_str = subtitle_str.substr(1, subtitle_str.size() - 2);
                // }

                // std::istringstream iss(subtitle_str);
                // std::vector<std::string> tokens;
                // std::string token;

                // while (iss >> token)
                // {
                //     tokens.push_back(token);
                // }

                // // index 9 : Speed;
                // if (tokens.size() >= 10)
                // {
                //     APP::speed = std::stoi(tokens[8]);
                //     // std::cout << "Speed: " << APP::speed << std::endl;
                // }
                // else
                // {
                //     // Token error
                // }

                VPB::SubtitleData subData;
                VPB::SubtitleParser::parse(subtitle_str, subData);
                APP::speed = subData.obd.speedKmh;
            }
            else
            {
                std::cout << "subtitle: empty" << std::endl;
            }
            if(buffer != nullptr)
            {
                gst_buffer_unmap(buffer, &map);
            }
        }

        if (sample != nullptr)
        {
            gst_sample_unref(sample);
        }

        return GST_FLOW_OK;
    }
#endif

/*--------------------------------------------------------------------------*
GL Sample Callback
*--------------------------------------------------------------------------*/
static inline void discardSample(GstElement* sink)
{
    GstSample* s = nullptr;
    g_signal_emit_by_name(sink, "pull-sample", &s, nullptr);
    if (s != nullptr) gst_sample_unref(s);
}

static GstFlowReturn new_gl_sample(GstElement* sink, GstData* data)
{
    int pidx = -1;
    for (int i = 0; i < MAX_BUFFER_NUM; i++)
        if (data->sinkGL[i] == sink) { pidx = i; break; }

    if (pidx < 0)                return GST_FLOW_ERROR;
    if (APP::app_ready == false) { discardSample(sink); return GST_FLOW_OK; }
 
    GstSample* sample = nullptr;
    GstBuffer* buffer = nullptr;
    GstMemory* mem    = nullptr;
 
    g_signal_emit_by_name(sink, "pull-sample", &sample, nullptr);
    if (sample == nullptr) return GST_FLOW_ERROR;
 
    buffer = gst_sample_get_buffer(sample);
    if (buffer == nullptr) { gst_sample_unref(sample); return GST_FLOW_ERROR; }
 
    mem = gst_buffer_get_memory(buffer, 0);
    if (mem != nullptr)
    {
        if (gst_is_gl_memory(mem) == TRUE)
            APP::cam_tex_ids[pidx] = reinterpret_cast<GstGLMemory*>(mem)->tex_id;
        gst_memory_unref(mem);
    }
    
    if(sample != nullptr)
    {
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

/*--------------------------------------------------------------------------*
MOD Sample Callback
*--------------------------------------------------------------------------*/

// static double __get_us(struct timeval t) { return (t.tv_sec * 1000000 + t.tv_usec); }
// struct timeval start_time, stop_time;

static GstFlowReturn new_mod_sample(GstElement* sink, GstData* data)
{
    int pidx = -1;
    for (int i = 0; i < MAX_BUFFER_NUM; i++)
        if (data->sinkMod[i] == sink) { pidx = i; break; }

    if (pidx < 0)                return GST_FLOW_ERROR;
    if (APP::app_ready == false) { discardSample(sink); return GST_FLOW_OK; }

    bool file_viewer_active = false;
    #ifdef USE_SVM
        file_viewer_active = SVM_isFileViewActive();
    #endif
    #ifdef USE_AICAM
        file_viewer_active = AI_isFileViewActive();
    #endif

    bool input_active = false;
    #ifdef USE_CAN
        input_active = APP::InputSelector[pidx];
    #endif

    if (file_viewer_active == true || input_active == false)
    {
        APP::detected_objs[pidx].clear();
        discardSample(sink);
        return GST_FLOW_OK;
    }
 
    GstSample* sample = nullptr;
    GstBuffer* buffer = nullptr;
    GstMapInfo info;
 
    g_signal_emit_by_name(sink, "pull-sample", &sample, nullptr);
    if (sample == nullptr) return GST_FLOW_ERROR;
 
    buffer = gst_sample_get_buffer(sample);
    if (buffer == nullptr) { gst_sample_unref(sample); return GST_FLOW_ERROR; }
 
    if (gst_buffer_map(buffer, &info, GST_MAP_READ) == TRUE)
    {
        #ifdef USE_RKNN
            process_callback_data_for_inference(info.data, pidx);
        #endif
        gst_buffer_unmap(buffer, &info);
    }
    
    if(sample != nullptr)
    {
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

#ifdef USE_DVR

    /*--------------------------------------------------------------------------*
     * DVR Initialization
     *--------------------------------------------------------------------------*/
    void initDVR()
    {
        if (APP::dvrConf.recordingChannel == 0)
        {
            LOG_DVR_WARNING("Skipping DVR initialization (0 channels)");
            return;
        }

        std::shared_ptr<DvrManager> localDvr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            if (g_DvrManager != nullptr) return;
        }

        // Create shared config
        auto configPtr = std::make_shared<DvrConfig>(APP::dvrConf);
        #if USE_QDVR_MANAGER
            LOG_DVR_INFOF("%sUsing QDvrManager (Enhanced)%s", Color::YELLOW, Color::RESET);
            localDvr = std::make_shared<QDvrManager>(configPtr);
        #else
            LOG_DVR_INFO("Using DvrManager (Base)");
            localDvr = std::make_shared<DvrManager>(configPtr);
        #endif
        
        // Build channel list and FPS map
        std::vector<int> channels;
        std::map<int, int> channelFpsMap;
        
        for (int i = 0; i < APP::dvrConf.recordingChannel; i++)
        {
            int targetFps = APP::dvrConf.isParkingMode ? APP::dvrConf.recordingFpsParking
                          : isSvmCamera(i)             ? APP::dvrConf.recordingFpsAvm
                                                       : APP::dvrConf.recordingFpsInternal;

            channels.push_back(i);
            channelFpsMap[i] = targetFps;
        }
        
        // Initialize DVR
        if (localDvr->initialize(channels, channelFpsMap) == true)
        {
            localDvr->startNormalRecording();
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            g_DvrManager = localDvr;
        }
        else
        {
            LOG_DVR_WARNING("Reset DvrManager due to failed initialization");
            // localDvr is automatically discarded when going out of scope
        }
    }
    
    /*--------------------------------------------------------------------------*
     * DVR Cleanup
     *--------------------------------------------------------------------------*/
    void stopDVR()
    {
        std::shared_ptr<DvrManager> localDvr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            localDvr = g_DvrManager;
            g_DvrManager = nullptr;
        }

        if (localDvr != nullptr)
        {
            LOG_DVR_INFO("Stopping DVR Manager...");
            if (localDvr->isInitialized() == true)
            {
                localDvr->shutdown();
            }
            localDvr.reset();
            LOG_DVR_SUCCESS("DVR Manager stopped");
        }
    }

    /*--------------------------------------------------------------------------*
     * DVR Update (called periodically)
     *--------------------------------------------------------------------------*/
    #ifdef USE_DVR_SUB
        void updateDVRSubtitle()
        {
            std::shared_ptr<DvrManager> dvrMgr = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_dvrMutex);
                dvrMgr = g_DvrManager;
            }
            if ((dvrMgr == nullptr) || (dvrMgr->isInitialized() == false)) return;

            std::string gps, sen, obd;

            #if defined(USE_GPS) || defined(USE_TAXI)
                gps = APP::g_gps.read().toString();
            #else
                gps = TimeUtils::getCurrentTimeString(true);
            #endif

            #if defined(DEVICE_RK3588) || defined (USE_TAXI)
                sen = APP::g_imu.read().toString();
            #endif

            #ifdef USE_CAN
                obd = APP::g_obd.read().toString();
            #endif

            char buf[512];
            snprintf(buf, sizeof(buf), "[%s %s %s]", gps.c_str(), sen.c_str(), obd.c_str());
            dvrMgr->setSubtitleText(buf);
        }
    #endif

    void updateDVR()
    {
        // Check for config changes
        #if defined(USE_SVM) || defined(USE_AICAM)
            if (APP::dvrConf.isDvrSettingsChanged == true)
            {
                LOG_DVR_INFO("DVR settings changed, restarting...");

                if (APP::appConf.syncDvrConfig(APP::dvrConf) == true)
                {
                    stopDVR();
                    gstCameraCleanup();
                    
                    if (APP::dvrConf.writeConfig() == false)
                    {
                        LOG_DVR_ERROR("Failed to save configuration");
                        return;
                    }
                }
                
                APP::dvrConf.isDvrSettingsChanged = false;
                return;
            }
            
            #if defined(USE_SVM) || defined(USE_AICAM)
            {
                bool file_viewer_active = false;
                #ifdef USE_SVM
                    file_viewer_active = SVM_isFileViewActive();
                #endif
                #ifdef USE_AICAM
                    file_viewer_active = AI_isFileViewActive();
                #endif

                static int s_savedRecordingChannel = 0;

                if (file_viewer_active == true && APP::dvrConf.recordingChannel != 0)
                {
                    s_savedRecordingChannel = APP::dvrConf.recordingChannel;
                    onChannelCountChanged(0);
                }
                else if (file_viewer_active == false && APP::dvrConf.recordingChannel == 0
                         && s_savedRecordingChannel != 0)
                {
                    onChannelCountChanged(s_savedRecordingChannel);
                    s_savedRecordingChannel = 0;
                }
            }
            #endif


        #endif

        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }

        if ((dvrMgr != nullptr) && (dvrMgr->isInitialized() == true))
        {
            #ifdef USE_DVR_SUB
                static int updateSubCounter = 0;
                updateSubCounter++;
                
                if (updateSubCounter >= 50 / 30)
                {
                    updateDVRSubtitle();
                    updateSubCounter = 0;
                }
            #endif

            // Update recording state (every 1 second at 50fps)
            static int updateCounter = 0;
            updateCounter++;
            
            if (updateCounter >= 50 * 1)
            {
                dvrMgr->startNormalRecording();
                updateCounter = 0;
            }
            
            // Check storage (every 10 seconds in a background thread to prevent GUI/render block)
            static int storageCounter = 0;
            storageCounter++;
            
            if (storageCounter >= 50 * 10)
            {
                std::thread([](std::shared_ptr<DvrManager> mgr) {
                    if (mgr)
                    {
                        mgr->checkAndCleanStorage();
                    }
                }, dvrMgr).detach();
                storageCounter = 0;
            }
        }
    }

    /*--------------------------------------------------------------------------*
     * Event Trigger Examples
     *--------------------------------------------------------------------------*/
    void onManualTriggeredEvent()
    {
        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }
        if ((dvrMgr != nullptr) && (dvrMgr->isInitialized() == true))
        {
            LOG_DVR_INFO("Manual event triggered");
            EventData eventData(EventType::MANUAL, "User triggered");
            dvrMgr->startEventRecording(eventData);
        }
    }

    void onCollisionDetected(float gForce, const std::string &direction, int v_level)
    {
        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }
        if ((dvrMgr != nullptr) && (dvrMgr->isInitialized() == true))
        {
            LOG_DVR_INFOF("Collision detected: %.2f G, %s", gForce, direction.c_str());
            
            std::ostringstream oss;
            oss << "{\"gForce\":" << gForce 
                << ",\"direction\":\"" << direction
                << ",\"vibration_level\":\"" << v_level
                << "\"}";
            
            EventData eventData(EventType::COLLISION, oss.str());
            dvrMgr->startEventRecording(eventData);
        }
    }

    void onParkingMotionDetected()
    {
        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }
        if ((dvrMgr != nullptr) && (dvrMgr->isInitialized() == true))
        {
            LOG_DVR_INFO("Parking motion detected");
            EventData eventData(EventType::PARKING_MOTION, "Motion in parking");
            dvrMgr->startEventRecording(eventData);
        }
    }

    void DrSafePUAisActivated()
    {
        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }
        if ((dvrMgr != nullptr) && (dvrMgr->isInitialized() == true))
        {
            LOG_DVR_INFO("Dr.Safe PUA Activated");
            EventData eventData(EventType::PUA, "PUA Event Activated");
            dvrMgr->startEventRecording(eventData);
        }
    }

    /*--------------------------------------------------------------------------*
     * Config Change Helpers
     *--------------------------------------------------------------------------*/
    static void updateDvrConfigAndRestart(std::function<void(DvrConfig&)> configUpdater, 
                                          const char* message = nullptr)
    {
        if (message != nullptr)
        {
            LOG_DVR_INFO(message);
        }
        
        stopDVR();
        gstCameraCleanup();
        
        configUpdater(APP::dvrConf);
        
        if (APP::dvrConf.writeConfig() == false)
        {
            LOG_DVR_ERROR("Failed to save configuration");
            return;
        }
    }

    void enterParkingMode()
    {
        updateDvrConfigAndRestart(
            [](DvrConfig &conf) { conf.isParkingMode = true; },
            "Entering parking mode");
    }

    void exitParkingMode()
    {
        updateDvrConfigAndRestart(
            [](DvrConfig &conf) { conf.isParkingMode = false; },
            "Exiting parking mode");
    }

    void onFramerateChanged(int newFps)
    {
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Changing framerate to %d fps", newFps);

        updateDvrConfigAndRestart(
            #ifdef USE_SVM
                [newFps](DvrConfig &conf) { conf.recordingFpsAvm = newFps; },
            #else
                [newFps](DvrConfig &conf) { conf.recordingFpsInternal = newFps; },
            #endif
            msg);
    }

    void onChannelCountChanged(int newChannelCount)
    {
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Changing channel count to %d", newChannelCount);
        
        updateDvrConfigAndRestart(
            [newChannelCount](DvrConfig &conf) { conf.recordingChannel = newChannelCount; },
            msg);
    }

    void onAudioEnableChanged(bool newEnabled)
    {
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Changing audio enabled to %d", (int)newEnabled);
        
        updateDvrConfigAndRestart(
            [newEnabled](DvrConfig &conf) { conf.audioEnabled = newEnabled; },
            msg);
    }

    /*--------------------------------------------------------------------------*
     * DVR Sample Callback (receives encoded video data)
     *--------------------------------------------------------------------------*/
    static GstFlowReturn new_dvr_sample(GstElement* sink, gpointer /*user_data*/)
    {
        int channelIdx = -1;
        for (int i = 0; i < MAX_BUFFER_NUM; i++)
            if (DATA_GST_CAM.sinkDvr[i] == sink) { channelIdx = i; break; }
    
        if (channelIdx < 0)                              return GST_FLOW_ERROR;
        if (channelIdx >= APP::dvrConf.recordingChannel) { discardSample(sink); return GST_FLOW_FLUSHING; }
        if (APP::app_ready == false)                     { discardSample(sink); return GST_FLOW_OK; }
    
        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }
        if (dvrMgr == nullptr || dvrMgr->isInitialized() == false) { discardSample(sink); return GST_FLOW_FLUSHING; }
    
        GstSample* sample = nullptr;
        GstBuffer* buffer = nullptr;
        GstMapInfo info;
    
        g_signal_emit_by_name(sink, "pull-sample", &sample, nullptr);
        if (sample == nullptr) return GST_FLOW_ERROR;

        GstCaps* caps = gst_sample_get_caps(sample);
        if (caps != nullptr)
        {
            dvrMgr->storeVideoCaps(channelIdx, caps);
        }
    
        buffer = gst_sample_get_buffer(sample);
        if (buffer == nullptr) { gst_sample_unref(sample); return GST_FLOW_ERROR; }
    
        if (gst_buffer_map(buffer, &info, GST_MAP_READ) == TRUE)
        {
            if (info.data != nullptr && info.size > 0)
            {
                GstClockTime pts = GST_BUFFER_PTS(buffer);
                dvrMgr->pushEncodedData(channelIdx, info.data, info.size, pts);
            }
            
            gst_buffer_unmap(buffer, &info);
        }
    
        if(sample != nullptr)
        {
            gst_sample_unref(sample);
        }
        return GST_FLOW_OK;
    }

    static GstFlowReturn new_audio_sample(GstElement* sink, gpointer /*user_data*/)
    {
        if (APP::app_ready == false) { discardSample(sink); return GST_FLOW_OK; }

        std::shared_ptr<DvrManager> dvrMgr = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_dvrMutex);
            dvrMgr = g_DvrManager;
        }
        if (dvrMgr == nullptr || dvrMgr->isInitialized() == false) { discardSample(sink); return GST_FLOW_FLUSHING; }

        GstSample* sample = nullptr;
        GstBuffer* buffer = nullptr;
        GstMapInfo info;

        g_signal_emit_by_name(sink, "pull-sample", &sample, nullptr);
        if (sample == nullptr) return GST_FLOW_ERROR;

        GstCaps* caps = gst_sample_get_caps(sample);
        if (caps != nullptr)
        {
            dvrMgr->storeAudioCaps(caps);
        }

        buffer = gst_sample_get_buffer(sample);
        if (buffer == nullptr) { gst_sample_unref(sample); return GST_FLOW_ERROR; }

        if (gst_buffer_map(buffer, &info, GST_MAP_READ) == TRUE)
        {
            if (info.data != nullptr && info.size > 0)
            {
                GstClockTime pts = GST_BUFFER_PTS(buffer);
                dvrMgr->pushEncodedData(AUDIO_CH_ID, info.data, info.size, pts);
            }
            gst_buffer_unmap(buffer, &info);
        }

        if(sample != nullptr)
        {
            gst_sample_unref(sample);
        }
        return GST_FLOW_OK;
    }

#endif // USE_DVR

/*--------------------------------------------------------------------------*
Main Loop Callback
*--------------------------------------------------------------------------*/
void gstCameraCleanup()
{
    if (GST_CAMERA_CLEANED != 0) return;
 
    GstState state, pending;
    gst_element_get_state(DATA_GST_CAM.pipeline, &state, &pending, GST_CLOCK_TIME_NONE);
    if (state == GST_STATE_PLAYING)
    {
        gstPipelineCleanup(DATA_GST_CAM, "GST_CAM");
        for (int i = 0; i < MAX_BUFFER_NUM; i++)
            APP::cam_tex_ids[i] = 0;    // borrowed from GStreamer — never glDelete
    }
 
    GST_CAMERA_CLEANED = 1;
}

static void renderPlainMainScreen()
{
    #if !defined(USE_SVM) && !defined(USE_AICAM)

        if (APP::main_canvas == nullptr) return;

        // Collect cameras with valid GL textures
        std::vector<int> cams;
        for (int i = 0; i < MAX_BUFFER_NUM; i++)
            if (APP::cam_tex_ids[i] != 0) cams.push_back(i);
        if (cams.empty() == true) return;

        // Static texture wrappers — reused every frame, no allocation
        static APP::UI::Texture s_tex[MAX_BUFFER_NUM];

        const int   n     = static_cast<int>(cams.size());
        const float dispW = static_cast<float>(APP::display_size[0]);
        const float dispH = static_cast<float>(APP::display_size[1]);

        int side = 1;
        while (side * side < n) side++;
        const float cW = dispW / side;
        const float cH = dispH / side;

        // Paints
        const APP::UI::Paint pTex;
        const APP::UI::Paint pLblText(APP::UI::Color::White);
              APP::UI::Paint pBox;   pBox.filled = true;

        // GL context is already current for this thread (acquired in Gst_Camera_Thread)
        APP::IO::Platform::getInstance().beginFrame();
        APP::main_canvas->begin();

        for (int i = 0; i < n; i++)
        {
            const int   cam = cams[i];
            const float x   = (i % side) * cW;
            const float y   = (i / side) * cH;

            // Camera frame — srcRect is pixel coords, not normalised UVs
            s_tex[cam].setExternalTexture(APP::cam_tex_ids[cam], IMG_WIDTH, IMG_HEIGHT);
            APP::main_canvas->drawTexture(&s_tex[cam],
                APP::UI::RectF(0, 0, IMG_WIDTH, IMG_HEIGHT),
                APP::UI::RectF::fromXYWH(x, y, cW, cH), pTex);

            // Label
            std::string lbl = "Cam" + std::to_string(cam) + ": " + std::to_string(SRC_FRAMERATE) + "fps";
            #ifdef USE_DVR
                if (cam < APP::dvrConf.recordingChannel)
                {
                    int  fps = APP::dvrConf.isParkingMode ? APP::dvrConf.recordingFpsParking
                             : isSvmCamera(cam)           ? APP::dvrConf.recordingFpsAvm
                                                          : APP::dvrConf.recordingFpsInternal;
                    lbl += " [REC@" + std::to_string(fps) + "fps]";
                }
            #endif

            APP::main_canvas->drawText(lbl, APP::UI::Vec2(x + 8.0f, y + 8.0f), pLblText);

            // Detection boxes — BoundingBox: x,y,width,height normalised; color; label
            for (const auto& obj : APP::detected_objs[cam])
            {
                const float bx = x + obj.x * cW;
                const float by = y + obj.y * cH;
                pBox.bgColor = obj.color.withAlpha(0.3f);
                APP::main_canvas->drawRect(APP::UI::RectF::fromXYWH(bx, by, obj.width * cW, obj.height * cH), pBox);
                if (obj.label.empty() == false)
                    APP::main_canvas->drawText(obj.label, APP::UI::Vec2(bx + 4.0f, by - 24.0f), pLblText);
            }
        }

        APP::main_canvas->end();
        APP::IO::Platform::getInstance().endFrame();

    #endif
}

static gboolean GstCameraMainLoopCallback(gpointer user_data)
{
    if (APP::app_running == false)
    {
        // #ifdef USE_DVR
        //     stopDVR();
        // #endif

        // gstCameraCleanup();

        g_main_loop_quit((GMainLoop*)g_pGstCameraMainLoop);
        return FALSE;
    }

    if (DATA_GST_CAM.pipeline == nullptr)
    {
        if (APP::app_ready == true)  // init pipeline once when app is ready
        {

            std::string pipeStr = "gst-launch-1.0 \n";

            /*
            #ifdef USE_CAMERA
                pipeStr += ("v4l2src device=/dev/video" + std::to_string(APP::video_channels[0]) + " io-mode=dmabuf ! "
                                "video/x-raw,format=NV12,width=" + std::to_string(IMG_WIDTH) + ",height=" + std::to_string(IMG_HEIGHT) + ",framerate=" + std::to_string(SRC_FRAMERATE) + "/1 ! tee name=t0 \n"
                            "v4l2src device=/dev/video" + std::to_string(APP::video_channels[1]) + " io-mode=dmabuf ! "
                                "video/x-raw,format=NV12,width=" + std::to_string(IMG_WIDTH) + ",height=" + std::to_string(IMG_HEIGHT) + ",framerate=" + std::to_string(SRC_FRAMERATE) + "/1 ! tee name=t1 \n"
                            "v4l2src device=/dev/video" + std::to_string(APP::video_channels[2]) + " io-mode=dmabuf ! "
                                "video/x-raw,format=NV12,width=" + std::to_string(IMG_WIDTH) + ",height=" + std::to_string(IMG_HEIGHT) + ",framerate=" + std::to_string(SRC_FRAMERATE) + "/1 ! tee name=t2 \n"
                            "v4l2src device=/dev/video" + std::to_string(APP::video_channels[3]) + " io-mode=dmabuf ! "
                                "video/x-raw,format=NV12,width=" + std::to_string(IMG_WIDTH) + ",height=" + std::to_string(IMG_HEIGHT) + ",framerate=" + std::to_string(SRC_FRAMERATE) + "/1 ! tee name=t3 \n");
            #else
                pipeStr += ("filesrc location=" + std::string(APP::video_path) + std::string(APP::video_file_name) + " ! qtdemux name=demux \n"
                            "demux.subtitle_0 ! queue ! appsink name=subsink \n"
                            "demux.video_" + std::to_string(APP::video_channels[0]) + " ! queue max-size-buffers=3 ! h265parse ! decodebin ! videoconvert ! tee name=t0 \n"
                            "demux.video_" + std::to_string(APP::video_channels[1]) + " ! queue max-size-buffers=3 ! h265parse ! decodebin ! videoconvert ! tee name=t1 \n"
                            "demux.video_" + std::to_string(APP::video_channels[2]) + " ! queue max-size-buffers=3 ! h265parse ! decodebin ! videoconvert ! tee name=t2 \n"
                            "demux.video_" + std::to_string(APP::video_channels[3]) + " ! queue max-size-buffers=3 ! h265parse ! decodebin ! videoconvert ! tee name=t3 \n");
            #endif

            pipeStr += ("t0. ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGB ! appsink name=gl_sink0 \n"
                        "t1. ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGB ! appsink name=gl_sink1 \n"
                        "t2. ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGB ! appsink name=gl_sink2 \n"
                        "t3. ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGB ! appsink name=gl_sink3 \n"

                        "t0. ! queue ! appsink name=mod_sink0 \n"
                        "t1. ! queue ! appsink name=mod_sink1 \n"
                        "t2. ! queue ! appsink name=mod_sink2 \n"
                        "t3. ! queue ! appsink name=mod_sink3 \n");
            */
            

            #ifdef EMBEDDED_DEVICE      // Embedded camera
                std::string videoCaps = " io-mode=dmabuf ! video/x-raw,format=NV12,width=" + 
                                       std::to_string(IMG_WIDTH) + ",height=" + 
                                       std::to_string(IMG_HEIGHT) + ",framerate=" + 
                                       std::to_string(SRC_FRAMERATE) + "/1 ! ";
            #else                       // PC camera
                std::string videoCaps = " io-mode=dmabuf ! videoconvert ! video/x-raw ! ";
            #endif

            #ifndef USE_CAMERA          // not use camera
                pipeStr += ("filesrc location=" + std::string(APP::video_path) + 
                           std::string(APP::video_file_name) + " ! qtdemux name=demux \n" +
                           "demux.subtitle_0 ! queue ! appsink name=subsink \n");
            #endif

            for(int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                std::string logIdx  = std::to_string(i);
                std::string devIdx  = std::to_string(APP::video_channels[i]);
                #ifdef USE_CAMERA       // use camera
                    pipeStr += ("v4l2src device=/dev/video" + devIdx + videoCaps + "tee name=t" + logIdx + " \n");
                #else
                    pipeStr += ("demux.video_" + devIdx + " ! queue max-size-buffers=3 ! h265parse ! decodebin ! videoconvert ! tee name=t" + logIdx + " \n");
                #endif
            }
            for(int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                std::string logIdx = std::to_string(i);
                pipeStr += ("t" + logIdx + ". ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGB ! appsink name=gl_sink" + logIdx + " \n");
            }

            #ifdef EMBEDDED_DEVICE
                for(int i = 0; i < SVM_CAMERAS_NUM; i++)
                {
                    std::string logIdx = std::to_string(i);
                    pipeStr += ("t" + logIdx + ". ! queue ! rgaconvert ! video/x-raw,format=RGB,width=416,height=416 ! appsink name=mod_sink" + logIdx + " \n");
                }
            #endif


            #ifdef USE_DVR
                
                /*pipeStr +=    "t0. ! queue ! videorate name=rate0 ! mpph265enc name=encoder0 ! appsink name=dvr_sink0 \n"
                                "t1. ! queue ! videorate name=rate1 ! mpph265enc name=encoder1 ! appsink name=dvr_sink1 \n"
                                "t2. ! queue ! videorate name=rate2 ! mpph265enc name=encoder2 ! appsink name=dvr_sink2 \n"
                                "t3. ! queue ! videorate name=rate3 ! mpph265enc name=encoder3 ! appsink name=dvr_sink3 \n";*/

                for(int i = 0; i < APP::dvrConf.recordingChannel; i++)
                {
                    if (i >= MAX_BUFFER_NUM)
                    {
                        LOG_GST_WARNINGF("Channel %d exceeds MAX_BUFFER_NUM(%d), skip adding to pipeline", i, MAX_BUFFER_NUM);
                        break;
                    }

                    std::string logIdx        = std::to_string(i);
                    bool        isInternalCam  = !isSvmCamera(i);

                    int targetFps    = APP::dvrConf.isParkingMode ? APP::dvrConf.recordingFpsParking
                                     : isInternalCam              ? APP::dvrConf.recordingFpsInternal
                                                                  : APP::dvrConf.recordingFpsAvm;

                    int targetWidth  = isInternalCam ? APP::dvrConf.recordingWidthInternal  : APP::dvrConf.recordingWidth;
                    int targetHeight = isInternalCam ? APP::dvrConf.recordingHeightInternal : APP::dvrConf.recordingHeight;

                    (void) targetWidth;
                    (void) targetHeight;

                    #ifdef USE_SVM
                        if (i >= SVM_CAMERAS_NUM)
                        {
                            pipeStr += ("\nv4l2src device=/dev/video" + std::to_string(i + 7) + videoCaps + "tee name=t" + logIdx + " \n");
                            pipeStr += ("t" + logIdx + ". ! queue ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGB ! appsink name=gl_sink" + logIdx + " \n");
                        }
                    #endif

                    #ifdef EMBEDDED_DEVICE  // embeded camera => use mpph265enc

                        // Set GOP to match 1 second of video
                        int gop = targetFps;
                                                                
                        #if(0)
                            // Scale bitrate proportionally to FPS
                            // Base: 4 Mbps for 30fps => ~133kbps per frame
                            int baseBitrate = 4000000;
                            int baseFps = 30;
                            int bitrate = (baseBitrate * targetFps) / baseFps; // Proportional scaling
                        #else
                            // fixed bitrate
                            int bitrate = 4000000;
                        #endif

                        pipeStr += ("t" + logIdx + ". ! " + 
                                    "queue ! videorate name=rate" + logIdx + 
                                    " drop-only=true ! capsfilter name=capsfilt" + logIdx +
                                    " caps=video/x-raw,framerate=" + std::to_string(targetFps) + "/1\n" +
                                    " ! mpph265enc name=encoder" + logIdx + 
                                    " bps=" + std::to_string(bitrate) + 
                                    " bps-min=" + std::to_string((int)(bitrate * 0.9)) + 
                                    " bps-max=" + std::to_string((int)(bitrate * 1.2)) +
                                    " gop=" + std::to_string(gop) +
                                    " rc-mode=1 qp-min=10 qp-max=51 header-mode=1"
                                    " ! h265parse ! video/x-h265,stream-format=hvc1,alignment=au"
                                    " ! appsink name=dvr_sink" + logIdx + " \n");

                    #else   // pc Use x265enc
            
                        pipeStr += ("t" + logIdx + ". ! " + 
                                    "queue ! videorate name=rate" + logIdx + 
                                    " drop-only=true ! capsfilter name=capsfilt" + logIdx +
                                    " caps=video/x-raw,framerate=" + std::to_string(targetFps) + "/1\n" +
                                    " ! videoscale name=scale" + logIdx +
                                    " ! video/x-raw,width=" + std::to_string(targetWidth) + 
                                    ",height=" + std::to_string(targetHeight) + "\n" +
                                    " ! x265enc name=encoder" + logIdx + 
                                    " speed-preset=ultrafast tune=zerolatency" +
                                    " ! h265parse ! video/x-h265,stream-format=hvc1,alignment=au"
                                    " ! appsink name=dvr_sink" + logIdx + " \n");
                        
                    #endif
                }

                if (APP::dvrConf.audioEnabled == true)
                {
                    int bitrate = (APP::dvrConf.audioBitrate > 0) ? APP::dvrConf.audioBitrate : 128000;
                    pipeStr += ("alsasrc device=" + APP::dvrConf.audioDevice +
                               " ! audio/x-raw,format=S" + std::to_string(APP::dvrConf.audioBitDepth) + "LE" +
                               ",rate=" + std::to_string(APP::dvrConf.audioSampleRate) +
                               ",channels=" + std::to_string(APP::dvrConf.audioChannels) +
                               ",layout=interleaved ! queue ! audioconvert" +
                               " ! audiowsinclimit name=am_hp mode=high-pass cutoff=80" +
                               " ! audiowsinclimit name=am_lp mode=low-pass cutoff=4000" +
                               " ! audiodynamic characteristics=hard-knee mode=compressor threshold=0.15 ratio=3.0" +
                               " ! volume name=am_vol volume=4.0 ! audioresample" +
                               " ! avenc_aac bitrate=" + std::to_string(bitrate) +
                               " ! aacparse ! audio/mpeg,mpegversion=4,stream-format=raw"
                               " ! appsink name=am_sink \n");
                }

            #endif


            LOG_GST_PIPELINE("GstCamera", pipeStr.c_str());


            DATA_GST_CAM.pipeline = gst_parse_launch(pipeStr.c_str(), nullptr);

            if (DATA_GST_CAM.pipeline == nullptr)
            {
                LOG_GST_ERROR("Failed to create GStreamer pipeline");
                return TRUE;
            }

            #ifndef USE_CAMERA     // Subtitle on PC with video
                DATA_GST_CAM.sinkSub = gst_bin_get_by_name(GST_BIN (DATA_GST_CAM.pipeline), "subsink");
                g_object_set(G_OBJECT(DATA_GST_CAM.sinkSub), "drop", FALSE, "sync", FALSE, "emit-signals", TRUE, "max-buffers", 1, NULL);
                g_signal_connect (DATA_GST_CAM.sinkSub, "new-sample", G_CALLBACK(UpdateSubtitleInfo), nullptr);
            #endif // USE_CAMERA
            
            
            // Setup for gl_sink, mod_sink
            for(int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                std::string glSinkName = "gl_sink" + std::to_string(i);
                DATA_GST_CAM.sinkGL[i] = gst_bin_get_by_name(GST_BIN(DATA_GST_CAM.pipeline), glSinkName.c_str());

                if(DATA_GST_CAM.sinkGL[i] != nullptr)
                {
                    g_object_set(DATA_GST_CAM.sinkGL[i], "drop", TRUE, "sync", TRUE, "emit-signals", TRUE, "max-buffers", 1, NULL);
                    g_signal_connect(DATA_GST_CAM.sinkGL[i], "new-sample", G_CALLBACK(new_gl_sample), &DATA_GST_CAM);
                }
                else
                {
                    LOG_GST_WARNINGF("Failed to create %s", glSinkName.c_str());
                }

                std::string modSinkName = "mod_sink" + std::to_string(i);
                DATA_GST_CAM.sinkMod[i] = gst_bin_get_by_name(GST_BIN(DATA_GST_CAM.pipeline), modSinkName.c_str());
                if(DATA_GST_CAM.sinkMod[i] != nullptr)
                {
                    g_object_set(DATA_GST_CAM.sinkMod[i], "drop", TRUE, "sync", TRUE, "emit-signals", TRUE, "max-buffers", 1, NULL);
                    g_signal_connect(DATA_GST_CAM.sinkMod[i], "new-sample", G_CALLBACK(new_mod_sample), &DATA_GST_CAM);
                }
                else
                {
                    LOG_GST_WARNINGF("Failed to create %s", modSinkName.c_str());
                }
            }

            #ifdef USE_DVR
                // Setup for dvr_sink for all recordingChannel
                for(int i = 0; i < APP::dvrConf.recordingChannel; i++)
                {
                    if (i >= MAX_BUFFER_NUM)
                    {
                        LOG_GST_WARNINGF("Channel %d exceeds MAX_BUFFER_NUM(%d), skip connecting sinkDvr", i, MAX_BUFFER_NUM);
                        break;
                    }

                    std::string dvrSinkName = "dvr_sink" + std::to_string(i);
                    DATA_GST_CAM.sinkDvr[i] = gst_bin_get_by_name(GST_BIN(DATA_GST_CAM.pipeline), dvrSinkName.c_str());
                    
                    if(DATA_GST_CAM.sinkDvr[i] != nullptr)
                    {
                        g_object_set(DATA_GST_CAM.sinkDvr[i], "drop", TRUE, "sync", FALSE, "emit-signals", TRUE, "max-buffers", 1, NULL);
                        g_signal_connect(DATA_GST_CAM.sinkDvr[i], "new-sample", G_CALLBACK(new_dvr_sample), nullptr);
                    }
                    else
                    {
                        LOG_GST_WARNINGF("Failed to create %s", dvrSinkName.c_str());
                    }

                    if(i >= SVM_CAMERAS_NUM)
                    {
                        std::string glSinkName = "gl_sink" + std::to_string(i);
                        DATA_GST_CAM.sinkGL[i] = gst_bin_get_by_name(GST_BIN(DATA_GST_CAM.pipeline), glSinkName.c_str());

                        if (DATA_GST_CAM.sinkGL[i] != nullptr)
                        {
                            g_object_set(DATA_GST_CAM.sinkGL[i], "drop", TRUE, "sync", TRUE, "emit-signals", TRUE, "max-buffers", 1, NULL);
                            g_signal_connect(DATA_GST_CAM.sinkGL[i], "new-sample", G_CALLBACK(new_gl_sample), &DATA_GST_CAM);
                        }
                        else
                        {
                            LOG_GST_WARNINGF("Failed to create %s", glSinkName.c_str());
                        }
                    }
                }

                if (APP::dvrConf.audioEnabled == true)
                {
                    DATA_GST_CAM.sinkAudio = gst_bin_get_by_name(GST_BIN(DATA_GST_CAM.pipeline), "am_sink");
                    if (DATA_GST_CAM.sinkAudio != nullptr)
                    {
                        g_object_set(DATA_GST_CAM.sinkAudio, "emit-signals", TRUE, "sync", FALSE, "drop", FALSE, "max-buffers", 4, nullptr);
                        g_signal_connect(DATA_GST_CAM.sinkAudio, "new-sample", G_CALLBACK(new_audio_sample), nullptr);
                    }
                    else
                    {
                        LOG_GST_WARNING("Failed to create am_sink");
                    }
                }

            #endif

            GstContext* context = APP::IO::Platform::getInstance().getGstContext();
            if(context != nullptr)
            {
                gst_element_set_context (GST_ELEMENT (DATA_GST_CAM.pipeline), context);
            }

            GST_CAMERA_CLEANED = 0;

            GstStateChangeReturn ret = gst_element_set_state(DATA_GST_CAM.pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) 
            {
                LOG_GST_ERROR("Unable to set the GstCamera pipeline to the playing state");
                gstPipelineCleanup(DATA_GST_CAM, "GST_CAM");
                return false;
            }

            #ifdef USE_DVR
                initDVR();
            #endif

            // DATA_GST_CAM.bus = gst_element_get_bus(DATA_GST_CAM.pipeline);
        }
    }
    else // normal loop runs here
    {
        if (APP::app_ready == false)             // this called when app need to exit => cleanup
        {
            #ifdef USE_DVR
                stopDVR();
            #endif
            
            gstCameraCleanup();
        }
        else                                    // normal loop runs here
        {
            renderPlainMainScreen();

            #ifdef USE_DVR
                updateDVR();
            #endif
        }
    }

    return TRUE;
}


void* Gst_Camera_Thread(void* pArg)
{
    #ifdef USE_RKNN
        init_process_detection();
    #endif

    #if !defined(USE_SVM) && !defined(USE_AICAM)
        APP::IO::Platform& platform = APP::IO::Platform::getInstance();
        if (platform.makeGLContextCurrent() == false)
        {
            LOG_APP_ERROR("Gst_Camera_Thread: Failed to make GL context current");
        }
        else if (APP::main_canvas != nullptr)
        {
            // No AppBase in plain mode — load fonts manually so drawText works
            const std::vector<APP::UI::FontConfig> fontCfgs = {
                APP::UI::FontConfig("kr", "NotoSansKR-Bold", std::string(_FONTS_PATH_) + "/NotoSansKR-Bold.ttf",
                                    60.0f, APP::UI::CharacterSet::KOREAN, 2048, 2048)
            };
            APP::main_canvas->reloadFonts(fontCfgs);
        }
    #endif

    g_pGstCameraMainContext = g_main_context_new();
    g_pGstCameraMainLoop    = g_main_loop_new(g_pGstCameraMainContext, FALSE);
    GSource* src = g_timeout_source_new(20);        // 50 fps
    g_source_set_callback(src, GstCameraMainLoopCallback, g_pGstCameraMainLoop, NULL);
    g_source_attach(src, g_pGstCameraMainContext);

    g_main_context_unref(g_pGstCameraMainContext);
    g_pGstCameraMainContext = nullptr;
    g_source_unref(src);
    src = nullptr;

    g_main_loop_run(g_pGstCameraMainLoop);

    if (g_pGstCameraMainLoop != nullptr)
    {
        g_main_loop_unref(g_pGstCameraMainLoop);
        g_pGstCameraMainLoop = nullptr;
    }

    #ifdef USE_RKNN
        deinit_process_detection();
    #endif

    #if !defined(USE_SVM) && !defined(USE_AICAM)
        platform.releaseGLContext();
    #endif

    // eglMakeCurrent(APP::IO::Platform::getInstance().getEGLDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglReleaseThread();

    printf("\n----------taskFunction_GstCamera done-----------\n");
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

GstCameraProcess::GstCameraProcess() : m_CloseTick(NOTUSED), m_CloseWaitSec(10), m_timeCount(0) {}
GstCameraProcess::~GstCameraProcess() {}

void GstCameraProcess::Create(S32 _param1, S32 _param2, const char *_pTitle, SWinMain* _pParent)
{
	create_mainwin((CHAR*)_pTitle, _param1, _param2, _pParent);
}

void GstCameraProcess::Time_Callback(void)
{
    if (m_timeCount >= 10) { if (system("sync")) {}; m_timeCount = 0; }
    m_timeCount++;
}

void GstCameraProcess::Swm_Slickey(SMessage &_msg)
{
    LOG_APP_INFOF("GstCamera key=%d", _msg.m_parameter1);
    switch (_msg.m_parameter1)
    {
        case 1:  send_message(SWM_CLOSE, CLEANUP_PROCESS); break;
        default: send_message(SWM_CLOSE);                  break;
    }
}

void GstCameraProcess::pre_callback_procedure(SMessage &_msg)
{
	SMainWin::pre_callback_procedure(_msg);

    if (_msg.m_message == SWM_INIT)
    {
        m_CloseWaitSec = 10;
        m_CloseTick    = (m_CloseWaitSec != 0) ? create_tick(this, m_CloseWaitSec) : NOTUSED;
        m_timeCount    = 0;
        tTaskCreate(&m_Task, Gst_Camera_Thread, &m_tAttrTask, static_cast<ArgTaskFn>(nullptr), 1);
    }
}

void GstCameraProcess::callback_procedure(SMessage &_msg)
{
    switch (_msg.m_message)
    {
        case SWM_TICK:    Time_Callback();       break;
        case SWM_KEY:                            break;
        case SWM_SLICKEY: Swm_Slickey(_msg);     break;
        default:                                 break;
    }
}

void GstCameraProcess::post_callback_procedure(SMessage &_msg)
{
	SMainWin::post_callback_procedure(_msg);

    if (_msg.m_message != SWM_CLOSE) return;

    if (m_CloseTick != NOTUSED) { destroy_tick(m_CloseTick); m_CloseWaitSec = 0; }

    backup_process_id = GST_CAMERA_PROCESS;

    if (system("sync")) {}

    if (PROCESS_ID_END > _msg.m_parameter1) open_process_id((APP_PROCESS_ID)_msg.m_parameter1);
}
