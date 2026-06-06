/*==========================================================================
 * qdvr_manager.cpp - Queue-based DVR Manager Implementation
 *==========================================================================*/
#include "qdvr_manager.hpp"

/*--------------------------------------------------------------------------
 * Constructor / Destructor
 *--------------------------------------------------------------------------*/
QDvrManager::QDvrManager(std::shared_ptr<DvrConfig> config)
    : DvrManager(config)
    #if USE_THREAD_WRITER
        , m_normalWriterActive(false)
    #endif
    #if USE_EVENT_THREAD_WRITER
        , m_eventWriterActive(false)
    #endif
    #if USE_EVENT_QUEUE
        , m_eventProcessingActive(false), m_eventWriterBusy(false)
    #endif
{
    std::stringstream msg;
    msg << "QDvrManager created (";
    #if USE_THREAD_WRITER == 1
        msg << "Thread Normal, ";
    #else
        msg << "Direct Normal, ";
    #endif
    #if USE_EVENT_THREAD_WRITER == 1
        msg << "Thread Event, ";
    #else
        msg << "Direct Event, ";
    #endif
    #if USE_EVENT_QUEUE == 1
        msg << "Queued Events)";
    #else
        msg << "Single Event)";
    #endif
    
    LOG_DVR_INFOF("%s%s%s", LOG::Color::YELLOW, msg.str().c_str(), LOG::Color::RESET);
}

QDvrManager::~QDvrManager()
{
    shutdown();
}

/*--------------------------------------------------------------------------
 * Lifecycle Functions
 *--------------------------------------------------------------------------*/
bool QDvrManager::initialize(const std::vector<int>& channels, const std::map<int, int>& channelFpsMap)
{
    if (DvrManager::initialize(channels, channelFpsMap) == false)
    {
        return false;
    }
    
    #if USE_THREAD_WRITER
        LOG_DVR_INFOF("%sStarting %d normal writer threads%s", LOG::Color::YELLOW, m_numFiles, LOG::Color::RESET);
        m_normalWriterActive = true;
        
        for (int i = 0; i < m_numFiles; i++)
        {
            m_normalWriterThreads.emplace_back(&QDvrManager::normalWriterWorker, this, i);
        }
    #endif
    
    #if USE_EVENT_THREAD_WRITER
        LOG_DVR_INFOF("%sStarting event writer thread%s", LOG::Color::YELLOW, LOG::Color::RESET);
        m_eventWriterActive = true;
        m_eventWriterThread = std::thread(&QDvrManager::eventWriterWorker, this);
    #endif
    
    #if USE_EVENT_QUEUE
        LOG_DVR_INFOF("%sStarting event queue processing thread%s", LOG::Color::YELLOW, LOG::Color::RESET);
        m_eventProcessingActive = true;
        m_eventProcessingThread = std::thread(&QDvrManager::eventProcessingWorker, this);
    #endif
    
    LOG_DVR_SUCCESSF("%sQDvrManager initialization complete%s", LOG::Color::YELLOW, LOG::Color::RESET);
    return true;
}

void QDvrManager::shutdown()
{
    if (m_isInitialized == false)
    {
        return;
    }

    #if USE_THREAD_WRITER
        m_normalWriterActive = false;
        for (auto& t : m_normalWriterThreads)
        {
            if (t.joinable() == true)
            {
                t.join();
            }
        }
        m_normalWriterThreads.clear();
        LOG_DVR_INFOF("%sNormal writer threads stopped%s", LOG::Color::YELLOW, LOG::Color::RESET);
    #endif

    #if USE_EVENT_THREAD_WRITER
        m_eventWriterActive = false;
        if (m_eventWriterThread.joinable() == true)
        {
            m_eventWriterThread.join();
        }
        LOG_DVR_INFOF("%sEvent writer thread stopped%s", LOG::Color::YELLOW, LOG::Color::RESET);
    #endif

    #if USE_EVENT_QUEUE
        m_eventProcessingActive = false;
        if (m_eventProcessingThread.joinable() == true)
        {
            m_eventProcessingThread.join();
        }
        clearQueue();
        LOG_DVR_INFOF("%sQueued event processing thread stopped%s", LOG::Color::YELLOW, LOG::Color::RESET);
    #endif

    DvrManager::shutdown();
    LOG_DVR_SUCCESSF("%sQDvrManager shutdown completed%s", LOG::Color::YELLOW, LOG::Color::RESET);
}

/*--------------------------------------------------------------------------
 * Event Recording (Queue-based)
 *--------------------------------------------------------------------------*/
#if USE_EVENT_QUEUE
    bool QDvrManager::startEventRecording(const EventData& eventData)
    {
        if (m_isInitialized == false)
        {
            LOG_DVR_ERROR("Not initialized");
            return false;
        }
        
        // Check available pre-event buffer
        size_t availablePre = 0;
        if (m_channels.empty() == false)
        {
            auto it = m_eventBuffers.find(m_channels[0]);
            if (it != m_eventBuffers.end())
            {
                availablePre = it->second->getFrameCount();
            }
        }
        
        size_t requiredPre = m_channelFpsMap[m_channels[0]] * m_config->recordingTimePreEvent;
        if ((eventData.requiresFullPreEvent == true) && (availablePre < requiredPre))
        {
            LOG_DVR_WARNINGF("Insufficient pre-event buffer (%zu/%zu frames) for event type: %s",
                            availablePre, requiredPre, eventData.getTypeName());
            
            if (eventData.getPriority() > 1)
            {
                return false;
            }
        }
        
        // Check if current event should be preempted
        bool shouldPreempt = false;
        {
            std::lock_guard<std::mutex> lock(m_eventMutex);
            if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
            {
                shouldPreempt = shouldPreemptCurrentEvent(eventData);
            }
        }
        
        if (shouldPreempt == true)
        {
            LOG_DVR_WARNINGF("%sPreempting current event for higher priority: %s%s",
                            LOG::Color::MAGENTA, eventData.getTypeName(), LOG::Color::RESET);
            
            saveCurrentEventForResumption();
            
            // Wait for event writer to finish current frame
            int waitCount = 0;
            while ((m_eventWriterBusy.load() == true) && (waitCount < 100))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                waitCount++;
            }
            
            stopCurrentEventRecording();
        }
        
        std::string outputDir = m_config->getEventFullPath();
        std::filesystem::create_directories(outputDir);
        
        std::string filename = outputDir + "/E_" + getTimeString() +
                              "_CH" + std::to_string(m_channels.size()) +
                              "_FR" + std::to_string(m_channelFpsMap[m_channels[0]]) +
                              "_" + eventData.getTypeName() + ".mp4";
        
        {
            std::lock_guard<std::mutex> lock(m_eventQueueMutex);
            
            if ((shouldPreempt == true) || (eventData.getPriority() <= 1))
            {
                // High priority: insert at front
                std::queue<EventQueueItem> temp;
                temp.emplace(eventData, filename, availablePre);
                
                while (m_eventQueue.empty() == false)
                {
                    temp.push(std::move(m_eventQueue.front()));
                    m_eventQueue.pop();
                }
                
                m_eventQueue = std::move(temp);
                
                LOG_DVR_INFOF("%sHigh priority event queued at front (Priority: %d, Queue: %zu)%s",
                             LOG::Color::MAGENTA, eventData.getPriority(), m_eventQueue.size(), LOG::Color::RESET);
            }
            else
            {
                // Normal priority: append to back
                m_eventQueue.emplace(eventData, filename, availablePre);
                
                LOG_DVR_INFOF("%sEvent queued: %s (Priority: %d, Queue: %zu)%s",
                             LOG::Color::MAGENTA, eventData.getTypeName(), eventData.getPriority(),
                             m_eventQueue.size(), LOG::Color::RESET);
            }
        }
        
        logEvent("EVENT_QUEUE", eventData, filename);
        return true;
    }

    size_t QDvrManager::getQueueSize() const
    {
        std::lock_guard<std::mutex> lock(m_eventQueueMutex);
        return m_eventQueue.size();
    }

    void QDvrManager::clearQueue()
    {
        std::lock_guard<std::mutex> lock(m_eventQueueMutex);
        
        while (m_eventQueue.empty() == false)
        {
            m_eventQueue.pop();
        }
        
        LOG_DVR_INFOF("%sEvent queue cleared%s", LOG::Color::YELLOW, LOG::Color::RESET);
    }
#endif

/*--------------------------------------------------------------------------
 * Data Management (Thread-based)
 *--------------------------------------------------------------------------*/
#if USE_THREAD_WRITER || USE_EVENT_THREAD_WRITER
    void QDvrManager::pushEncodedData(int channelId, const uint8_t* data, size_t size, GstClockTime pts)
    {
        if (m_isInitialized == false)
        {
            DVR_DEBUG("Not initialized");
            return;
        }

        if (isAudioChannel(channelId))
        {
            if (m_config->audioEnabled == false) return;

            // Push to normal audio buffers (one per file)
            #if USE_THREAD_WRITER
                for (int fileIdx = 0; fileIdx < m_numFiles; fileIdx++)
                {
                    int normalAudioIdx = getNormalAudioChIdx(fileIdx);
                    auto it = m_normalBuffers.find(normalAudioIdx);
                    if (it != m_normalBuffers.end()) it->second->push(data, size, pts);
                }
            #else
                // Direct write
                for (int fileIdx = 0; fileIdx < m_numFiles; fileIdx++)
                {
                    int normalAudioIdx = getNormalAudioChIdx(fileIdx);
                    std::shared_ptr<FileWriter> normalWriter;
                    {
                        std::lock_guard<std::mutex> lock(m_normalMutex);
                        if (fileIdx < static_cast<int>(m_normalWriters.size()))
                        {
                            normalWriter = m_normalWriters[fileIdx];
                        }
                    }
                    if (normalWriter != nullptr)
                    {
                        pushNormalData(normalWriter.get(), normalAudioIdx, data, size, pts);
                    }
                }
            #endif

            // Push to event audio buffer
            #if USE_EVENT_THREAD_WRITER
                int eventAudioIdx = getEventAudioChIdx(0);
                auto evIt = m_eventBuffers.find(eventAudioIdx);
                if (evIt != m_eventBuffers.end()) evIt->second->push(data, size, pts);
            #else
                int eventAudioIdx = getEventAudioChIdx(0);
                pushEventData(eventAudioIdx, data, size, pts);
            #endif
            
            return;
        }

        #if USE_THREAD_WRITER
            // Push to normal buffers (threads will consume)
            int fileIdx = getFileIndexForChannel(channelId);
            bool isValidFileIdx = false;
            size_t writersSize = 0;
            {
                std::lock_guard<std::mutex> lock(m_normalMutex);
                writersSize = m_normalWriters.size();
                isValidFileIdx = (fileIdx >= 0) && (fileIdx < static_cast<int>(writersSize));
            }

            if (isValidFileIdx)
            {
                auto it = m_normalBuffers.find(channelId);
                if (it != m_normalBuffers.end()) it->second->push(data, size, pts);
                
                #ifdef USE_DVR_SUB
                    if (channelId == m_channels[0])
                    {
                        std::string subtitleText;
                        {
                            std::lock_guard<std::mutex> lock(m_subtitleMutex);
                            subtitleText = m_subtitleText;
                        }
                        if (subtitleText.empty() == true) { subtitleText = " "; }
                        
                        std::lock_guard<std::mutex> lock(m_normalMutex);
                        for (int i = 0; i < static_cast<int>(m_normalWriters.size()); i++)
                        {
                            if ((m_normalWriters[i] != nullptr) && (m_normalWriters[i]->isActive() == true))
                            {
                                int subChId = getNormalSubChIdx(i);
                                bool shouldPush = false;
                                {
                                    std::lock_guard<std::mutex> lock(m_subtitleMutex);
                                    auto& lastInfo = m_lastSubMap[subChId];
                                    bool timeElapsed = (lastInfo.lastPts == GST_CLOCK_TIME_NONE ||
                                                        (pts > lastInfo.lastPts && (pts - lastInfo.lastPts) >= GST_SECOND));
                                    if (timeElapsed)
                                    {
                                        shouldPush = true;
                                        lastInfo.text = subtitleText;
                                        lastInfo.lastPts = pts;
                                    }
                                }
                                if (shouldPush)
                                {
                                    auto subIt = m_normalBuffers.find(subChId);
                                    if (subIt != m_normalBuffers.end())
                                    {
                                        subIt->second->push(reinterpret_cast<const uint8_t*>(subtitleText.c_str()),
                                                            subtitleText.size(), pts);
                                    }
                                }
                            }
                        }
                    }
                #endif
            }
        #else
            // Direct push to normal writers
            int fileIdx = getFileIndexForChannel(channelId);
            std::shared_ptr<FileWriter> normalWriter;
            {
                std::lock_guard<std::mutex> lock(m_normalMutex);
                if ((fileIdx >= 0) && (fileIdx < static_cast<int>(m_normalWriters.size())))
                {
                    normalWriter = m_normalWriters[fileIdx];
                }
            }

            if (normalWriter != nullptr)
            {
                pushNormalData(normalWriter.get(), channelId, data, size, pts);
                
                #ifdef USE_DVR_SUB
                    if (channelId == m_channels[0])
                    {
                        std::string subtitleText;
                        {
                            std::lock_guard<std::mutex> lock(m_subtitleMutex);
                            subtitleText = m_subtitleText;
                        }
                        if (subtitleText.empty() == true) { subtitleText = " "; }
                        
                        std::vector<std::shared_ptr<FileWriter>> activeWriters;
                        {
                            std::lock_guard<std::mutex> lock(m_normalMutex);
                            for (int i = 0; i < static_cast<int>(m_normalWriters.size()); i++)
                            {
                                if (m_normalWriters[i] != nullptr)
                                    activeWriters.push_back(m_normalWriters[i]);
                            }
                        }

                        for (size_t i = 0; i < activeWriters.size(); i++)
                        {
                            if (activeWriters[i]->isActive() == true)
                            {
                                int subChId = getNormalSubChIdx(i);
                                bool shouldPush = false;
                                {
                                    std::lock_guard<std::mutex> lock(m_subtitleMutex);
                                    auto& lastInfo = m_lastSubMap[subChId];
                                    bool timeElapsed = (lastInfo.lastPts == GST_CLOCK_TIME_NONE ||
                                                        (pts > lastInfo.lastPts && (pts - lastInfo.lastPts) >= GST_SECOND));
                                    if (timeElapsed)
                                    {
                                        shouldPush = true;
                                        lastInfo.text = subtitleText;
                                        lastInfo.lastPts = pts;
                                    }
                                }
                                if (shouldPush)
                                {
                                    pushNormalData(activeWriters[i].get(), subChId,
                                                  reinterpret_cast<const uint8_t*>(subtitleText.c_str()),
                                                  subtitleText.size(), pts);
                                }
                            }
                        }
                    }
                #endif
            }
        #endif

        #if USE_EVENT_THREAD_WRITER
            // Push to event buffers (thread will consume)
            auto evIt = m_eventBuffers.find(channelId);
            if (evIt != m_eventBuffers.end()) evIt->second->push(data, size, pts);
            
            #ifdef USE_DVR_SUB
                if (channelId == m_channels[0])
                {
                    std::string subtitleText;
                    {
                        std::lock_guard<std::mutex> lock(m_subtitleMutex);
                        subtitleText = m_subtitleText;
                    }
                    if (subtitleText.empty() == true) { subtitleText = " "; }
                    
                    int subChId = getEventSubChIdx(0);
                    bool shouldPush = false;
                    {
                        std::lock_guard<std::mutex> lock(m_subtitleMutex);
                        auto& lastInfo = m_lastSubMap[subChId];
                        bool timeElapsed = (lastInfo.lastPts == GST_CLOCK_TIME_NONE ||
                                            (pts > lastInfo.lastPts && (pts - lastInfo.lastPts) >= GST_SECOND));
                        if (timeElapsed)
                        {
                            shouldPush = true;
                            lastInfo.text = subtitleText;
                            lastInfo.lastPts = pts;
                        }
                    }
                    if (shouldPush)
                    {
                        auto subIt = m_eventBuffers.find(subChId);
                        if (subIt != m_eventBuffers.end())
                        {
                            subIt->second->push(reinterpret_cast<const uint8_t*>(subtitleText.c_str()),
                                               subtitleText.size(), pts);
                        }
                    }
                }
            #endif
        #else
            // Direct push to event writer
            pushEventData(channelId, data, size, pts);
            
            #ifdef USE_DVR_SUB
                if (channelId == m_channels[0])
                {
                    std::string subtitleText;
                    {
                        std::lock_guard<std::mutex> lock(m_subtitleMutex);
                        subtitleText = m_subtitleText;
                    }
                    if (subtitleText.empty() == true) { subtitleText = " "; }
                    
                    int subChId = getEventSubChIdx(0);
                    bool shouldPush = false;
                    {
                        std::lock_guard<std::mutex> lock(m_subtitleMutex);
                        auto& lastInfo = m_lastSubMap[subChId];
                        bool timeElapsed = (lastInfo.lastPts == GST_CLOCK_TIME_NONE ||
                                            (pts > lastInfo.lastPts && (pts - lastInfo.lastPts) >= GST_SECOND));
                        if (timeElapsed)
                        {
                            shouldPush = true;
                            lastInfo.text = subtitleText;
                            lastInfo.lastPts = pts;
                        }
                    }
                    if (shouldPush)
                    {
                        pushEventData(subChId,
                                     reinterpret_cast<const uint8_t*>(subtitleText.c_str()),
                                     subtitleText.size(), pts);
                    }
                }
            #endif
        #endif
    }
#endif

/*--------------------------------------------------------------------------
 * Worker Threads
 *--------------------------------------------------------------------------*/
#if USE_THREAD_WRITER
    void QDvrManager::normalWriterWorker(int fileIdx)
    {
        DVR_DEBUGF("%sNormal writer worker %d started%s", LOG::Color::YELLOW, fileIdx, LOG::Color::RESET);
        
        constexpr int BUSY_MS     = 5;
        constexpr int IDLE_MS     = 10;
        constexpr int INACTIVE_MS = 100;
        
        while (m_normalWriterActive == true)
        {
            std::shared_ptr<FileWriter> writer;
            {
                std::lock_guard<std::mutex> lock(m_normalMutex);
                if ((fileIdx >= 0) && (fileIdx < static_cast<int>(m_normalWriters.size())))
                {
                    writer = m_normalWriters[fileIdx];
                }
            }

            if ((writer == nullptr) || (writer->isActive() == false))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(INACTIVE_MS));
                continue;
            }
            
            // Check if any channel has data
            std::vector<int> checkChannels = writer->getChannelIds();
            if (m_config->audioEnabled)
            {
                checkChannels.push_back(getNormalAudioChIdx(fileIdx));
            }
            bool hasData = false;
            
            for (int chId : checkChannels)
            {
                auto it = m_normalBuffers.find(chId);
                if ((it != m_normalBuffers.end()) && (it->second->getUnreadCount() > 0))
                {
                    hasData = true;
                    break;
                }
            }
            
            if (hasData == false)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_MS));
                continue;
            }
            
            // Process all channels with data
            for (int chId : checkChannels)
            {
                auto it = m_normalBuffers.find(chId);
                if (it == m_normalBuffers.end())
                {
                    continue;
                }
                
                CircularBuffer* buf = it->second.get();
                if (buf->getUnreadCount() > 0)
                {
                    auto frames = buf->getUnreadFrames();
                    size_t written = 0;
                    
                    for (const auto& f : frames)
                    {
                        // For subtitle/audio: get reference video channel for this file
                        int refChannelId = f->channelId;
                        
                        if (isSubtitleChannel(f->channelId) == true || isAudioChannel(f->channelId) == true)
                        {
                            int tempIdx = isSubtitleChannel(f->channelId)
                                        ? (f->channelId - NORMAL_SUB_CH_BASE)
                                        : (f->channelId - NORMAL_AUDIO_CH_BASE);
                            #ifdef USE_DVR_SPLIT
                                refChannelId = tempIdx * DVR_CHANNELS_PER_FILE;
                            #else
                                refChannelId = m_channels[0];
                            #endif
                        }
                        
                        // For video channels: stop if reached target frame count
                        // For subtitle channels: stop if reference video channel reached target
                        if (writer->shouldRotateChannel(refChannelId) == true)
                        {
                            break;
                        }
                        
                        bool ok = false;
                        if (isAudioChannel(f->channelId))
                        {
                            ok = writer->writeAudio(f->data.get(), f->size, f->pts);
                        }
                        else
                        {
                            ok = writer->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
                        }

                        if (ok == true)
                        {
                            written++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    
                    buf->markAsRead(written);
                    buf->clearReadFrames();
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(BUSY_MS));
        }
        
        DVR_DEBUGF("%sNormal writer worker %d stopped%s", LOG::Color::YELLOW, fileIdx, LOG::Color::RESET);
    }
#endif

#if USE_EVENT_THREAD_WRITER
    void QDvrManager::eventWriterWorker()
    {
        DVR_DEBUGF("%s%sEvent writer worker started%s", LOG::Color::MAGENTA, LOG::Color::YELLOW, LOG::Color::RESET);
        
        while (m_eventWriterActive == true)
        {
            std::shared_ptr<FileWriter> writer;
            bool active = false;

            {
                std::lock_guard<std::mutex> lock(m_eventMutex);
                if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
                {
                    writer = m_eventWriter;
                    active = true;
                }
            }
            
            if (active == false)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // Check if any event channel has data
            std::vector<int> checkChannels = writer->getChannelIds();
            if (m_config->audioEnabled)
            {
                checkChannels.push_back(getEventAudioChIdx(0));
            }
            bool hasData = false;
            
            for (int chId : checkChannels)
            {
                auto it = m_eventBuffers.find(chId);
                if ((it != m_eventBuffers.end()) && (it->second->getUnreadCount() > 0))
                {
                    hasData = true;
                    break;
                }
            }
            
            if (hasData == false)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Process all event channels with data
            for (int chId : checkChannels)
            {
                auto it = m_eventBuffers.find(chId);
                if (it == m_eventBuffers.end())
                {
                    continue;
                }
                
                CircularBuffer* buf = it->second.get();
                if (buf->getUnreadCount() > 0)
                {
                    auto frames = buf->getUnreadFrames();
                    size_t written = 0;
                    
                    for (const auto& f : frames)
                    {
                        bool shouldContinue = false;
                        
                        {
                            std::lock_guard<std::mutex> lock(m_eventMutex);
                            shouldContinue = ((m_eventWriter != nullptr) &&
                                            (m_eventWriter->isActive() == true) &&
                                            (m_eventWriter->shouldRotate() == false));
                        }
                        
                        if (shouldContinue == false)
                        {
                            break;
                        }
                        
                        bool ok = false;
                        if (isAudioChannel(f->channelId))
                        {
                            ok = writer->writeAudio(f->data.get(), f->size, f->pts);
                        }
                        else
                        {
                            ok = writer->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
                        }

                        if (ok == true)
                        {
                            written++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    
                    buf->markAsRead(written);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        
        DVR_DEBUGF("%s%sEvent writer worker stopped%s", LOG::Color::MAGENTA, LOG::Color::YELLOW, LOG::Color::RESET);
    }
#endif

#if USE_EVENT_QUEUE
    void QDvrManager::eventProcessingWorker()
    {
        DVR_DEBUGF("%s%sQueued event processing worker started%s",
                  LOG::Color::MAGENTA, LOG::Color::YELLOW, LOG::Color::RESET);
        
        while (m_eventProcessingActive == true)
        {
            // Check if current event finished
            bool currentFinished = false;
            {
                std::lock_guard<std::mutex> lock(m_eventMutex);
                if ((m_eventWriter != nullptr) && (m_eventWriter->shouldRotate() == true))
                {
                    currentFinished = true;
                }
            }
            
            if (currentFinished == true)
            {
                // Wait for writer to finish
                int waitCount = 0;
                while ((m_eventWriterBusy.load() == true) && (waitCount < 100))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    waitCount++;
                }
                
                stopCurrentEventRecording();
            }
            
            // Check if we can start next event
            bool hasActive = false;
            bool hasQueued = false;
            
            {
                std::lock_guard<std::mutex> lock(m_eventMutex);
                hasActive = ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true));
            }
            
            if (hasActive == false)
            {
                std::unique_ptr<EventQueueItem> next;
                
                {
                    std::lock_guard<std::mutex> lock(m_eventQueueMutex);
                    if (m_eventQueue.empty() == false)
                    {
                        next = std::make_unique<EventQueueItem>(m_eventQueue.front().eventData,
                                                               m_eventQueue.front().outputPath,
                                                               m_eventQueue.front().preEventFrameCount);
                        m_eventQueue.pop();
                        hasQueued = true;
                    }
                }
                
                if ((hasQueued == true) && (next != nullptr))
                {
                    startEventRecordingInternal(*next);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        DVR_DEBUGF("%s%sQueued event processing worker stopped%s",
                  LOG::Color::MAGENTA, LOG::Color::YELLOW, LOG::Color::RESET);
    }

    void QDvrManager::startEventRecordingInternal(const EventQueueItem& item)
    {
        m_eventWriterBusy = true;
        std::lock_guard<std::mutex> lock(m_eventMutex);
        
        std::vector<int> fileChannels = m_channels;
        #ifdef USE_DVR_SUB
            fileChannels.push_back(getEventSubChIdx(0));
        #endif
        
        m_eventWriter = std::make_shared<FileWriter>(item.outputPath, m_config, RecordingMode::EVENT,
                                                     fileChannels, m_channelFpsMap);
        
        for (int chId : fileChannels)
        {
            if (isVideoChannel(chId))
            {
                GstCaps* caps = getVideoCaps(chId);
                if (caps)
                {
                    m_eventWriter->setVideoCaps(chId, caps);
                    gst_caps_unref(caps);
                }
            }
        }
        if (m_config->audioEnabled)
        {
            GstCaps* caps = getAudioCaps();
            if (caps)
            {
                m_eventWriter->setAudioCaps(caps);
                gst_caps_unref(caps);
            }
        }

        if ((m_eventWriter->initialize() == false) || (m_eventWriter->start() == false))
        {
            LOG_DVR_ERRORF("%sFailed to start event recording: %s%s",
                          LOG::Color::MAGENTA, item.outputPath.c_str(), LOG::Color::RESET);
            m_eventWriter.reset();
            m_eventWriterBusy = false;
            return;
        }
        
        m_currentEventData = item.eventData;
        m_eventStartTime = std::chrono::system_clock::now();
        
        // Write pre-event video/subtitle/audio — this establishes m_anchorPts for A/V sync.
        LOG_DVR_INFOF("%sWriting %zu pre-event frames%s",
                     LOG::Color::MAGENTA, item.preEventFrameCount, LOG::Color::RESET);

        std::vector<int> flushChannels = fileChannels;
        if (m_config->audioEnabled)
        {
            flushChannels.push_back(getEventAudioChIdx(0));
        }

        for (int chId : flushChannels)
        {
            auto it = m_eventBuffers.find(chId);
            if (it == m_eventBuffers.end())
            {
                continue;
            }

            size_t preFrames = 0;
            if (isAudioChannel(chId))
            {
                int audioFps = (m_config->audioSampleRate + 1023) / 1024;
                preFrames = audioFps * m_config->recordingTimePreEvent;
            }
            else
            {
                preFrames = m_channelFpsMap[chId] * m_config->recordingTimePreEvent;
            }
            auto frames = it->second->getLastFrames(preFrames);

            if (isSubtitleChannel(chId) && !frames.empty())
            {
                const auto& lastF = frames.back();
                std::string text(reinterpret_cast<const char*>(lastF->data.get()), lastF->size);
                std::lock_guard<std::mutex> subLock(m_subtitleMutex);
                m_lastSubMap[chId] = LastSubInfo{text, lastF->pts};
            }

            for (const auto& f : frames)
            {
                if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
                {
                    if (isAudioChannel(f->channelId))
                    {
                        m_eventWriter->writeAudio(f->data.get(), f->size, f->pts);
                    }
                    else
                    {
                        m_eventWriter->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
                    }
                }
            }

            it->second->resetReadIndex();
        }

        // Flush pre-event AAC from m_audioPreEventBuffer (anchor already set above).
        if ((m_config->audioEnabled == true) && (m_audioPreEventBuffer != nullptr))
        {
            auto aacFrames = m_audioPreEventBuffer->getLastFrames(0);
            for (const auto& f : aacFrames)
            {
                if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
                    m_eventWriter->writeAudio(f->data.get(), f->size, f->pts);
            }
        }
        
        logEvent("EVENT_START", item.eventData, item.outputPath);
        
        LOG_DVR_SUCCESSF("%sEvent recording started: %s (Priority: %d)%s",
                        LOG::Color::MAGENTA, item.outputPath.c_str(),
                        item.eventData.getPriority(), LOG::Color::RESET);
        
        m_eventWriterBusy = false;
    }

    void QDvrManager::stopCurrentEventRecording()
    {
        m_eventWriterBusy = true;
        std::lock_guard<std::mutex> lock(m_eventMutex);
        
        if (m_eventWriter == nullptr)
        {
            m_eventWriterBusy = false;
            return;
        }
        
        EventData eventData = m_currentEventData;
        std::string filePath = m_eventWriter->getOutputPath();
        
        m_eventWriter->stop();
        m_eventWriter.reset();
        
        logEvent("EVENT_STOP", eventData, filePath);
        
        LOG_DVR_SUCCESSF("%sEvent recording completed%s", LOG::Color::MAGENTA, LOG::Color::RESET);
        m_eventWriterBusy = false;
    }

    bool QDvrManager::shouldPreemptCurrentEvent(const EventData& newEvent)
    {
        if ((m_eventWriter == nullptr) || (m_eventWriter->isActive() == false))
        {
            return false;
        }
        
        int currentPrio = m_currentEventData.getPriority();
        int newPrio = newEvent.getPriority();
        
        if (newPrio < currentPrio)
        {
            return true;
        }
        
        if ((newPrio == 0) && (currentPrio == 0))
        {
            LOG_DVR_WARNINGF("%sMultiple CRITICAL events - preempting for latest%s",
                            LOG::Color::MAGENTA, LOG::Color::RESET);
            return true;
        }
        
        return false;
    }

    void QDvrManager::saveCurrentEventForResumption()
    {
        logEvent("EVENT_PREEMPTED", m_currentEventData, m_eventWriter->getOutputPath());
        
        LOG_DVR_WARNINGF("%sEvent preempted: %s (recorded %.1fs)%s",
                        LOG::Color::MAGENTA, m_currentEventData.getTypeName(),
                        std::chrono::duration<double>(std::chrono::system_clock::now() - m_eventStartTime).count(),
                        LOG::Color::RESET);
    }
#endif
