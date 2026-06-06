/*==========================================================================
 * dvr_config.hpp - DVR Configuration Management
 *==========================================================================*/
#ifndef __DVR_CONFIG_HPP__
#define __DVR_CONFIG_HPP__

#include "dvr_const.hpp"

class DvrConfig
{
public:
    /*--- Storage Configuration ---*/
    #ifdef EMBEDDED_DEVICE
        #ifdef DEVICE_RK3588
            std::string storageDev      = "/dev/mmcblk0p8";
        #else
            std::string storageDev      = "/dev/nvme0n1p1";
        #endif
    #else
        std::string storageDev          = "/dev/sda1";
    #endif
    
    std::string storageDevPath          = "/";
    std::string normalDirName           = "Normal";
    std::string eventDirName            = "Event";
    std::string parkingDirName          = "Parking";
    
    /*--- Storage Limits ---*/
    long        maxSizeOfNormalFolder   = 300 * 1000 * MB_TO_BYTE;
    long        maxSizeOfEventFolder    = 50  * 1000 * MB_TO_BYTE;
    long        maxSizeOfParkingFolder  = 100 * 1000 * MB_TO_BYTE;
    
    /*--- Recording Settings ---*/
    int         recordingTime           = 60;
    int         recordingChannel        = SVM_CAMERAS_NUM;
    int         recordingFpsAvm         = 30;
    int         recordingFpsInternal    = 30;
    int         recordingFpsParking     = 5;
    
    /*--- Event Settings ---*/
    int         recordingTimePreEvent   = 10;
    int         recordingTimeTotalEvent = 30;
    
    /*--- Resolution Settings ---*/
    int         recordingWidth          = 1920;
    int         recordingHeight         = 1080;
    int         recordingWidthInternal  = 1280;
    int         recordingHeightInternal = 720;
    
    /*--- Mode Settings ---*/
    int         vibrationLevel          = 1;
    bool        isParkingMode           = false;
    bool        isDvrSettingsChanged    = false;
    
    /*--- Audio Settings ---*/
    bool        audioEnabled            = true;
    std::string audioDevice             = "hw:0,0";     // ALSA device (e.g. "hw:0,0", "default", "plughw:1,0")
    int         audioSampleRate         = 44100;        // Hz: 8000, 16000, 44100, 48000
    int         audioChannels           = 2;            // 1=mono, 2=stereo
    int         audioBitDepth           = 16;           // Bits per sample: 8, 16, 32
    int         audioBitrate            = 256000;       // AAC bitrate in bps (e.g. 64000, 128000)

    /*--- Methods ---*/
    DvrConfig() {}

    void        loadConfig();
    bool        readConfig();
    bool        writeConfig();
    void        printConfig();

    void        updateStoragePath()        { storageDevPath = getMountPoint(storageDev); }

    std::string getEventFullPath()   const { return storageDevPath + "/VideoFiles/" + eventDirName; }
    std::string getNormalFullPath()  const { return storageDevPath + "/VideoFiles/" + normalDirName; }
    std::string getParkingFullPath() const { return storageDevPath + "/VideoFiles/" + parkingDirName; }

private:
    std::string getMountPoint(const std::string& deviceName);
};

#endif // __DVR_CONFIG_HPP__
