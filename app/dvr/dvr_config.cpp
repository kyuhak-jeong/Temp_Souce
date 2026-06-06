/*==========================================================================
 * dvr_config.cpp - DVR Configuration Implementation
 *==========================================================================*/
#include "dvr_config.hpp"

void DvrConfig::loadConfig()
{
    if (readConfig() == false) writeConfig();
    updateStoragePath();
}

bool DvrConfig::readConfig()
{
    std::string filename = dvr_conf_file;
    std::ifstream file(filename);
    
    if (file.is_open() == false)
    {
        LOG_DVR_WARNINGF("Failed to open: %s", filename.c_str());
        return false;
    }
    
    ordered_json root;
    try { file >> root; }
    catch (const nlohmann::json::parse_error& e)
    { LOG_DVR_ERRORF("Parse error: %s", e.what()); return false; }
    file.close();
    
    if (root.contains("CameraMonitoringSystem") == false) { return false; }
    const auto& cms = root["CameraMonitoringSystem"];

    // DVR_Link section
    if (cms.contains("DVR_Link") == true)
    {
        const auto& l = cms["DVR_Link"];
        if (l.contains("recordingTime") == true)        recordingTime        = l["recordingTime"].get<int>();
        if (l.contains("recordingChannel") == true)     recordingChannel     = l["recordingChannel"].get<int>();
        if (l.contains("recordingFpsAvm") == true)      recordingFpsAvm      = l["recordingFpsAvm"].get<int>();
        if (l.contains("recordingFpsInternal") == true) recordingFpsInternal = l["recordingFpsInternal"].get<int>();
    }

    // DVR_Internal section
    if (cms.contains("DVR_Internal") == true)
    {
        const auto& i = cms["DVR_Internal"];
        if (i.contains("storageDev") == true)               storageDev              = i["storageDev"].get<std::string>();
        if (i.contains("eventDirName") == true)             eventDirName            = i["eventDirName"].get<std::string>();
        if (i.contains("normalDirName") == true)            normalDirName           = i["normalDirName"].get<std::string>();
        if (i.contains("parkingDirName") == true)           parkingDirName          = i["parkingDirName"].get<std::string>();
        if (i.contains("maxSizeOfNormalFolder") == true)    maxSizeOfNormalFolder   = i["maxSizeOfNormalFolder"].get<int>()  * MB_TO_BYTE;
        if (i.contains("maxSizeOfEventFolder") == true)     maxSizeOfEventFolder    = i["maxSizeOfEventFolder"].get<int>()   * MB_TO_BYTE;
        if (i.contains("maxSizeOfParkingFolder") == true)   maxSizeOfParkingFolder  = i["maxSizeOfParkingFolder"].get<int>() * MB_TO_BYTE;
        if (i.contains("vibrationLevel") == true)           vibrationLevel          = i["vibrationLevel"].get<int>();
        if (i.contains("isParkingMode") == true)            isParkingMode           = i["isParkingMode"].get<bool>();
        if (i.contains("recordingTimePreEvent") == true)    recordingTimePreEvent   = i["recordingTimePreEvent"].get<int>();
        if (i.contains("recordingTimeTotalEvent") == true)  recordingTimeTotalEvent = i["recordingTimeTotalEvent"].get<int>();
        if (i.contains("recordingFpsParking") == true)      recordingFpsParking     = i["recordingFpsParking"].get<int>();
        if (i.contains("recordingWidth") == true)           recordingWidth          = i["recordingWidth"].get<int>();
        if (i.contains("recordingHeight") == true)          recordingHeight         = i["recordingHeight"].get<int>();
        if (i.contains("recordingWidthInternal") == true)   recordingWidthInternal  = i["recordingWidthInternal"].get<int>();
        if (i.contains("recordingHeightInternal") == true)  recordingHeightInternal = i["recordingHeightInternal"].get<int>();
    }
    
    // DVR_Audio section
    if (cms.contains("DVR_Audio") == true)
    {
        const auto& a = cms["DVR_Audio"];
        if (a.contains("audioEnabled") == true)    audioEnabled    = a["audioEnabled"].get<bool>();
        if (a.contains("audioDevice") == true)     audioDevice     = a["audioDevice"].get<std::string>();
        if (a.contains("audioSampleRate") == true) audioSampleRate = a["audioSampleRate"].get<int>();
        if (a.contains("audioChannels") == true)   audioChannels   = a["audioChannels"].get<int>();
        if (a.contains("audioBitDepth") == true)   audioBitDepth   = a["audioBitDepth"].get<int>();
        if (a.contains("audioBitrate") == true)    audioBitrate    = a["audioBitrate"].get<int>();
    }
    
    LOG_DVR_INFOF("Configuration loaded from: %s", filename.c_str());
    return true;
}

bool DvrConfig::writeConfig()
{
    ordered_json root;
    auto& cms = root["CameraMonitoringSystem"];

    // DVR_Link
    auto& l = cms["DVR_Link"];
    l["recordingTime"]        = recordingTime;
    l["recordingChannel"]     = recordingChannel;
    l["recordingFpsAvm"]      = recordingFpsAvm;
    l["recordingFpsInternal"] = recordingFpsInternal;

    // DVR_Internal
    auto& i = cms["DVR_Internal"];
    i["storageDev"]              = storageDev;
    i["eventDirName"]            = eventDirName;
    i["normalDirName"]           = normalDirName;
    i["parkingDirName"]          = parkingDirName;
    i["maxSizeOfNormalFolder"]   = static_cast<int>(maxSizeOfNormalFolder  / MB_TO_BYTE);
    i["maxSizeOfEventFolder"]    = static_cast<int>(maxSizeOfEventFolder   / MB_TO_BYTE);
    i["maxSizeOfParkingFolder"]  = static_cast<int>(maxSizeOfParkingFolder / MB_TO_BYTE);
    i["vibrationLevel"]          = vibrationLevel;
    i["isParkingMode"]           = isParkingMode;
    i["recordingTimePreEvent"]   = recordingTimePreEvent;
    i["recordingTimeTotalEvent"] = recordingTimeTotalEvent;
    i["recordingFpsParking"]     = recordingFpsParking;
    i["recordingWidth"]          = recordingWidth;
    i["recordingHeight"]         = recordingHeight;
    i["recordingWidthInternal"]  = recordingWidthInternal;
    i["recordingHeightInternal"] = recordingHeightInternal;
    
    // DVR_Audio
    auto& a = cms["DVR_Audio"];
    a["audioEnabled"]    = audioEnabled;
    a["audioDevice"]     = audioDevice;
    a["audioSampleRate"] = audioSampleRate;
    a["audioChannels"]   = audioChannels;
    a["audioBitDepth"]   = audioBitDepth;
    a["audioBitrate"]    = audioBitrate;
    
    std::string filename = "dvr_conf.json";
    std::ofstream file(filename);
    if (file.is_open() == false)
    { LOG_DVR_ERRORF("Failed to open for writing: %s", filename.c_str()); return false; }
    
    file << root.dump(4);
    file.close();
    LOG_DVR_INFOF("Configuration saved to: %s", filename.c_str());
    return true;
}

void DvrConfig::printConfig()
{
    auto box = LOG::Logger::createBox(65);
    box.setTitle("DVR Configuration");
    
    box.addLine("Storage");
    box.addLinef("  Device: %-53s", storageDev.c_str());
    box.addLinef("  Path  : %-53s", storageDevPath.c_str());
    box.addSeparator();
    
    box.addLine("Recording Modes          Normal      Event       Parking");
    char buf[128];
    std::snprintf(buf, sizeof(buf), "  Folder Name            %-11s %-11s %-11s",
                  normalDirName.c_str(), eventDirName.c_str(), parkingDirName.c_str());
    box.addLine(buf);
    std::snprintf(buf, sizeof(buf), "  Max Size (MB)          %-11ld %-11ld %-11ld",
                  maxSizeOfNormalFolder  / MB_TO_BYTE,
                  maxSizeOfEventFolder   / MB_TO_BYTE,
                  maxSizeOfParkingFolder / MB_TO_BYTE);
    box.addLine(buf);
    
    char eventTime[32];
    std::snprintf(eventTime, sizeof(eventTime), "%d+%d",
                  recordingTimePreEvent,
                  recordingTimeTotalEvent - recordingTimePreEvent);
    std::snprintf(buf, sizeof(buf), "  Recording Time (s)     %-11d %-11s %-11d",
                  recordingTime, eventTime, recordingTime);
    box.addLine(buf);
    box.addSeparator();
    
    box.addLine("FPS Settings             AVM         Internal    Parking");
    std::snprintf(buf, sizeof(buf), "  Frames Per Second      %-11d %-11d %-11d",
                  recordingFpsAvm, recordingFpsInternal, recordingFpsParking);
    box.addLine(buf);
    box.addSeparator();
    
    box.addLine("Resolution Settings      AVM         Internal");
    std::snprintf(buf, sizeof(buf), "  Source (camera)        %dx%-4d   %dx%d",
                  IMG_WIDTH, IMG_HEIGHT, IMG_WIDTH, IMG_HEIGHT);
    box.addLine(buf);
    std::snprintf(buf, sizeof(buf), "  Recording              %dx%-4d   %dx%d",
                  recordingWidth, recordingHeight, recordingWidthInternal, recordingHeightInternal);
    box.addLine(buf);
    box.addSeparator();
    
    box.addLine("Audio Settings");
    box.addLinef("  Enabled              : %s",  audioEnabled ? "Yes" : "No");
    if (audioEnabled == true)
    {
        box.addLinef("  Device               : %-38s", audioDevice.c_str());
        box.addLinef("  Sample Rate          : %d Hz",  audioSampleRate);
        box.addLinef("  Channels             : %s",     (audioChannels == 1) ? "Mono" : "Stereo");
        box.addLinef("  Bit Depth            : %d-bit", audioBitDepth);
        box.addLinef("  AAC Bitrate          : %d kbps", audioBitrate / 1000);
    }
    box.addSeparator();
    
    box.addLine("System Settings");
    box.addLinef("  Recording Channels   : %d", recordingChannel);
    box.addLinef("  Parking Mode         : %s", isParkingMode ? "Active" : "Inactive");
    box.addLinef("  Vibration Level      : %d", vibrationLevel);
    
    box.render();
}

std::string DvrConfig::getMountPoint(const std::string& deviceName)
{
    std::array<char, 128> buffer;
    std::string result;
    std::string cmd = "df " + deviceName + " 2>/dev/null";

    FILE* pipe_raw = popen(cmd.c_str(), "r");
    if (pipe_raw == nullptr) { return "/"; }

    auto deleter = [](FILE* fp) { if (fp) pclose(fp); };
    std::unique_ptr<FILE, decltype(deleter)> pipe(pipe_raw, deleter);

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();

    std::istringstream ss(result);
    std::string line;
    while (std::getline(ss, line))
    {
        if (line.find(deviceName) != std::string::npos)
        {
            std::istringstream lineSS(line);
            std::string fs, blocks, used, avail, usage, mp;
            lineSS >> fs >> blocks >> used >> avail >> usage >> mp;
            if (mp == "/")
            {
                const char* home = getenv("HOME");
                if (home != nullptr) { return std::string(home); }
            }
            return mp;
        }
    }
    return "/";
}
