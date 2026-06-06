/*==========================================================================
 * app_conf.cpp
 *==========================================================================*/
#include "app_conf.h"
#include "app_vars.h"

// ============================================================================
// JSON read
// ============================================================================

bool AppConfiguration::readConfig(const std::string& filename)
{
    std::ifstream file(filename);
    if (file.is_open() == false)
    {
        LOG_APP_WARNINGF("AppConf: cannot open '%s'", filename.c_str());
        return false;
    }

    ordered_json root;
    try   { file >> root; }
    catch (const nlohmann::json::parse_error& e)
    {
        LOG_APP_ERRORF("AppConf: parse error in '%s': %s", filename.c_str(), e.what());
        return false;
    }
    file.close();

    if (root.contains("CameraMonitoringSystem") == false)
    {
        LOG_APP_ERRORF("AppConf: missing root key in '%s'", filename.c_str());
        return false;
    }
    const auto& cms = root["CameraMonitoringSystem"];

    // ── AVM (SVM) ─────────────────────────────────────────────────────────
    #ifdef USE_SVM
        if (cms.contains("AVM") == false) return false;
        const auto& avm = cms["AVM"];

        if (avm.contains("activation") == true && avm["activation"].is_array() == true)
        {
            const auto& v = avm["activation"];
            for (int i = 0; i < (int)v.size() && i < (int)menu_svm.activation.size(); i++)
                menu_svm.activation[i] = v[i].get<bool>();
        }
        if (avm.contains("view_setting") == true && avm["view_setting"].is_array() == true)
        {
            const auto& v = avm["view_setting"];
            for (int i = 0; i < (int)v.size() && i < (int)menu_svm.view_setting.size(); i++)
                menu_svm.view_setting[i] = v[i].get<int>();
        }
        if (avm.contains("vcam") == true && avm["vcam"].is_array() == true)
        {
            const auto& cams = avm["vcam"];
            for (int i = 0; i < (int)cams.size() && i < (int)menu_svm.vcam.size(); i++)
                for (int j = 0; j < (int)cams[i].size() && j < (int)menu_svm.vcam[i].size(); j++)
                    menu_svm.vcam[i][j] = cams[i][j].get<float>();
        }
        if (avm.contains("rcam") == true && avm["rcam"].is_array() == true)
        {
            const auto& cams = avm["rcam"];
            for (int i = 0; i < (int)cams.size() && i < (int)menu_svm.rcam.size(); i++)
                for (int j = 0; j < (int)cams[i].size() && j < (int)menu_svm.rcam[i].size(); j++)
                    menu_svm.rcam[i][j] = cams[i][j].get<int>();
        }
    #endif // USE_SVM

    // ── AiCam ─────────────────────────────────────────────────────────────
    #ifdef USE_AICAM
        if (cms.contains("AiCam") == false) return false;
        const auto& aicam = cms["AiCam"];

        if (aicam.contains("selected_camIdx") == true)
            menu_aicam.selected_camIdx = aicam["selected_camIdx"].get<int>();

        if (aicam.contains("roi_pts") == true && aicam["roi_pts"].is_array() == true)
        {
            const auto& cams = aicam["roi_pts"];
            for (int i = 0; i < (int)cams.size() && i < AI_CAM_NUM; i++)
                for (int j = 0; j < (int)cams[i].size() && j < AI_CAM_ROI_LEVEL_MAX_NUM; j++)
                    for (int k = 0; k < (int)cams[i][j].size() && k < AI_CAM_ROI_PTS_NUM; k++)
                    {
                        menu_aicam.roi_pts[i][j][k].x = cams[i][j][k][0].get<int>();
                        menu_aicam.roi_pts[i][j][k].y = cams[i][j][k][1].get<int>();
                    }
        }

        if (cms.contains("Taxi") == true)
        {
            const auto& taxi = cms["Taxi"];
            if (taxi.contains("primary_cam_idx") == true)
                menu_taxi.primary_cam_idx = taxi["primary_cam_idx"].get<int>();
            if (taxi.contains("pip_dock") == true)
                menu_taxi.pip_dock        = taxi["pip_dock"].get<int>();
            if (taxi.contains("pip_orientation") == true)
                menu_taxi.pip_orientation = taxi["pip_orientation"].get<int>();
            if (taxi.contains("cam_enabled") == true && taxi["cam_enabled"].is_array() == true)
            {
                const auto& v = taxi["cam_enabled"];
                for (int i = 0; i < (int)v.size() && i < AI_CAM_NUM; i++)
                    menu_taxi.cam_enabled[i] = v[i].get<bool>();
            }
        }
    #endif // USE_AICAM

    // ── DVR ───────────────────────────────────────────────────────────────
    #ifdef USE_DVR
        if (cms.contains("DVR") == false) return false;
        const auto& dvr = cms["DVR"];

        if (dvr.contains("recording_time")         == true)
            menu_dvr.recording_time                = dvr["recording_time"].get<int>();
        if (dvr.contains("recording_channel")      == true)
            menu_dvr.recording_channel             = dvr["recording_channel"].get<int>();
        if (dvr.contains("recording_fps_avm")      == true)
            menu_dvr.recording_fps_avm             = dvr["recording_fps_avm"].get<int>();
        if (dvr.contains("recording_fps_internal") == true)
            menu_dvr.recording_fps_internal        = dvr["recording_fps_internal"].get<int>();
        if (dvr.contains("audio_enabled")          == true)
            menu_dvr.audio_enabled                 = dvr["audio_enabled"].get<bool>();
    #endif // USE_DVR

    // ── System ────────────────────────────────────────────────────────────
    if (cms.contains("System") == false) return false;
    const auto& sys = cms["System"];

    if (sys.contains("language") == true)
        system_language = sys["language"].get<int>();
    if (sys.contains("system_display_timeout") == true)
        system_display_timeout = sys["system_display_timeout"].get<int>();
    if (sys.contains("calib_vehicle_idx") == true)
        system_calib_vehicle_idx = sys["calib_vehicle_idx"].get<int>();
    if (sys.contains("calib_pw") == true && sys["calib_pw"].is_array() == true)
    {
        const auto& pw = sys["calib_pw"];
        for (int i = 0; i < (int)pw.size() && i < (int)system_calib_pw.size(); i++)
            system_calib_pw[i] = pw[i].get<int>();
    }
    if (sys.contains("system_time") == true)
    {
        const auto& t = sys["system_time"];
        if (t.contains("year") == true)      system_time.year      = t["year"].get<int>();
        if (t.contains("month") == true)     system_time.month     = t["month"].get<int>();
        if (t.contains("day") == true)       system_time.day       = t["day"].get<int>();
        if (t.contains("hour") == true)      system_time.hour      = t["hour"].get<int>();
        if (t.contains("minute") == true)    system_time.minute    = t["minute"].get<int>();
        if (t.contains("second") == true)    system_time.second    = t["second"].get<int>();
        if (t.contains("auto_time") == true) system_time.auto_time = t["auto_time"].get<bool>();
    }
    // Legacy: migrate old timezone array
    else if (sys.contains("timezone") == true && sys["timezone"].is_array() == true)
    {
        system_time.hour = std::max(0, std::min(23, sys["timezone"][0].get<int>()));
    }

    LOG_APP_INFOF("AppConf: loaded '%s'", filename.c_str());
    return true;
}

// ============================================================================
// JSON write
// ============================================================================

bool AppConfiguration::writeConfig(const std::string& filename, bool readSvmXml)
{
    ordered_json root;
    
    // Read existing config if it exists
    std::ifstream inFile(filename);
    if (inFile.is_open() == true)
    {
        try
        {
            inFile >> root;
        }
        catch (...)
        {
            root = ordered_json::object(); // If parse fails, start fresh
        }
        inFile.close();
    }
    
    // Ensure root structure exists
    if (root.contains("CameraMonitoringSystem") == false) root["CameraMonitoringSystem"] = ordered_json::object();
    auto& cms = root["CameraMonitoringSystem"];

    // ── AVM (SVM) ─────────────────────────────────────────────────────────
    #ifdef USE_SVM
        if (readSvmXml == true)
        {
            TryReadSvmMenuInfoFromXml(
                std::string(_SETTINGS_PATH_) + "/settings_view.xml",   menu_svm);
            TryReadSvmMenuInfoFromXml(
                std::string(_SETTINGS_PATH_) + "/settings_common.xml", menu_svm);
        }
        auto& avm = cms["AVM"];
        avm["activation"]   = menu_svm.activation;
        avm["view_setting"] = menu_svm.view_setting;
        avm["vcam"]         = menu_svm.vcam;
        avm["rcam"]         = menu_svm.rcam;
    #else
        // If AVM block exists but we're not using SVM, preserve it
        if (cms.contains("AVM") == true) LOG_APP_INFOF("AppConf: preserving existing AVM config (USE_SVM not defined)");
    #endif

    // ── AiCam ─────────────────────────────────────────────────────────────
    #ifdef USE_AICAM
        auto& aicam = cms["AiCam"];
        aicam["selected_camIdx"] = menu_aicam.selected_camIdx;
        for (int i = 0; i < AI_CAM_NUM; i++)
            for (int j = 0; j < AI_CAM_ROI_LEVEL_MAX_NUM; j++)
                for (int k = 0; k < AI_CAM_ROI_PTS_NUM; k++)
                    aicam["roi_pts"][i][j][k] = ordered_json::array({
                        menu_aicam.roi_pts[i][j][k].x,
                        menu_aicam.roi_pts[i][j][k].y });

        auto& taxi              = cms["Taxi"];
        taxi["cam_enabled"]     = menu_taxi.cam_enabled;
        taxi["primary_cam_idx"] = menu_taxi.primary_cam_idx;
        taxi["pip_dock"]        = menu_taxi.pip_dock;
        taxi["pip_orientation"] = menu_taxi.pip_orientation;
    #else
        if (cms.contains("AiCam") == true) LOG_APP_INFOF("AppConf: preserving existing AiCam config (USE_AICAM not defined)");
        if (cms.contains("Taxi")  == true) LOG_APP_INFOF("AppConf: preserving existing Taxi config (USE_AICAM not defined)");
    #endif

    // ── DVR ───────────────────────────────────────────────────────────────
    #ifdef USE_DVR
        cms["DVR"] = ordered_json{
            { "recording_time",         menu_dvr.recording_time         },
            { "recording_channel",      menu_dvr.recording_channel      },
            { "recording_fps_avm",      menu_dvr.recording_fps_avm      },
            { "recording_fps_internal", menu_dvr.recording_fps_internal },
            { "audio_enabled",          menu_dvr.audio_enabled          },
        };
    #else
        if (cms.contains("DVR") == true) LOG_APP_INFOF("AppConf: preserving existing DVR config (USE_DVR not defined)");
    #endif

    // ── System (always updated) ────────────────────────────────────────────
    auto& sys                     = cms["System"];
    sys["language"]               = system_language;
    sys["system_display_timeout"] = system_display_timeout;
    sys["system_time"] = ordered_json{
        { "year",      system_time.year      },
        { "month",     system_time.month     },
        { "day",       system_time.day       },
        { "hour",      system_time.hour      },
        { "minute",    system_time.minute    },
        { "second",    system_time.second    },
        { "auto_time", system_time.auto_time },
    };
    sys["calib_vehicle_idx"] = system_calib_vehicle_idx;
    sys["calib_pw"]          = system_calib_pw;

    std::ofstream ofs(filename);
    if (ofs.is_open() == false)
    {
        LOG_APP_ERRORF("AppConf: cannot write '%s'", filename.c_str());
        return false;
    }
    ofs << root.dump(4);
    ofs.close();
    LOG_APP_INFOF("AppConf: saved '%s'", filename.c_str());
    return true;
}

// ============================================================================
// Print
// ============================================================================

void AppConfiguration::printConfig() const
{
    auto box = LOG::Logger::createBox(60);
    box.setTitle("App Configuration");

    box.addLine("System");
    box.addLinef("  Language       : %s",
                 system_language == Language_English ? "English" : "Korean");
    box.addLinef("  Time           : %04d-%02d-%02d %02d:%02d:%02d%s",
                 system_time.year, system_time.month,  system_time.day,
                 system_time.hour, system_time.minute, system_time.second,
                 system_time.auto_time ? " (auto)" : "");
    box.addLinef("  Calib Vehicle  : %d", system_calib_vehicle_idx);

    #ifdef USE_DVR
        box.addSeparator();
        box.addLine("DVR");
        box.addLinef("  Rec Time       : %d s", menu_dvr.recording_time);
        box.addLinef("  Channels       : %d",   menu_dvr.recording_channel);
        box.addLinef("  FPS AVM        : %d",   menu_dvr.recording_fps_avm);
        box.addLinef("  FPS Internal   : %d",   menu_dvr.recording_fps_internal);
        box.addLinef("  Audio          : %s",   menu_dvr.audio_enabled ? "Enabled" : "Disabled");
    #endif // USE_DVR

    #ifdef USE_AICAM
        box.addSeparator();
        box.addLine("AiCam");
        box.addLinef("  Selected Cam   : %d", menu_aicam.selected_camIdx);
        box.addSeparator();
        box.addLine("Taxi PiP");
        box.addLinef("  Primary Cam    : %d", menu_taxi.primary_cam_idx);
        box.addLinef("  Dock           : %d", menu_taxi.pip_dock);
        box.addLinef("  Orientation    : %s",
                     menu_taxi.pip_orientation == 0 ? "Horizontal" : "Vertical");
        for (int i = 0; i < AI_CAM_NUM; i++)
            box.addLinef("  Cam[%d] Enabled : %s", i,
                         menu_taxi.cam_enabled[i] == true ? "yes" : "no");
    #endif // USE_AICAM

    box.render();
}

// ============================================================================
// Initializers
// ============================================================================

#ifdef USE_AICAM
    void AppConfiguration::initAiCamConfig()
    {
        constexpr int EDGE = 30;   // match HIT_RADIUS

        for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
        {
            for (int lvIdx = 0; lvIdx < AI_CAM_ROI_LEVEL_MAX_NUM; lvIdx++)
            {
                const float num = static_cast<float>(AI_CAM_ROI_LEVEL_MAX_NUM + 1);
                menu_aicam.roi_pts[camIdx][lvIdx][0] = {
                    EDGE,             WND_HEIGHT - EDGE };
                menu_aicam.roi_pts[camIdx][lvIdx][1] = {
                    (float)(WND_WIDTH  * (AI_CAM_ROI_LEVEL_MAX_NUM + 1 - (lvIdx + 1)) / (2.0f * num)),
                    (float)(WND_HEIGHT * (AI_CAM_ROI_LEVEL_MAX_NUM - lvIdx)            / (2.0f * num)) };
                menu_aicam.roi_pts[camIdx][lvIdx][2] = {
                    (float)(WND_WIDTH  * (AI_CAM_ROI_LEVEL_MAX_NUM + 1 + (lvIdx + 1)) / (2.0f * num)),
                    (float)(WND_HEIGHT * (AI_CAM_ROI_LEVEL_MAX_NUM - lvIdx)            / (2.0f * num)) };
                menu_aicam.roi_pts[camIdx][lvIdx][3] = {
                    WND_WIDTH - EDGE, WND_HEIGHT - EDGE };
            }
        }
    }

    void AppConfiguration::initTaxiConfig()
    {
        menu_taxi.cam_enabled.fill(true);
        menu_taxi.primary_cam_idx = 0;
        menu_taxi.pip_dock        = 2;   // PiPDock::BOTTOM_LEFT
        menu_taxi.pip_orientation = 0;   // Orientation::HORIZONTAL
    }
#endif // USE_AICAM


#ifdef USE_DVR
    void AppConfiguration::initDvrConfig()
    {
        menu_dvr.recording_time         = 60;
        menu_dvr.recording_channel      = 4;
        menu_dvr.recording_fps_avm      = 30;
        menu_dvr.recording_fps_internal = 30;
        menu_dvr.audio_enabled          = true;
    }

    bool AppConfiguration::syncDvrConfig(DvrConfig& dvrConf)
    {
        bool changed = false;
        auto sync = [&](auto& dst, const auto& src)
            { if (dst != src) { dst = src; changed = true; } };
        sync(dvrConf.recordingTime,        menu_dvr.recording_time);
        sync(dvrConf.recordingChannel,     menu_dvr.recording_channel);
        sync(dvrConf.recordingFpsAvm,      menu_dvr.recording_fps_avm);
        sync(dvrConf.recordingFpsInternal, menu_dvr.recording_fps_internal);
        sync(dvrConf.audioEnabled,         menu_dvr.audio_enabled);
        return changed;
    }
#endif // USE_DVR


// ============================================================================
// XML — SVM only
// ============================================================================

#ifdef USE_SVM

// ── Static helpers ─────────────────────────────────────────────────────────

static inline std::string pack_message(char* tag, char* src)
{
    return "<" + std::string(tag) + ">" + std::string(src) + "</" + std::string(tag) + ">";
}

static void PRE_PROCESSING(std::stringstream& ss, char* src)
{
    char tmp[5000] = {};
    std::string(src).copy(tmp, strlen(src), 0);
    std::string s(tmp);
    std::replace_if(s.begin(), s.end(),
        [](char c){ return c == '\t' || c == '\n' || c == '\a'; }, ' ');
    ss.str(s);
    ss.exceptions(std::stringstream::failbit | std::stringstream::badbit);
}

static void POST_PROCESSING(std::stringstream& ss, char* src, char* tag)
{
    if (ss.eof() == false)
    {
        std::string dummy;
        std::getline(ss, dummy);
        dummy.erase(std::remove_if(dummy.begin(), dummy.end(), ::isspace), dummy.end());
        if (dummy.length() > 0) throw std::runtime_error(pack_message(tag, src));
    }
}

// Set XML node content; on libxml error free doc, cleanup, and throw.
static void xmlSetChecked(xmlNodePtr node, const std::string& val, xmlDocPtr doc)
{
    xmlNodeSetContent(node, (xmlChar*)val.c_str());
    if (const xmlError* err = xmlGetLastError())
    {
        std::string msg = err->message;
        xmlFreeDoc(doc);
        xmlCleanupParser();
        throw std::runtime_error(msg);
    }
}

// Walk root > LEVEL2 > LEVEL3 and call fn(name, content) for each element node.
template<typename Fn>
static void xmlWalkLevel3(xmlDocPtr doc, Fn fn)
{
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root == nullptr) return;
    for (xmlNodePtr n2 = root->children; n2 != nullptr; n2 = n2->next)
    {
        if (n2->type != XML_ELEMENT_NODE) continue;
        for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
        {
            if (n3->type != XML_ELEMENT_NODE) continue;
            xmlChar* content = xmlNodeGetContent(n3);
            if (content == nullptr) continue;
            try { fn((char*)n3->name, (char*)content); }
            catch (const std::exception& e)
            {
                xmlFree(content);
                xmlFreeDoc(doc);
                xmlCleanupParser();
                throw std::runtime_error(std::string(e.what()));
            }
            xmlFree(content);
        }
    }
}

// ── getCodeFromName ──────────────────────────────────────────────────────────

int AppConfiguration::getCodeFromName(const char* name)
{
    // SVM – real cameras (intrinsic)
    if      (strcmp(name, "rcam0") == 0) return 10200;
    else if (strcmp(name, "rcam1") == 0) return 10201;
    else if (strcmp(name, "rcam2") == 0) return 10202;
    else if (strcmp(name, "rcam3") == 0) return 10203;
    // SVM – virtual cameras (pose)
    else if (strcmp(name, "vcam0") == 0) return 10300;
    else if (strcmp(name, "vcam1") == 0) return 10301;
    else if (strcmp(name, "vcam2") == 0) return 10302;
    else if (strcmp(name, "vcam3") == 0) return 10303;
    // SVM – misc
    else if (strcmp(name, "view_setting") == 0) return 10520;
    else if (strcmp(name, "contour")      == 0) return 10600;
    else if (strcmp(name, "activation")   == 0) return 10900;
    // Calib – model
    else if (strcmp(name, "model_file")        == 0) return 10700;
    else if (strcmp(name, "model_translation") == 0) return 10720;
    else if (strcmp(name, "model_scale")       == 0) return 10730;
    else if (strcmp(name, "model_rotation")    == 0) return 10740;
    // Calib – vehicle
    else if (strcmp(name, "vehicle_information")   == 0) return 10800;
    else if (strcmp(name, "vehicle_specification") == 0) return 10810;
    // Calib – system params
    else if (strcmp(name, "patterns_number")  == 0) return 10120;
    else if (strcmp(name, "cameras_number")   == 0) return 10130;
    else if (strcmp(name, "calibration_type") == 0) return 10140;
    // Calib – pattern / arrangement
    else if (strcmp(name, "unit_space")                == 0) return 11000;
    else if (strcmp(name, "pattern_offset_from_car")   == 0) return 11100;
    else if (strcmp(name, "distance_between_patterns") == 0) return 11110;
    return 99999;
}

// ── convertStringToSvmMenuDatatype ──────────────────────────────────────────

void AppConfiguration::convertStringToSvmMenuDatatype(
    char* tag, int code, char* src, M_MENU_SVM& d)
{
    std::string temp;
    switch (code)
    {
        case 10200: case 10201: case 10202: case 10203:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> temp >> temp; // brightness, flipx
            ss >> d.rcam[code - 10200][0]   // hleft
               >> d.rcam[code - 10200][1]   // hright
               >> d.rcam[code - 10200][2]   // vtop
               >> d.rcam[code - 10200][3];  // vbot
            ss >> temp >> temp >> temp;     // sf, cx, cy
            for (int i = 0; i < AFFINE_COEF_NUM;       i++) ss >> temp;
            for (int i = 0; i < INV_POLY_COEF_MAX_NUM; i++) ss >> temp;
            for (int i = 0; i < POLY_COEF_MAX_NUM;     i++) ss >> temp;
            POST_PROCESSING(ss, src, tag);
            break;
        }
        case 10300: case 10301: case 10302: case 10303:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            int idx = code - 10300;
            ss >> d.vcam[idx][0] >> d.vcam[idx][1] >> d.vcam[idx][2]
               >> d.vcam[idx][3] >> d.vcam[idx][4] >> d.vcam[idx][5];
            POST_PROCESSING(ss, src, tag);
            break;
        }
        case 10520:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            int vs[5] = {};
            ss >> vs[0] >> vs[2] >> vs[3] >> vs[1] >> vs[4];
            d.view_setting[0] = (int)((VIEWMODE)vs[0] == VIEWMODE::CAMVIEW3D_FRONT);
            d.view_setting[1] = (int)((VIEWMODE)vs[1] == VIEWMODE::CAMVIEW3D_RIGHT);
            d.view_setting[2] = (int)((VIEWMODE)vs[2] == VIEWMODE::CAMVIEW3D_REAR);
            d.view_setting[3] = (int)((VIEWMODE)vs[3] == VIEWMODE::CAMVIEW3D_LEFT);
            d.view_setting[4] = (int)((VIEWMODE)vs[4] == VIEWMODE::CAMVIEW3D_REAR);
            POST_PROCESSING(ss, src, tag);
            break;
        }
        case 10900:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> temp >> temp >> temp; // scene_animation, model_animation, model_transparency
            ss >> d.activation[0] >> d.activation[1] >> d.activation[2]
               >> d.activation[3] >> d.activation[4];
            POST_PROCESSING(ss, src, tag);
            break;
        }
        default: break;
    }
}

// ── convertStringToCalibMenuDatatype ────────────────────────────────────────

void AppConfiguration::convertStringToCalibMenuDatatype(
    char* tag, int code, char* src, M_MENU_CALIB& d)
{
    std::string temp;
    switch (code)
    {
        case 10200: case 10201: case 10202: case 10203:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> temp >> temp;                     // brightness, flipx
            ss >> temp >> temp >> temp >> temp;     // hleft, hright, vtop, vbot
            ss >> d.svm_cameras_sf[code - 10200];  // sf
            ss >> temp >> temp;                     // cx, cy
            for (int i = 0; i < AFFINE_COEF_NUM;       i++) ss >> temp;
            for (int i = 0; i < INV_POLY_COEF_MAX_NUM; i++) ss >> temp;
            for (int i = 0; i < POLY_COEF_MAX_NUM;     i++) ss >> temp;
            POST_PROCESSING(ss, src, tag);
            break;
        }
        case 10700: d.model.model_file = std::string(src); break;
        case 10720:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.model.translation.x >> d.model.translation.y >> d.model.translation.z;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10730:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.model.scale.x >> d.model.scale.y >> d.model.scale.z;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10740:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.model.rotation.x >> d.model.rotation.y >> d.model.rotation.z;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10800:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.veh_info.vehicle_id  >> d.veh_info.customer     >> d.veh_info.manufacturer
               >> d.veh_info.model_name  >> d.veh_info.model_year   >> d.veh_info.vehicle_type;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10810:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.veh_spec.length           >> d.veh_spec.width           >> d.veh_spec.height
               >> d.veh_spec.front_overhang   >> d.veh_spec.wheel_base      >> d.veh_spec.rear_overhang
               >> d.veh_spec.front_track      >> d.veh_spec.rear_track
               >> d.veh_spec.min_steering_angle >> d.veh_spec.max_steering_angle;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10120:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.svm_patterns_num;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10130:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.svm_cameras_num;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10140:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.svm_calibration_type;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 11000:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.svm_pattern.unit_space;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 10600:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.contour.roi_start_x >> d.contour.roi_start_y
               >> d.contour.roi_width   >> d.contour.roi_height >> d.contour.contour_max_area;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 11100:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.arrangement.pattern_offset_from_car.front_pattern_offset
               >> d.arrangement.pattern_offset_from_car.right_pattern_offset
               >> d.arrangement.pattern_offset_from_car.rear_pattern_offset
               >> d.arrangement.pattern_offset_from_car.left_pattern_offset;
            POST_PROCESSING(ss, src, tag); break;
        }
        case 11110:
        {
            std::stringstream ss; PRE_PROCESSING(ss, src);
            ss >> d.arrangement.distance_between_patterns.vertical_1st_distance
               >> d.arrangement.distance_between_patterns.vertical_2nd_distance
               >> d.arrangement.distance_between_patterns.vertical_3rd_distance;
            POST_PROCESSING(ss, src, tag); break;
        }
        default: break;
    }
}

// ── TryReadSvmMenuInfoFromXml ────────────────────────────────────────────────

bool AppConfiguration::TryReadSvmMenuInfoFromXml(std::string file_path, M_MENU_SVM& svmMenuData)
{
    xmlResetLastError();
    xmlDocPtr doc = xmlReadFile(file_path.c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (doc == nullptr)
    {
        LOG_APP_WARNINGF("AppConf: failed to parse SVM XML '%s'", file_path.c_str());
        return false;
    }
    xmlWalkLevel3(doc, [&](char* name, char* content)
    {
        convertStringToSvmMenuDatatype(name, getCodeFromName(name), content, svmMenuData);
    });
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return true;
}

// ── UpdateSvmMenuInfoXmlFile ─────────────────────────────────────────────────

bool AppConfiguration::UpdateSvmMenuInfoXmlFile(std::string file_path, M_MENU_SVM& svmMenuData)
{
    xmlResetLastError();
    xmlDocPtr doc = xmlReadFile(file_path.c_str(), nullptr, 0);
    if (doc == nullptr)
    {
        xmlCleanupParser();
        throw std::runtime_error(std::string(xmlGetLastError()->message));
    }
    for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
        UpdateSvmXmlCache(doc, svmMenuData, camID);
    xmlSaveFormatFileEnc(file_path.c_str(), doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return true;
}

// ── UpdateSvmXmlCache ────────────────────────────────────────────────────────

void AppConfiguration::UpdateSvmXmlCache(xmlDocPtr doc, M_MENU_SVM& d, int camID)
{
    const std::string camName = "rcam" + std::to_string(camID);
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root == nullptr)
    {
        xmlFreeDoc(doc); xmlCleanupParser();
        throw std::runtime_error(std::string(xmlGetLastError()->message));
    }
    for (xmlNodePtr n2 = root->children; n2 != nullptr; n2 = n2->next)
    {
        if (n2->type != XML_ELEMENT_NODE) continue;
        if (strcmp((char*)n2->name, "real_camera") != 0) continue;
        for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
        {
            if (n3->type != XML_ELEMENT_NODE) continue;
            if (strcmp((char*)n3->name, camName.c_str()) != 0) continue;
            for (xmlNodePtr n4 = n3->children; n4 != nullptr; n4 = n4->next)
            {
                if (n4->type != XML_ELEMENT_NODE) continue;
                const char* nm = (char*)n4->name;
                std::string val;
                if      (strcmp(nm, "hleft")  == 0) val = std::to_string(d.rcam[camID][0]);
                else if (strcmp(nm, "hright") == 0) val = std::to_string(d.rcam[camID][1]);
                else if (strcmp(nm, "vtop")   == 0) val = std::to_string(d.rcam[camID][2]);
                else if (strcmp(nm, "vbot")   == 0) val = std::to_string(d.rcam[camID][3]);
                else continue;
                xmlSetChecked(n4, val, doc);
            }
        }
    }
}

// ── TryReadCalibMenuInfoFromXml ──────────────────────────────────────────────

bool AppConfiguration::TryReadCalibMenuInfoFromXml(std::string file_path, M_MENU_CALIB& calibMenuData)
{
    xmlResetLastError();
    xmlDocPtr doc = xmlReadFile(file_path.c_str(), nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (doc == nullptr)
    {
        LOG_APP_WARNINGF("AppConf: failed to parse Calib XML '%s'", file_path.c_str());
        return false;
    }
    xmlWalkLevel3(doc, [&](char* name, char* content)
    {
        convertStringToCalibMenuDatatype(name, getCodeFromName(name), content, calibMenuData);
    });
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return true;
}

// ── UpdateCalibMenuInfoXmlFile ───────────────────────────────────────────────

bool AppConfiguration::UpdateCalibMenuInfoXmlFile(std::string file_path, M_MENU_CALIB& calibMenuData)
{
    xmlResetLastError();
    xmlDocPtr doc = xmlReadFile(file_path.c_str(), nullptr, 0);
    if (doc == nullptr)
    {
        xmlCleanupParser();
        throw std::runtime_error(std::string(xmlGetLastError()->message));
    }
    UpdateCalibXmlCache(doc, calibMenuData);
    xmlSaveFormatFileEnc(file_path.c_str(), doc, "UTF-8", 1);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return true;
}

// ── UpdateCalibXmlCache ──────────────────────────────────────────────────────

void AppConfiguration::UpdateCalibXmlCache(xmlDocPtr doc, M_MENU_CALIB& d)
{
    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (root == nullptr)
    {
        xmlFreeDoc(doc); xmlCleanupParser();
        throw std::runtime_error(std::string(xmlGetLastError()->message));
    }
    for (xmlNodePtr n2 = root->children; n2 != nullptr; n2 = n2->next)
    {
        if (n2->type != XML_ELEMENT_NODE) continue;
        const char* n2name = (char*)n2->name;

        if (strcmp(n2name, "real_vehicle") == 0)
        {
            for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
            {
                if (n3->type != XML_ELEMENT_NODE) continue;
                if (strcmp((char*)n3->name, "vehicle_specification") != 0) continue;
                for (xmlNodePtr n4 = n3->children; n4 != nullptr; n4 = n4->next)
                {
                    if (n4->type != XML_ELEMENT_NODE) continue;
                    const char* nm = (char*)n4->name;
                    std::string val;
                    if      (strcmp(nm, "length")             == 0) val = std::to_string(d.veh_spec.length);
                    else if (strcmp(nm, "width")              == 0) val = std::to_string(d.veh_spec.width);
                    else if (strcmp(nm, "height")             == 0) val = std::to_string(d.veh_spec.height);
                    else if (strcmp(nm, "front_overhang")     == 0) val = std::to_string(d.veh_spec.front_overhang);
                    else if (strcmp(nm, "wheel_base")         == 0) val = std::to_string(d.veh_spec.wheel_base);
                    else if (strcmp(nm, "rear_overhang")      == 0) val = std::to_string(d.veh_spec.rear_overhang);
                    else if (strcmp(nm, "front_track")        == 0) val = std::to_string(d.veh_spec.front_track);
                    else if (strcmp(nm, "rear_track")         == 0) val = std::to_string(d.veh_spec.rear_track);
                    else if (strcmp(nm, "min_steering_angle") == 0) val = std::to_string(d.veh_spec.min_steering_angle);
                    else if (strcmp(nm, "max_steering_angle") == 0) val = std::to_string(d.veh_spec.max_steering_angle);
                    else continue;
                    xmlSetChecked(n4, val, doc);
                }
            }
        }
        else if (strcmp(n2name, "system_parameter") == 0)
        {
            for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
            {
                if (n3->type != XML_ELEMENT_NODE) continue;
                const char* nm = (char*)n3->name;
                std::string val;
                if      (strcmp(nm, "patterns_number") == 0) val = std::to_string(d.svm_patterns_num);
                else if (strcmp(nm, "cameras_number")  == 0) val = std::to_string(d.svm_cameras_num);
                else continue;
                xmlSetChecked(n3, val, doc);
            }
        }
        else if (strcmp(n2name, "calibration_parameter") == 0)
        {
            for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
            {
                if (n3->type != XML_ELEMENT_NODE) continue;
                if (strcmp((char*)n3->name, "calibration_type") != 0) continue;
                xmlSetChecked(n3, std::to_string(d.svm_calibration_type), doc);
            }
        }
        else if (strcmp(n2name, "unit_pattern_dimension") == 0)
        {
            for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
            {
                if (n3->type != XML_ELEMENT_NODE) continue;
                if (strcmp((char*)n3->name, "unit_space") != 0) continue;
                xmlSetChecked(n3, std::to_string(d.svm_pattern.unit_space), doc);
            }
        }
        else if (strcmp(n2name, "image_processing") == 0)
        {
            for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
            {
                if (n3->type != XML_ELEMENT_NODE) continue;
                if (strcmp((char*)n3->name, "contour") != 0) continue;
                for (xmlNodePtr n4 = n3->children; n4 != nullptr; n4 = n4->next)
                {
                    if (n4->type != XML_ELEMENT_NODE) continue;
                    const char* nm = (char*)n4->name;
                    std::string val;
                    if      (strcmp(nm, "roi_start_x")      == 0) val = std::to_string(d.contour.roi_start_x);
                    else if (strcmp(nm, "roi_start_y")      == 0) val = std::to_string(d.contour.roi_start_y);
                    else if (strcmp(nm, "roi_width")        == 0) val = std::to_string(d.contour.roi_width);
                    else if (strcmp(nm, "roi_height")       == 0) val = std::to_string(d.contour.roi_height);
                    else if (strcmp(nm, "contour_max_area") == 0) val = std::to_string(d.contour.contour_max_area);
                    else continue;
                    xmlSetChecked(n4, val, doc);
                }
            }
        }
        else if (strcmp(n2name, "arrangement") == 0)
        {
            for (xmlNodePtr n3 = n2->children; n3 != nullptr; n3 = n3->next)
            {
                if (n3->type != XML_ELEMENT_NODE) continue;
                const char* n3name = (char*)n3->name;

                if (strcmp(n3name, "pattern_offset_from_car") == 0)
                {
                    for (xmlNodePtr n4 = n3->children; n4 != nullptr; n4 = n4->next)
                    {
                        if (n4->type != XML_ELEMENT_NODE) continue;
                        const char* nm = (char*)n4->name;
                        std::string val;
                        if      (strcmp(nm, "front_pattern_offset") == 0) val = std::to_string(d.arrangement.pattern_offset_from_car.front_pattern_offset);
                        else if (strcmp(nm, "right_pattern_offset") == 0) val = std::to_string(d.arrangement.pattern_offset_from_car.right_pattern_offset);
                        else if (strcmp(nm, "rear_pattern_offset")  == 0) val = std::to_string(d.arrangement.pattern_offset_from_car.rear_pattern_offset);
                        else if (strcmp(nm, "left_pattern_offset")  == 0) val = std::to_string(d.arrangement.pattern_offset_from_car.left_pattern_offset);
                        else continue;
                        xmlSetChecked(n4, val, doc);
                    }
                }
                else if (strcmp(n3name, "distance_between_patterns") == 0)
                {
                    for (xmlNodePtr n4 = n3->children; n4 != nullptr; n4 = n4->next)
                    {
                        if (n4->type != XML_ELEMENT_NODE) continue;
                        const char* nm = (char*)n4->name;
                        std::string val;
                        if      (strcmp(nm, "vertical_1st_distance") == 0) val = std::to_string(d.arrangement.distance_between_patterns.vertical_1st_distance);
                        else if (strcmp(nm, "vertical_2nd_distance") == 0) val = std::to_string(d.arrangement.distance_between_patterns.vertical_2nd_distance);
                        else if (strcmp(nm, "vertical_3rd_distance") == 0) val = std::to_string(d.arrangement.distance_between_patterns.vertical_3rd_distance);
                        else continue;
                        xmlSetChecked(n4, val, doc);
                    }
                }
            }
        }
    }
}

#endif // USE_SVM
