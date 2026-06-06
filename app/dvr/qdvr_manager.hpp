/*==========================================================================
 * qdvr_manager.hpp - Queue-based DVR Manager with Event Preemption
 *==========================================================================*/
#ifndef __QDVR_MANAGER_HPP__
#define __QDVR_MANAGER_HPP__

#include "dvr_manager.hpp"

#if USE_EVENT_QUEUE
    struct EventQueueItem
    {
        EventData   eventData;
        std::string outputPath;
        size_t      preEventFrameCount;
        
        EventQueueItem(const EventData& data, const std::string& path, size_t preFrames)
            : eventData(data), outputPath(path), preEventFrameCount(preFrames) {}
    };
#endif

class QDvrManager : public DvrManager
{
public:
    explicit QDvrManager(std::shared_ptr<DvrConfig> config);
    virtual ~QDvrManager();
    
    virtual bool                    initialize(const std::vector<int>& channels, const std::map<int, int>& channelFpsMap) override;
    virtual void                    shutdown() override;
    
    #if USE_EVENT_QUEUE
        virtual bool                startEventRecording(const EventData& eventData = EventData()) override;
        size_t                      getQueueSize() const;
        void                        clearQueue();
    #endif
    
    #if USE_THREAD_WRITER || USE_EVENT_THREAD_WRITER
        virtual void                pushEncodedData(int channelId, const uint8_t* data, size_t size, GstClockTime pts) override;
    #endif

private:
    #if USE_THREAD_WRITER
        std::vector<std::thread>    m_normalWriterThreads;
        std::atomic<bool>           m_normalWriterActive;
        void                        normalWriterWorker(int fileIdx);
    #endif
    
    #if USE_EVENT_THREAD_WRITER
        std::thread                 m_eventWriterThread;
        std::atomic<bool>           m_eventWriterActive;
        void                        eventWriterWorker();
    #endif
    
    #if USE_EVENT_QUEUE
        std::queue<EventQueueItem>  m_eventQueue;
        mutable std::mutex          m_eventQueueMutex;
        std::thread                 m_eventProcessingThread;
        std::atomic<bool>           m_eventProcessingActive;
        std::atomic<bool>           m_eventWriterBusy;
        
        void                        eventProcessingWorker();
        void                        startEventRecordingInternal(const EventQueueItem& eventItem);
        void                        stopCurrentEventRecording();
        bool                        shouldPreemptCurrentEvent(const EventData& newEvent);
        void                        saveCurrentEventForResumption();
    #endif
};

#endif // __QDVR_MANAGER_HPP__