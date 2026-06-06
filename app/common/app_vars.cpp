#include "app_vars.h"

namespace APP
{
    std::atomic<bool> app_running{true};
    std::atomic<bool> app_ready{false};
    std::atomic<bool> app_request_shutdown{false};
    
    UI::ICanvas*     main_canvas = nullptr;
    UI::FrameLayout* root_layout = nullptr;
    UI::ImageView*   main_view   = nullptr;

    int display_size[2] = {1920, 1080};

    unsigned int cam_tex_ids[MAX_BUFFER_NUM] = {0};

    #ifdef USE_DVR
        DvrConfig    dvrConf;
        bool         storage_connected = false;
        std::string  storage_path      = "/";
        long         storage_used_mb   = 0;
        long         storage_total_mb  = 0;
    #endif
    
    #if defined(USE_SVM) || defined(USE_AICAM)
        AppConfiguration appConf;
    #endif

    #ifdef USE_CAN
        VEHICLE_MODEL       vehicle_model = VEHICLE_MODEL::Simulator;
        VEHICLE_STATUS      vehicle_status;
        std::array<bool, 4> InputSelector = {true, true, true, true};
        Seqlock<OBDData>    g_obd;
    #endif

    #ifdef USE_SVM
        bool capture_requested = false;
        bool capture_taken     = false;
        int  svm_warning_flag[(int)WARNING_FUNCTION_TYPE::W_NUM][5] = {0};
    #endif

    #ifdef USE_AICAM
        int aicam_warning_level[AI_CAM_NUM] = {0};
    #endif

    #if defined(USE_SERIAL) || defined(USE_TAXI)
        Seqlock<GPSData> g_gps;
        char timestamp[32] = {0};
        int pua_flag = -1;
    #endif

    #ifdef USE_DVR
        Seqlock<IMUData> g_imu;
    #endif

    #ifdef USE_BTO
        int aps_level  = 0;
        int bto_status = 0;
    #endif

    #ifdef USE_TAXI
        std::mutex subtitle_data_mutex;
    #endif

    std::array<std::vector<AI::BoundingBox>, MAX_BUFFER_NUM> detected_objs;

    std::string video_path        = std::string(DEFAULT_VIDEO_PATH);
    std::string video_file_name   = std::string(DEFAULT_VIDEO_FILE);
    int         video_channels[4] = {0, 1, 2, 3};

    int  speed       = 0;
    bool ble_connect = false;
}
