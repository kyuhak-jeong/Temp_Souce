/*==========================================================================
 * file_writer.hpp - Video/Audio File Writing with GStreamer
 *==========================================================================*/
#ifndef __FILE_WRITER_HPP__
#define __FILE_WRITER_HPP__

#include "dvr_config.hpp"
#include <shared_mutex>

class FileWriter
{
public:
    /*--- Constructor / Destructor ---*/
    FileWriter(const std::string& outputPath,
               std::shared_ptr<DvrConfig> config,
               RecordingMode mode,
               const std::vector<int>& channels,
               const std::map<int, int>& channelFpsMap);
    ~FileWriter();
    
    FileWriter(const FileWriter&)            = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    
    /*--- Lifecycle ---*/
    bool                        initialize();
    bool                        start();
    void                        stop();
    
    /*--- Write Operations ---*/
    bool                        writeFrame(int channelId, const uint8_t* data, size_t size, GstClockTime pts);
    bool                        writeAudio(const uint8_t* data, size_t size, GstClockTime pts);
    
    /*--- State Queries ---*/
    bool                        isActive() const                { return m_isActive; }
    bool                        isPipelineReady() const         { return m_pipeline != nullptr; }
    bool                        isAnchorSet() const             { return GST_CLOCK_TIME_IS_VALID(m_anchorPts.load()); }
    GstClockTime                getAnchorPts() const            { return m_anchorPts.load(); }
    bool                        shouldRotate() const;
    bool                        shouldRotateChannel(int channelId) const;
    RecordingMode               getMode() const                 { return m_mode; }
    std::string                 getOutputPath() const           { return m_outputPath; }
    const std::vector<int>&     getChannelIds() const           { return m_channelIds; }
    
    /*--- Statistics ---*/
    void                        printStatistics();

    /*--- Dynamic Caps ---*/
    void                        setVideoCaps(int channelId, GstCaps* caps);
    void                        setAudioCaps(GstCaps* caps);

private:
    /*--- GStreamer Components ---*/
    GstElement*                             m_pipeline;
    std::map<int, GstElement*>              m_appsrcMap;
    
    /*--- Configuration ---*/
    std::shared_ptr<DvrConfig>              m_config;
    RecordingMode                           m_mode;
    std::string                             m_outputPath;
    std::vector<int>                        m_channelIds;
    std::map<int, int>                      m_channelFpsMap;
    
    /*--- State ---*/
    std::atomic<bool>                       m_isActive {false};
    mutable std::shared_mutex               m_rwMutex;
    std::chrono::steady_clock::time_point   m_startTime;
    std::map<int, ChannelStats>             m_channelStatsMap;
    std::atomic<GstClockTime>               m_anchorPts;        // 첫 비디오 프레임 PTS (비디오 기준)
    std::atomic<GstClockTime>               m_audioAnchorPts;   // 첫 오디오 프레임 PTS (비디오 앵커 설정 전 대체용)
    std::atomic<GstClockTime>               m_subtitleAnchorPts;
    std::atomic<GstClockTime>               m_lastWrittenSubPts;


    std::map<int, GstCaps*>                 m_customCapsMap;
    GstCaps*                                m_customAudioCaps = nullptr;

    static std::mutex                       s_gstInitMutex;     // serializes concurrent pipeline creation
    
    /*--- Helper Functions ---*/
    std::string                 buildPipelineString();
    int                         getTargetFps(int channelId) const;
    int                         getTargetDuration() const;
    int                         getTargetFrameCount(int channelId) const;
    void                        getTargetResolution(int channelId, int& width, int& height) const;
    const char*                 getModeName() const;
    
    #ifdef USE_DVR_SUB
        std::string             formatSubtitle(const std::string& text, GstClockTime filePts, size_t index);
    #endif
};

#endif // __FILE_WRITER_HPP__