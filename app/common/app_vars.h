#ifndef __APP_VARS_H__
#define __APP_VARS_H__

#include "constants.h"
#include "logger.h"
#include "ui_core.h"
#include "ui_elements.h"

#define EGL_EGLEXT_PROTOTYPES

#ifdef USE_DVR
    #include "dvr_config.hpp"
#endif

#if defined(USE_SERIAL) || defined(USE_GPS)
    #include "external_sensors.h"
#endif

#if defined(USE_SVM) || defined(USE_AICAM)
    #include "app_conf.h"
#endif

namespace APP
{
    extern std::atomic<bool> app_running;
    extern std::atomic<bool> app_ready;
    extern std::atomic<bool> app_request_shutdown;

    extern UI::ICanvas*     main_canvas;
    extern UI::FrameLayout* root_layout;
    extern UI::ImageView*   main_view;
    
    extern int display_size[2];
    extern unsigned int cam_tex_ids[MAX_BUFFER_NUM];
    
    #ifdef USE_DVR
        extern DvrConfig    dvrConf;
        extern bool         storage_connected;
        extern std::string  storage_path;
        extern long         storage_used_mb;
        extern long         storage_total_mb;
    #endif

    #if defined(USE_SVM) || defined(USE_AICAM)
        extern AppConfiguration appConf;
    #endif

    #ifdef USE_CAN
        extern VEHICLE_MODEL       vehicle_model;
        extern VEHICLE_STATUS      vehicle_status;
        extern std::array<bool, 4> InputSelector;
        extern Seqlock<OBDData>    g_obd;
    #endif

    #ifdef USE_SVM
        extern bool capture_requested;
        extern bool capture_taken;
        extern int  svm_warning_flag[(int)WARNING_FUNCTION_TYPE::W_NUM][5];
    #endif

    #ifdef USE_AICAM
        extern int aicam_warning_level[AI_CAM_NUM];
    #endif

    #if defined(USE_SERIAL) || defined(USE_TAXI)
        extern Seqlock<GPSData> g_gps;
        extern char timestamp[32];
        extern int pua_flag;
    #endif

    #ifdef USE_DVR
        extern Seqlock<IMUData> g_imu;
    #endif

    #ifdef USE_BTO
        extern int aps_level;
        extern int bto_status;
    #endif

    #ifdef USE_TAXI
        extern std::mutex subtitle_data_mutex;
    #endif


    extern std::array<std::vector<AI::BoundingBox>, MAX_BUFFER_NUM> detected_objs;

    extern std::string video_path;
    extern std::string video_file_name;
    extern int         video_channels[4];

    extern int  speed;
    extern bool ble_connect;
}

#endif // __APP_VARS_H__
