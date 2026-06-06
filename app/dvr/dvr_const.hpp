/*==========================================================================
 * dvr_const.hpp - DVR Constants, Enums, and Data Structures
 *==========================================================================*/
#ifndef __DVR_CONST_HPP__
#define __DVR_CONST_HPP__

#include "constants.h"
#include "logger.h"

/*--------------------------------------------------------------------------
 * Feature Control Flags
 *--------------------------------------------------------------------------*/
#define USE_QDVR_MANAGER                    0

#define USE_THREAD_WRITER                   1       // Thread-based normal recording
#define USE_EVENT_THREAD_WRITER             1       // Thread-based event recording
#define USE_EVENT_QUEUE                     1       // Event queue with priority

/*--------------------------------------------------------------------------
 * Debug Control
 *--------------------------------------------------------------------------*/
#define DEBUG_DVR                           0

#if DEBUG_DVR
    #define DVR_DEBUG(msg)                  LOG_DVR_DEBUG(msg)
    #define DVR_DEBUGF(fmt, ...)            LOG_DVR_DEBUGF(fmt, ##__VA_ARGS__)
#else
    #define DVR_DEBUG(msg)
    #define DVR_DEBUGF(fmt, ...)
#endif

/*--------------------------------------------------------------------------
 * System Constants
 *--------------------------------------------------------------------------*/
constexpr size_t MB_TO_BYTE                 = 1024 * 1024;
constexpr size_t DEFAULT_BUFFER_MAX_BYTES   = 15 * MB_TO_BYTE;
constexpr size_t DEFAULT_BUFFER_MAX_COUNT   = 900;
constexpr size_t DVR_CHANNELS_PER_FILE      = 4;

/*--------------------------------------------------------------------------
 * Special Channel Indexing
 *--------------------------------------------------------------------------*/
constexpr int NORMAL_SUB_CH_BASE            = 1000;
constexpr int EVENT_SUB_CH_BASE             = 2000;
constexpr int NORMAL_AUDIO_CH_BASE          = 9000;
constexpr int EVENT_AUDIO_CH_BASE           = 9500;
constexpr int AUDIO_CH_ID                   = 9000;

inline int  getNormalSubChIdx(int fileIdx)      { return NORMAL_SUB_CH_BASE + fileIdx; }
inline int  getEventSubChIdx(int fileIdx = 0)   { return EVENT_SUB_CH_BASE + fileIdx; }
inline int  getNormalAudioChIdx(int fileIdx)    { return NORMAL_AUDIO_CH_BASE + fileIdx; }
inline int  getEventAudioChIdx(int fileIdx = 0) { return EVENT_AUDIO_CH_BASE + fileIdx; }

inline bool isSubtitleChannel(int chId)
{
    return ((chId >= NORMAL_SUB_CH_BASE && chId < NORMAL_SUB_CH_BASE + 1000) ||
            (chId >= EVENT_SUB_CH_BASE  && chId < EVENT_SUB_CH_BASE  + 1000));
}
inline bool isAudioChannel(int chId)
{
    return ((chId >= NORMAL_AUDIO_CH_BASE && chId < NORMAL_AUDIO_CH_BASE + 500) ||
            (chId >= EVENT_AUDIO_CH_BASE  && chId < EVENT_AUDIO_CH_BASE  + 500));
}
inline bool isVideoChannel(int chId)            { return !isSubtitleChannel(chId) && !isAudioChannel(chId); }

inline bool isSvmCamera(int camIdx)
{
    #ifdef USE_SVM
        return (camIdx < SVM_CAMERAS_NUM);
    #else
        (void)camIdx;
        return false;
    #endif
}

/*--------------------------------------------------------------------------
 * Recording Modes
 *--------------------------------------------------------------------------*/
enum class RecordingMode { NORMAL, EVENT };

/*--------------------------------------------------------------------------
 * Event Types (Priority: 0=Highest)
 *--------------------------------------------------------------------------*/
enum class EventType
{
    CRITICAL        = 0,    // Highest priority
    COLLISION       = 1,
    MANUAL          = 2,
    PARKING_MOTION  = 3,
    CUSTOM          = 4,
    PUA             = 5
};

/*--------------------------------------------------------------------------
 * Event Data Structure
 *--------------------------------------------------------------------------*/
struct EventData
{
    EventType                               type;
    std::string                             additionalData;
    bool                                    requiresFullPreEvent;
    std::chrono::system_clock::time_point   triggerTime;
    
    EventData(EventType evtType = EventType::CUSTOM, const std::string& data = "", bool fullPreEvent = true)
        : type(evtType), additionalData(data), requiresFullPreEvent(fullPreEvent), triggerTime(std::chrono::system_clock::now())
    {}
    
    const char* getTypeName() const
    {
        switch (type)
        {
            case EventType::CRITICAL:       return "CRITICAL";
            case EventType::COLLISION:      return "COLLISION";
            case EventType::MANUAL:         return "MANUAL";
            case EventType::PARKING_MOTION: return "PARKING_MOTION";
            case EventType::PUA:            return "PUA";
            default:                        return "CUSTOM";
        }
    }
    
    int  getPriority() const                        { return static_cast<int>(type); }
    bool operator<(const EventData& other) const    { return type < other.type; }
};

/*--------------------------------------------------------------------------
 * Channel Statistics
 *--------------------------------------------------------------------------*/
struct ChannelStats
{
    int                  channelId;
    int                  targetFps;
    std::atomic<size_t>  framesReceived  {0};
    std::atomic<size_t>  framesProcessed {0};
    std::atomic<size_t>  framesDropped   {0};
    GstClockTime         timestamp       {0};

    ChannelStats() : channelId(-1), targetFps(0), timestamp(0) {}

    ChannelStats(int id, int fps) : channelId(id), targetFps(fps), timestamp(0) {}

    ChannelStats(const ChannelStats& o)
        : channelId(o.channelId), targetFps(o.targetFps)
        , framesReceived(o.framesReceived.load())
        , framesProcessed(o.framesProcessed.load())
        , framesDropped(o.framesDropped.load())
        , timestamp(o.timestamp) {}

    ChannelStats& operator=(const ChannelStats& o)
    {
        if (this != &o)
        {
            channelId = o.channelId;
            targetFps = o.targetFps;
            framesReceived.store(o.framesReceived.load());
            framesProcessed.store(o.framesProcessed.load());
            framesDropped.store(o.framesDropped.load());
            timestamp = o.timestamp;
        }
        return *this;
    }
    
    void reset() { framesReceived = 0; framesProcessed = 0; framesDropped = 0; timestamp = 0; }
};

/*--------------------------------------------------------------------------
 * Circular Buffer - Thread-safe Frame Storage
 *--------------------------------------------------------------------------*/
class CircularBuffer
{
public:
    CircularBuffer(int chId,
                   size_t framesLimit = DEFAULT_BUFFER_MAX_COUNT,
                   size_t bytesLimit  = DEFAULT_BUFFER_MAX_BYTES)
        : m_channelId(chId), m_maxFrames(framesLimit), m_maxBytes(bytesLimit)
        , m_currentBytes(0), m_readIndex(0), m_totalPushed(0), m_totalRead(0)
    {}
    
    ~CircularBuffer() { clear(); }
    
    void push(const uint8_t* rawData, size_t dataSize, GstClockTime pts = GST_CLOCK_TIME_NONE)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto frame = std::make_shared<FrameData>(m_channelId, rawData, dataSize, pts);
        m_buffer.push_back(frame);
        m_currentBytes += dataSize;
        m_totalPushed++;
        enforceLimits();
    }

    std::vector<std::shared_ptr<FrameData>> getUnreadFrames(size_t maxBatch = 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<FrameData>> result;
        size_t count = 0;
        size_t limit = (maxBatch == 0) ? m_buffer.size() : maxBatch;
        
        for (size_t i = m_readIndex; (i < m_buffer.size() && count < limit); ++i, ++count)
            result.push_back(m_buffer[i]->clone());

        return result;
    }
    
    std::vector<std::shared_ptr<FrameData>> getLastFrames(size_t count = 0) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<FrameData>> result;

        if (count == 0) for (const auto& f : m_buffer) { result.push_back(f->clone()); }
        else
        {
            size_t start = (m_buffer.size() > count) ? (m_buffer.size() - count) : 0;
            for (size_t i = start; i < m_buffer.size(); ++i) result.push_back(m_buffer[i]->clone());
        }
        return result;
    }
    
    std::vector<std::shared_ptr<FrameData>> getAllFrames() const { return getLastFrames(0); }
    
    void markAsRead(size_t count)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t actual   = std::min(count, m_buffer.size() - m_readIndex);
        m_readIndex     = std::min(m_readIndex + count, m_buffer.size());
        m_totalRead    += actual;
    }
    
    void clearReadFrames()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (size_t i = 0; (i < m_readIndex && m_buffer.empty() == false); ++i)
        {
            m_currentBytes -= m_buffer.front()->size;
            m_buffer.pop_front();
        }
        
        m_readIndex = 0;
    }
    
    void resetReadIndex()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_readIndex = m_buffer.size();
        m_totalRead = m_totalPushed;
    }
    
    void clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_buffer.clear();
        m_currentBytes  = 0;
        m_readIndex     = 0;
        m_totalPushed   = 0;
        m_totalRead     = 0;
    }
    
    size_t getFrameCount() const    { std::lock_guard<std::mutex> lock(m_mutex); return m_buffer.size(); }
    size_t getUnreadCount() const   { std::lock_guard<std::mutex> lock(m_mutex); return m_buffer.size() - m_readIndex; }
    size_t getCurrentBytes() const  { std::lock_guard<std::mutex> lock(m_mutex); return m_currentBytes; }
    int    getChannelId() const     { return m_channelId; }

private:
    std::deque<std::shared_ptr<FrameData>>  m_buffer;
    mutable std::mutex                      m_mutex;
    int                                     m_channelId;
    size_t                                  m_maxFrames;
    size_t                                  m_maxBytes;
    std::atomic<size_t>                     m_currentBytes;
    size_t                                  m_readIndex;
    size_t                                  m_totalPushed;
    size_t                                  m_totalRead;
    
    void removeOldest()
    {
        if (m_buffer.empty() == false)
        {
            m_currentBytes -= m_buffer.front()->size;
            m_buffer.pop_front();
            if (m_readIndex > 0) m_readIndex--;
        }
    }
    
    void enforceLimits()
    {
        while (m_buffer.empty() == false &&
               (m_currentBytes > m_maxBytes || m_buffer.size() > m_maxFrames))
        {
            removeOldest();
        }
    }
};

#endif // __DVR_CONST_HPP__
