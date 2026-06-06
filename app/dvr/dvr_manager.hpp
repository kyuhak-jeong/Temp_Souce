/*==========================================================================
 * dvr_manager.hpp - Base DVR Management System
 *==========================================================================*/
#ifndef __DVR_MANAGER_HPP__
#define __DVR_MANAGER_HPP__

#include "dvr_config.hpp"
#include "file_writer.hpp"
#include <set>
#include <map>
#include <atomic>

class DvrManager
{
public:
    explicit DvrManager(std::shared_ptr<DvrConfig> config);
    virtual ~DvrManager();
    
    DvrManager(const DvrManager&)            = delete;
    DvrManager& operator=(const DvrManager&) = delete;
    
    /*--- Lifecycle ---*/
    virtual bool    initialize(const std::vector<int>& channels, const std::map<int, int>& channelFpsMap);
    virtual void    shutdown();
    
    /*--- Normal/Parking Recording ---*/
    virtual bool    startNormalRecording();
    virtual void    stopNormalRecording();
    
    /*--- Event Recording ---*/
    virtual bool    startEventRecording(const EventData& eventData = EventData());
    virtual void    stopEventRecording();
    
    /*--- Data Management ---*/
    virtual void    pushEncodedData(int channelId, const uint8_t* data, size_t size, GstClockTime pts);

    
    void            storeVideoCaps(int channelId, GstCaps* caps);
    void            storeAudioCaps(GstCaps* caps);
    GstCaps*        getVideoCaps(int channelId) const;
    GstCaps*        getAudioCaps() const;
    
    #ifdef USE_DVR_SUB
        void        setSubtitleText(const std::string& text);
    #endif
    
    /*--- Storage ---*/
    void            checkAndCleanStorage();
    
    /*--- Utilities ---*/
    std::string     getTimeString(bool forFilename = true) const;
    bool            isInitialized() const { return m_isInitialized; }

protected:
    std::shared_ptr<DvrConfig>                      m_config;
    std::vector<int>                                m_channels;
    std::map<int, int>                              m_channelFpsMap;
    
    int                                             m_numFiles;
    std::vector<std::shared_ptr<FileWriter>>        m_normalWriters;
    mutable std::mutex                              m_normalMutex;
    std::map<int, std::unique_ptr<CircularBuffer>>  m_normalBuffers;

    std::shared_ptr<FileWriter>                     m_eventWriter;
    std::map<int, std::unique_ptr<CircularBuffer>>  m_eventBuffers;
    EventData                                       m_currentEventData;
    std::chrono::system_clock::time_point           m_eventStartTime;
    mutable std::mutex                              m_eventMutex;
    
    bool                                            m_isInitialized;
    std::atomic<bool>                               m_storageCleanupInProgress;

    std::unique_ptr<CircularBuffer>                 m_audioPreEventBuffer;

    GstCaps*                                        m_dvrVideoCaps[MAX_BUFFER_NUM];
    GstCaps*                                        m_dvrAudioCaps;
    mutable std::mutex                              m_capsMutex;

    // All-channels-ready guard: recording deferred until every channel in
    // m_channels has delivered at least one frame, or timeout expires.
    std::set<int>                                   m_channelFirstFrameSeen;
    std::chrono::steady_clock::time_point           m_channelWaitStart;
    bool                                            m_channelWaitStarted = false;
    static constexpr int                            CHANNEL_WAIT_TIMEOUT_MS = 8000;
    bool                                            isAllChannelsReady();

    #ifdef USE_DVR_SUB
        struct LastSubInfo
        {
            std::string text;
            GstClockTime lastPts = GST_CLOCK_TIME_NONE;
        };
        std::map<int, LastSubInfo>                  m_lastSubMap;
        std::string                                 m_subtitleText;
        mutable std::mutex                          m_subtitleMutex;
    #endif
    
    /*--- Helpers ---*/
    void            calculateNumFiles();
    int             getFileIndexForChannel(int channelId) const;
    void            rotateNormalRecording();
    void            cleanupNormalWriters();
    void            pushNormalData(FileWriter* writer, int channelId, const uint8_t* data, size_t size, GstClockTime pts);
    void            pushEventData(int channelId, const uint8_t* data, size_t size, GstClockTime pts);
    long            getFolderSize(const std::string& path);
    std::string     cleanOldestFile(const std::string& path);
    void            logEvent(const std::string& logType, const EventData& eventData, const std::string& filePath);
};

#endif // __DVR_MANAGER_HPP__