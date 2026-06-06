/*==========================================================================
 * app_conf.h - Application Configuration
 *==========================================================================*/
#ifndef APP_CONF_H
#define APP_CONF_H

#include "constants.h"

/*--------------------------------------------------------------------------
 * Language / Network constants
 *--------------------------------------------------------------------------*/
constexpr int Language_English   = 0;
constexpr int Language_Korean    = 1;
constexpr int NetworkMode_Server = 0;
constexpr int NetworkMode_Client = 1;

/*--------------------------------------------------------------------------
 * SystemTime
 *--------------------------------------------------------------------------*/
struct SystemTime
{
    int  year      = 2024;
    int  month     = 1;
    int  day       = 1;
    int  hour      = 0;
    int  minute    = 0;
    int  second    = 0;
    bool auto_time = true;

    bool operator==(const SystemTime& o) const
    {
        return year == o.year && month == o.month && day == o.day &&
               hour == o.hour && minute == o.minute && second == o.second &&
               auto_time == o.auto_time;
    }
    bool operator!=(const SystemTime& o) const { return !(*this == o); }
};

/*--------------------------------------------------------------------------
 * Feature-specific menu structs
 *--------------------------------------------------------------------------*/

#ifdef USE_DVR
    #include "dvr_config.hpp"

    struct M_MENU_DVR
    {
        // Video
        int  recording_time         = 60;
        int  recording_channel      = 4;
        int  recording_fps_avm      = 30;
        int  recording_fps_internal = 30;

        // Audio
        bool audio_enabled          = true;

        bool operator==(const M_MENU_DVR& o) const
        {
            return recording_time         == o.recording_time         &&
                   recording_channel      == o.recording_channel      &&
                   recording_fps_avm      == o.recording_fps_avm      &&
                   recording_fps_internal == o.recording_fps_internal &&
                   audio_enabled          == o.audio_enabled;
        }
        bool operator!=(const M_MENU_DVR& o) const { return !(*this == o); }
    };
#endif // USE_DVR


#ifdef USE_SVM
    #include "svmXML.hpp"

    struct M_MENU_SVM
    {
        bool visible          = false;
        int  selected_viewIdx = 0;
        int  selected_camIdx  = 0;

        std::array<bool, 5>                 activation   = {};
        std::array<int, 5>                  view_setting = {};
        std::array<std::array<float, 6>, 4> vcam         = {};
        std::array<std::array<int,   4>, 4> rcam         = {};

        bool operator==(const M_MENU_SVM& o) const
        {
            return visible          == o.visible          &&
                   selected_viewIdx == o.selected_viewIdx &&
                   selected_camIdx  == o.selected_camIdx  &&
                   activation       == o.activation       &&
                   view_setting     == o.view_setting     &&
                   vcam             == o.vcam             &&
                   rcam             == o.rcam;
        }
        bool operator!=(const M_MENU_SVM& o) const { return !(*this == o); }
    };

    struct M_MENU_CALIB
    {
        MODEL                 model;
        VEHICLE_INFORMATION   veh_info;
        VEHICLE_SPECIFICATION veh_spec;

        int   svm_calibration_type = 0;
        int   svm_cameras_num      = 0;
        int   svm_patterns_num     = 0;
        float svm_cameras_sf[SVM_CAMERAS_NUM] = {};

        CONTOUR     contour;
        SVMPATTERN  svm_pattern;
        ARRANGEMENT arrangement;
    };
#endif // USE_SVM


#ifdef USE_AICAM
    struct M_MENU_AICAM
    {
        int selected_camIdx = 0;
        // roi_pts[cam][level][point]
        std::array<std::array<std::array<XY, AI_CAM_ROI_PTS_NUM>, AI_CAM_ROI_LEVEL_MAX_NUM>, AI_CAM_NUM> roi_pts = {};
    };

    struct M_MENU_TAXI
    {
        std::array<bool, AI_CAM_NUM> cam_enabled    = { true, true, true, true };
        int                          primary_cam_idx = 0;
        int                          pip_dock        = 2;   // PiPDock::BOTTOM_LEFT
        int                          pip_orientation = 0;   // Orientation::HORIZONTAL
    };
#endif // USE_AICAM


/*--------------------------------------------------------------------------
 * AppConfiguration
 *--------------------------------------------------------------------------*/
class AppConfiguration
{
public:
    // ── System / Network ──────────────────────────────────────────────────
    int                system_language          = Language_English;
    SystemTime         system_time;
    int                system_calib_vehicle_idx = 0;
    std::array<int, 6> system_calib_pw          = {};
    int                system_display_timeout   = 0;   // seconds; 0 = always on

    int network_ip[12] = {};
    int network_pw[10] = {};
    int network_mode   = NetworkMode_Server;

    // ── Feature menu data ─────────────────────────────────────────────────
    #ifdef USE_DVR
        M_MENU_DVR   menu_dvr;
    #endif
    #ifdef USE_SVM
        M_MENU_SVM   menu_svm;
        M_MENU_CALIB menu_calib;
    #endif
    #ifdef USE_AICAM
        M_MENU_AICAM menu_aicam;
        M_MENU_TAXI  menu_taxi;
    #endif

    // ── JSON I/O ──────────────────────────────────────────────────────────
    bool readConfig(const std::string& filename = app_conf_file);
    bool writeConfig(const std::string& filename = app_conf_file, bool readSvmXml = false);
    void printConfig() const;

    // ── Initializers ──────────────────────────────────────────────────────
    #ifdef USE_AICAM
        void initAiCamConfig();
        void initTaxiConfig();
    #endif
    #ifdef USE_DVR
        void initDvrConfig();
        bool syncDvrConfig(DvrConfig& dvrConf);
    #endif

    // ── XML (SVM only) ────────────────────────────────────────────────────
    #ifdef USE_SVM
        bool TryReadSvmMenuInfoFromXml(std::string file_path, M_MENU_SVM& svmMenuData);
        bool UpdateSvmMenuInfoXmlFile(std::string file_path, M_MENU_SVM& svmMenuData);
        bool TryReadCalibMenuInfoFromXml(std::string file_path, M_MENU_CALIB& calibMenuData);
        bool UpdateCalibMenuInfoXmlFile(std::string file_path, M_MENU_CALIB& calibMenuData);

        int  getCodeFromName(const char* name);
        void convertStringToSvmMenuDatatype(char* tag, int code, char* src, M_MENU_SVM& svmMenuData);
        void convertStringToCalibMenuDatatype(char* tag, int code, char* src, M_MENU_CALIB& calibMenuData);
        void UpdateSvmXmlCache(xmlDocPtr xml_doc_ptr, M_MENU_SVM& svmMenuData, int camID);
        void UpdateCalibXmlCache(xmlDocPtr xml_doc_ptr, M_MENU_CALIB& calibMenuData);
    #endif
};

#endif // APP_CONF_H
