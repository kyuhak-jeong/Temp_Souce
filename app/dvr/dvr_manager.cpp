/*==========================================================================
 * dvr_manager.cpp - Base DVR Manager Implementation
 *==========================================================================*/
#include "dvr_manager.hpp"

/*--------------------------------------------------------------------------
 * Constructor / Destructor
 *--------------------------------------------------------------------------*/
DvrManager::DvrManager(std::shared_ptr<DvrConfig> config)
    : m_config(config), m_numFiles(0), m_isInitialized(false)
    , m_storageCleanupInProgress(false), m_dvrAudioCaps(nullptr)
{
    for (int i = 0; i < MAX_BUFFER_NUM; ++i)
    {
        m_dvrVideoCaps[i] = nullptr;
    }
    LOG_DVR_INFO("DvrManager created");
}

DvrManager::~DvrManager() { shutdown(); }

/*--------------------------------------------------------------------------
 * Lifecycle Functions
 *--------------------------------------------------------------------------*/
bool DvrManager::initialize(const std::vector<int>& channels, const std::map<int, int>& channelFpsMap)
{
    if (m_isInitialized == true) { LOG_DVR_WARNING("Already initialized"); return false; }
    if ((channels.size() == 0) || (channelFpsMap.size() == 0))
    { LOG_DVR_WARNING("Failed to initialize due to wrong channel size"); return false; }
    
    m_channels = channels;
    m_channelFpsMap = channelFpsMap;
    calculateNumFiles();
    
    // Create video/subtitle buffers for all channels.
    for (int chId : m_channels)
    {
        m_normalBuffers[chId] = std::make_unique<CircularBuffer>(chId, DEFAULT_BUFFER_MAX_COUNT);
        m_eventBuffers[chId]  = std::make_unique<CircularBuffer>(chId, m_channelFpsMap[chId] * m_config->recordingTimePreEvent);
    }

    // Create normal and event audio buffers (one per file for normal)
    if (m_config->audioEnabled == true)
    {
        for (int fileIdx = 0; fileIdx < m_numFiles; fileIdx++)
        {
            int normalAudioIdx = getNormalAudioChIdx(fileIdx);
            m_normalBuffers[normalAudioIdx] = std::make_unique<CircularBuffer>(normalAudioIdx, DEFAULT_BUFFER_MAX_COUNT * 2);
        }
        
        int eventAudioIdx = getEventAudioChIdx(0);
        int audioFps = (m_config->audioSampleRate + 1023) / 1024;
        m_eventBuffers[eventAudioIdx] = std::make_unique<CircularBuffer>(eventAudioIdx, audioFps * m_config->recordingTimePreEvent);
    }



    #ifdef USE_DVR_SUB
        int subFps = m_channelFpsMap[m_channels[0]];
        
        // Normal subtitle buffers (one per file)
        for (int fileIdx = 0; fileIdx < m_numFiles; fileIdx++)
        {
            int normalSubIdx = getNormalSubChIdx(fileIdx);
            m_normalBuffers[normalSubIdx] = std::make_unique<CircularBuffer>(normalSubIdx, DEFAULT_BUFFER_MAX_COUNT);
            m_channelFpsMap[normalSubIdx] = subFps;
        }
        
        // Event subtitle buffer (1 frame/sec, limit to recordingTimePreEvent to avoid stale frames)
        int eventSubIdx = getEventSubChIdx(0);
        m_eventBuffers[eventSubIdx] = std::make_unique<CircularBuffer>(eventSubIdx, m_config->recordingTimePreEvent);
        m_channelFpsMap[eventSubIdx] = subFps;
    #endif

    #ifdef USE_DVR_SUB
        {
            std::lock_guard<std::mutex> lock(m_subtitleMutex);
            m_lastSubMap.clear();
        }
    #endif
    
    m_isInitialized = true;
    
    std::stringstream info;
    for (size_t i = 0; i < m_channels.size(); i++)
    {
        info << "Ch" << m_channels[i] << ":" << m_channelFpsMap[m_channels[i]] << "fps";
        if (i < (m_channels.size() - 1)) { info << ", "; }
    }
    
    LOG_DVR_INFOF("Initialized with %d files: %s", m_numFiles, info.str().c_str());
    return true;
}

void DvrManager::shutdown()
{
    if (m_isInitialized == false) { return; }
    LOG_DVR_INFO("Shutting down...");
    m_isInitialized = false;

    m_channelFirstFrameSeen.clear();
    m_channelWaitStarted = false;


    
    // Move out under lock so we don't race with startEventRecording/pushEventData/pushEncodedData.
    std::shared_ptr<FileWriter> eventToStop;
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        eventToStop = std::move(m_eventWriter);
    }
    if (eventToStop) eventToStop->stop();

    cleanupNormalWriters();
    
    {
        std::lock_guard<std::mutex> lock(m_capsMutex);
        for (int i = 0; i < MAX_BUFFER_NUM; ++i)
        {
            if (m_dvrVideoCaps[i] != nullptr)
            {
                gst_caps_unref(m_dvrVideoCaps[i]);
                m_dvrVideoCaps[i] = nullptr;
            }
        }
        if (m_dvrAudioCaps != nullptr)
        {
            gst_caps_unref(m_dvrAudioCaps);
            m_dvrAudioCaps = nullptr;
        }
    }

    #ifdef USE_DVR_SUB
        {
            std::lock_guard<std::mutex> lock(m_subtitleMutex);
            m_lastSubMap.clear();
        }
    #endif

    LOG_DVR_SUCCESS("Shutdown complete");
}

/*--------------------------------------------------------------------------
 * Normal/Parking Recording
 *--------------------------------------------------------------------------*/

bool DvrManager::startNormalRecording()
{
    if (m_isInitialized == false) { LOG_DVR_ERROR("Not initialized"); return false; }

    // Check if any writer needs rotation
    bool needsRotation = false;
    {
        std::lock_guard<std::mutex> lock(m_normalMutex);
        for (auto& w : m_normalWriters)
        {
            if ((w != nullptr) && (w->isActive() == true) && (w->shouldRotate() == true))
            {
                needsRotation = true;
                break;
            }
        }
    }

    if (needsRotation == true)
    {
        LOG_DVR_INFO("File rotation needed");
        rotateNormalRecording();
        return true;
    }

    // Check if all writers are already active
    bool allActive = true;
    {
        std::lock_guard<std::mutex> lock(m_normalMutex);
        for (auto& w : m_normalWriters)
        {
            if ((w == nullptr) || (w->isActive() == false))
            {
                // sleepd ?
                allActive = false;
                break;
            }
        }
    }

    if (allActive == true) { return true; }

    // Defer recording until all channels have sent at least one frame (prevents track gaps caused by a camera that is slow to initialize).
    if (isAllChannelsReady() == false) { return false; }

    // Start new recording
    std::string outputDir = m_config->isParkingMode ? m_config->getParkingFullPath() : m_config->getNormalFullPath();
    std::string filePrefix = m_config->isParkingMode ? "P_" : "N_";

    LOG_DVR_INFOF("Starting %s recording", m_config->isParkingMode ? "PARKING" : "NORMAL");

    std::filesystem::create_directories(outputDir);
    std::string dateTime = getTimeString();

    for (int fileIdx = 0; fileIdx < m_numFiles; fileIdx++)
    {
        {
            std::lock_guard<std::mutex> lock(m_normalMutex);
            if ((m_normalWriters[fileIdx] != nullptr) && (m_normalWriters[fileIdx]->isActive() == true)) { continue; }
        }

        // Build channel list for this file
        std::vector<int> fileChannels;

        #ifdef USE_DVR_SPLIT
            int start = fileIdx * DVR_CHANNELS_PER_FILE;
            int end = std::min(static_cast<int>(start + DVR_CHANNELS_PER_FILE), static_cast<int>(m_channels.size()));
            for (int i = start; i < end; i++) { fileChannels.push_back(m_channels[i]); }
        #else
            fileChannels = m_channels;
        #endif

        if (fileChannels.empty() == true) { continue; }

        std::string audioSuffix = m_config->audioEnabled ? "_A" : "";
        std::string filename = outputDir + "/" + filePrefix + dateTime + "_#" + std::to_string(fileIdx) +
                              "_CH" + std::to_string(fileChannels.size()) +
                              "_FR" + std::to_string(m_channelFpsMap[fileChannels[0]]) +
                              audioSuffix + ".mp4";

        #ifdef USE_DVR_SUB
            fileChannels.push_back(getNormalSubChIdx(fileIdx));
            {
                std::lock_guard<std::mutex> lock(m_subtitleMutex);
                m_lastSubMap[getNormalSubChIdx(fileIdx)] = LastSubInfo{"", GST_CLOCK_TIME_NONE};
            }
        #endif

        auto newWriter = std::make_shared<FileWriter>(filename, m_config, RecordingMode::NORMAL, fileChannels, m_channelFpsMap);

        for (int chId : fileChannels)
        {
            if (isVideoChannel(chId))
            {
                GstCaps* caps = getVideoCaps(chId);
                if (caps)
                {
                    newWriter->setVideoCaps(chId, caps);
                    gst_caps_unref(caps);
                }
            }
        }
        if (m_config->audioEnabled)
        {
            GstCaps* caps = getAudioCaps();
            if (caps)
            {
                newWriter->setAudioCaps(caps);
                gst_caps_unref(caps);
            }
        }

        if ((newWriter->initialize() == false) || (newWriter->start() == false))
        {
            LOG_DVR_ERRORF("Failed to start file %d", fileIdx); // never reach here snap:
            return false;
        }



        { // snap: check needed
            std::lock_guard<std::mutex> lock(m_normalMutex);
            m_normalWriters[fileIdx] = std::move(newWriter);
        }
    }

    return true;
}

void DvrManager::stopNormalRecording() 
{
    cleanupNormalWriters(); 
}

void DvrManager::rotateNormalRecording()
{
    LOG_DVR_INFO("Rotating recording files");

    for (auto& bp : m_normalBuffers)
        bp.second->clearReadFrames();

    cleanupNormalWriters();
    calculateNumFiles();
    startNormalRecording();
}

void DvrManager::cleanupNormalWriters()
{
    std::vector<std::shared_ptr<FileWriter>> toStop;
    {
        std::lock_guard<std::mutex> lock(m_normalMutex);
        for (auto& w : m_normalWriters)
            if (w != nullptr) toStop.push_back(std::move(w));
        m_normalWriters.clear();
    }
    for (auto& w : toStop) w->stop();
}

/*--------------------------------------------------------------------------
 * Event Recording
 *--------------------------------------------------------------------------*/
bool DvrManager::startEventRecording(const EventData& eventData)
{
    if (m_isInitialized == false) 
    { 
        LOG_DVR_ERROR("Not initialized"); 
        return false; 
    }

    // Fast early-exit if event recording is already active
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
        {
            LOG_DVR_WARNINGF("%sEvent recording already active%s", 
                           LOG::Color::MAGENTA, LOG::Color::RESET);
            return true;
        }
    }

    // Build output path
    std::string outputDir = m_config->getEventFullPath() + "/" + eventData.getTypeName();
    std::filesystem::create_directories(outputDir);

    std::string audioSuffix = m_config->audioEnabled ? "_A" : "";
    std::string filename = outputDir + "/E_" + getTimeString() +
                          "_CH" + std::to_string(m_channels.size()) +
                          "_FR" + std::to_string(m_channelFpsMap[m_channels[0]]) +
                          audioSuffix + ".mp4";

    std::vector<int> fileChannels = m_channels;
#ifdef USE_DVR_SUB
    fileChannels.push_back(getEventSubChIdx(0));
    {
        std::lock_guard<std::mutex> lock(m_subtitleMutex);
        m_lastSubMap[getEventSubChIdx(0)] = LastSubInfo{"", GST_CLOCK_TIME_NONE};
    }
#endif

    // Create writer (without holding lock)
    auto localWriter = std::make_shared<FileWriter>(filename, m_config,
                           RecordingMode::EVENT, fileChannels, m_channelFpsMap);

    for (int chId : fileChannels)
    {
        if (isVideoChannel(chId))
        {
            GstCaps* caps = getVideoCaps(chId);
            if (caps)
            {
                localWriter->setVideoCaps(chId, caps);
                gst_caps_unref(caps);
            }
        }
    }
    if (m_config->audioEnabled)
    {
        GstCaps* caps = getAudioCaps();
        if (caps)
        {
            localWriter->setAudioCaps(caps);
            gst_caps_unref(caps);
        }
    }

    auto t_total  = std::chrono::steady_clock::now();
    auto elapsedMs = [&t_total]() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - t_total).count();
    };

    // Initialize & Start
    auto t_init = std::chrono::steady_clock::now();
    bool initOk = localWriter->initialize();
    LOG_DVR_INFOF("[TIMING] initialize: %lldms", 
                  (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t_init).count());

    if (initOk == false)
    {
        LOG_DVR_ERRORF("%sFailed to initialize event recording%s", 
                      LOG::Color::MAGENTA, LOG::Color::RESET);
        return false;
    }

    auto t_start = std::chrono::steady_clock::now();
    bool startOk = localWriter->start();
    LOG_DVR_INFOF("[TIMING] start: %lldms", 
                  (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t_start).count());

    if (startOk == false)
    {
        LOG_DVR_ERRORF("%sFailed to start event recording%s", 
                      LOG::Color::MAGENTA, LOG::Color::RESET);
        return false;
    }

    // Flush pre-event video/subtitle/audio frames sorted by PTS (NO LOCK)
    size_t totalVideoFrames = 0;
    auto t_vflush = std::chrono::steady_clock::now();

    std::vector<int> flushChannels = fileChannels;
    if (m_config->audioEnabled)
    {
        flushChannels.push_back(getEventAudioChIdx(0));
    }

    std::vector<std::shared_ptr<FrameData>> allPreEventFrames;

    for (int chId : flushChannels)
    {
        auto it = m_eventBuffers.find(chId);
        if (it == m_eventBuffers.end()) continue;

        auto frames = it->second->getAllFrames();        // deep clone
        allPreEventFrames.insert(allPreEventFrames.end(), frames.begin(), frames.end());
        it->second->resetReadIndex();   // mark as consumed
    }

    // Sort all pre-event frames across channels by timestamp (PTS) to guarantee correct interleaving
    std::sort(allPreEventFrames.begin(), allPreEventFrames.end(), [](const auto& a, const auto& b) {
        return a->pts < b->pts;
    });

    // Find the first video frame PTS to use as a sync reference for audio/subtitle trimming
    GstClockTime videoFirstPts = GST_CLOCK_TIME_NONE;
    for (const auto& f : allPreEventFrames)
    {
        if (isVideoChannel(f->channelId))
        {
            videoFirstPts = f->pts;
            break;
        }
    }

    GstClockTime lastSubPts = GST_CLOCK_TIME_NONE;
    std::string lastSubText = "";
    int eventSubCh = getEventSubChIdx(0);

    for (const auto& f : allPreEventFrames)
    {
        if (isSubtitleChannel(f->channelId))
        {
            if (GST_CLOCK_TIME_IS_VALID(videoFirstPts) && f->pts < videoFirstPts) continue;
            if (lastSubPts == GST_CLOCK_TIME_NONE || f->pts > lastSubPts)
            {
                lastSubPts = f->pts;
                lastSubText = std::string(reinterpret_cast<const char*>(f->data.get()), f->size);
            }
        }
    }

    {
        std::lock_guard<std::mutex> subLock(m_subtitleMutex);
        if (lastSubPts != GST_CLOCK_TIME_NONE)
        {
            m_lastSubMap[eventSubCh] = LastSubInfo{lastSubText, lastSubPts};
        }
        else
        {
            m_lastSubMap[eventSubCh] = LastSubInfo{"", GST_CLOCK_TIME_NONE};
        }
    }

    size_t processedFrames = 0;
    for (const auto& f : allPreEventFrames)
    {
        if (isAudioChannel(f->channelId))
        {
            // Drop audio frames that predate the first video frame to ensure clean start alignment
            if (GST_CLOCK_TIME_IS_VALID(videoFirstPts) && f->pts < videoFirstPts) continue;
            localWriter->writeAudio(f->data.get(), f->size, f->pts);
            processedFrames++;
        }
        else if (isSubtitleChannel(f->channelId))
        {
            // Drop subtitle frames that predate the first video frame to ensure clean start alignment
            if (GST_CLOCK_TIME_IS_VALID(videoFirstPts) && f->pts < videoFirstPts) continue;
            localWriter->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
            processedFrames++;
        }
        else
        {
            localWriter->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
            processedFrames++;
        }
    }
    totalVideoFrames = processedFrames;

    LOG_DVR_INFOF("[TIMING] video/audio/subtitle flush (%zu frames): %lldms",
                  totalVideoFrames,
                  (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t_vflush).count());

    // Atomically install the writer
    std::shared_ptr<FileWriter> oldWriter = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        
        // Double check (race condition 방지)
        if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
        {
            LOG_DVR_WARNINGF("Another event started during setup, discarding this one");
            localWriter->stop();
            return true;
        }

        oldWriter = std::move(m_eventWriter);   // 이전 writer 정리 준비
        m_eventWriter = localWriter;
        m_currentEventData = eventData;
        m_eventStartTime = std::chrono::system_clock::now();
    }

    // 이전 writer 정리 (lock 영역 밖에서 안전하게)
    if (oldWriter)
    {
        LOG_DVR_INFO("Stopping previous event writer");
        oldWriter->stop();
    }

    LOG_DVR_INFOF("[TIMING] startEventRecording total: %lldms", 
                  (long long)elapsedMs());
    
    logEvent("EVENT_START", eventData, filename);

    return true;
}


// /*--------------------------------------------------------------------------
//  * Event Recording
//  *--------------------------------------------------------------------------*/
// bool DvrManager::startEventRecording(const EventData& eventData)
// {
//     if (m_isInitialized == false) 
//     { 
//         LOG_DVR_ERROR("Not initialized"); return false; 
//     }

//     // Fast early-exit if event recording is already active (brief lock, no work done).
//     {
//         std::lock_guard<std::mutex> lock(m_eventMutex);
//         if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
//         {
//             LOG_DVR_WARNINGF("%sEvent recording already active%s", LOG::Color::MAGENTA, LOG::Color::RESET);
//             return true;
//         }
//     }

//     // Build output path — local variables only, no shared state touched.
//     std::string outputDir = m_config->getEventFullPath() + "/" + eventData.getTypeName();
//     std::filesystem::create_directories(outputDir);

//     std::string audioSuffix = m_config->audioEnabled ? "_A" : "";
//     std::string filename = outputDir + "/E_" + getTimeString() +
//                           "_CH" + std::to_string(m_channels.size()) +
//                           "_FR" + std::to_string(m_channelFpsMap[m_channels[0]]) +
//                           audioSuffix + ".mp4";

//     std::vector<int> fileChannels = m_channels;
//     #ifdef USE_DVR_SUB
//         fileChannels.push_back(getEventSubChIdx(0));
//     #endif

//     // Create, initialise and start the writer WITHOUT holding m_eventMutex.
//     // This includes the GStreamer PLAYING-state wait (Fix 4) which can take
//     // hundreds of milliseconds — releasing the lock here prevents that wait
//     // from blocking the GStreamer streaming threads.
//     auto localWriter = std::make_shared<FileWriter>(filename, m_config, RecordingMode::EVENT, fileChannels, m_channelFpsMap);

//     auto t_total  = std::chrono::steady_clock::now();

//     auto elapsedMs = [&t_total]() 
//     {
//         return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_total).count();
//     };

//     auto t_init = std::chrono::steady_clock::now();
    
//     bool initOk = localWriter->initialize();

//     LOG_DVR_INFOF("[TIMING] initialize: %lldms", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_init).count());

//     if (initOk == false)
//     {
//         LOG_DVR_ERRORF("%sFailed to start event recording%s", LOG::Color::MAGENTA, LOG::Color::RESET);
//         return false;
//     }

//     auto t_start = std::chrono::steady_clock::now();
//     bool startOk = localWriter->start();

//     LOG_DVR_INFOF("[TIMING] start: %lldms", (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_start).count());

//     if (startOk == false)
//     {
//         LOG_DVR_ERRORF("%sFailed to start event recording%s", LOG::Color::MAGENTA, LOG::Color::RESET);
//         return false;
//     }

//     // Flush pre-event video WITHOUT holding m_eventMutex.
//     // localWriter is not yet visible to other threads, so no synchronisation is
//     // needed here. Streaming threads see m_eventWriter == nullptr during this
//     // phase and simply push to m_eventBuffers (circular buffer) without blocking.
//     // Video frames are flushed first to establish m_anchorPts inside FileWriter.
//     size_t totalVideoFrames = 0;
//     auto t_vflush = std::chrono::steady_clock::now();
//     for (int chId : fileChannels)
//     {
//         auto it = m_eventBuffers.find(chId);
//         if (it == m_eventBuffers.end()) continue;

//         auto frames = it->second->getAllFrames();
//         for (const auto& f : frames)
//             localWriter->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
//         totalVideoFrames += frames.size();
//         it->second->resetReadIndex();
//     }
//     LOG_DVR_INFOF("[TIMING] video flush (%zu frames): %lldms",
//                   totalVideoFrames,
//                   (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
//                       std::chrono::steady_clock::now() - t_vflush).count());

//     // Anchor is now set — flush pre-event AAC from AudioManager's ring buffer.
//     auto t_aflush = std::chrono::steady_clock::now();
//     size_t totalAudioFrames = 0;
//     if ((m_config->audioEnabled == true) && (m_audioManager != nullptr))
//     {
//         auto aacFrames = m_audioManager->getPreEventFrames();
//         for (const auto& f : aacFrames)
//             localWriter->writeAudio(f->data.get(), f->size, f->pts);
//         totalAudioFrames = aacFrames.size();
//     }
//     LOG_DVR_INFOF("[TIMING] audio flush (%zu frames): %lldms",
//                   totalAudioFrames,
//                   (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
//                       std::chrono::steady_clock::now() - t_aflush).count());

//     // Atomically install the ready writer. m_eventMutex is held for only the
//     // duration of a pointer swap — streaming threads are not meaningfully blocked.
//     {
//         std::lock_guard<std::mutex> lock(m_eventMutex);
//         // Double-check: another event may have started while we were setting up.
//         if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
//         {
//             localWriter->stop();
//             return true;
//         }
//         m_eventWriter = localWriter;
//         m_currentEventData = eventData;
//         m_eventStartTime = std::chrono::system_clock::now();
//     }

//     LOG_DVR_INFOF("[TIMING] startEventRecording total: %lldms", (long long)elapsedMs());
//     logEvent("EVENT_START", eventData, filename);
//     return true;
// }

void DvrManager::stopEventRecording()
{
    std::lock_guard<std::mutex> lock(m_eventMutex);
    if (m_eventWriter == nullptr) { return; }
    
    EventData eventData = m_currentEventData;
    std::string filePath = m_eventWriter->getOutputPath();
    
    logEvent("EVENT_STOP", eventData, filePath);
    
    m_eventWriter->stop();
    m_eventWriter.reset();
    
    LOG_DVR_SUCCESSF("%sEvent recording stopped%s", LOG::Color::MAGENTA, LOG::Color::RESET);
    //usleep(5*1000);
}

/*--------------------------------------------------------------------------
 * Data Management
 *--------------------------------------------------------------------------*/
void DvrManager::pushEncodedData(int channelId, const uint8_t* data, size_t size, GstClockTime pts)
{
    if (m_isInitialized == false) { DVR_DEBUG("Not initialized"); return; }

    if (isAudioChannel(channelId))
    {
        if (m_config->audioEnabled == false) return;

        // Push to normal audio buffers (one per file)
        for (int fileIdx = 0; fileIdx < m_numFiles; fileIdx++)
        {
            int normalAudioIdx = getNormalAudioChIdx(fileIdx);
            std::shared_ptr<FileWriter> normalWriter;
            {
                std::lock_guard<std::mutex> lock(m_normalMutex);
                if (fileIdx < static_cast<int>(m_normalWriters.size()))
                    normalWriter = m_normalWriters[fileIdx];
            }
            pushNormalData(normalWriter.get(), normalAudioIdx, data, size, pts);
        }

        // Push to event audio buffer
        int eventAudioIdx = getEventAudioChIdx(0);
        pushEventData(eventAudioIdx, data, size, pts);
        
        return;
    }

    // Track per-channel first frame for all-channels-ready guard
    if (m_channelFirstFrameSeen.count(channelId) == 0)
    {
        m_channelFirstFrameSeen.insert(channelId);
        LOG_DVR_INFOF("First frame: ch%d (%zu/%zu channels ready)",
                      channelId, m_channelFirstFrameSeen.size(), m_channels.size());
    }

    // Push to normal recording — capture shared_ptr under lock to avoid UAF with cleanupNormalWriters
    {
        std::shared_ptr<FileWriter> normalWriter;
        {
            std::lock_guard<std::mutex> lock(m_normalMutex);
            int fileIdx = getFileIndexForChannel(channelId);
            if ((fileIdx >= 0) && (fileIdx < static_cast<int>(m_normalWriters.size())))
                normalWriter = m_normalWriters[fileIdx];
        }
        if (normalWriter)
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

                std::vector<std::shared_ptr<FileWriter>> writers;
                {
                    std::lock_guard<std::mutex> lock(m_normalMutex);
                    writers = m_normalWriters;
                }
                for (int i = 0; i < static_cast<int>(writers.size()); i++)
                {
                    if ((writers[i] != nullptr) && (writers[i]->isActive() == true))
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
                            pushNormalData(writers[i].get(), subChId,
                                          reinterpret_cast<const uint8_t*>(subtitleText.c_str()),
                                          subtitleText.size(), pts);
                        }
                    }
                }
            }
        #endif
    }

    // Push to event recording
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
}

void DvrManager::pushNormalData(FileWriter* writer, int channelId, const uint8_t* data, size_t size, GstClockTime pts)
{
    if (writer == nullptr)
    {
        DVR_DEBUGF("Invalid normal writer for channel %d", channelId);
        return;
    }
    
    auto it = m_normalBuffers.find(channelId);
    if (it == m_normalBuffers.end())
    {
        DVR_DEBUGF("Normal buffer not found for channel %d", channelId);
        return;
    }

    CircularBuffer* buffer = it->second.get();
    
    if (writer == nullptr)
    {
        buffer->push(data, size, pts);
        return;
    }
    
    if (writer->isActive() == true)
    {
        // Write pending unread frames first
        size_t unread = buffer->getUnreadCount();
        if (unread > 0)
        {
            auto frames = buffer->getUnreadFrames();
            size_t written = 0;
            
            for (const auto& f : frames)
            {
                // For subtitle/audio: get reference video channel for this file
                int refChannelId = f->channelId;
                
                if (isSubtitleChannel(f->channelId) == true || isAudioChannel(f->channelId) == true)
                {
                    int fileIdx = isSubtitleChannel(f->channelId) 
                                ? (f->channelId - NORMAL_SUB_CH_BASE)
                                : (f->channelId - NORMAL_AUDIO_CH_BASE);
                    
                    #ifdef USE_DVR_SPLIT
                        // Reference is first channel of this file
                        refChannelId = fileIdx * DVR_CHANNELS_PER_FILE;
                    #else
                        // Reference is first channel
                        refChannelId = m_channels[0];
                    #endif
                }
                
                // For video channels: stop if reached target frame count
                // For subtitle channels: stop if reference video channel reached target
                if (writer->shouldRotateChannel(refChannelId) == true) break;
                
                bool ok = false;
                if (isAudioChannel(f->channelId))
                {
                    ok = writer->writeAudio(f->data.get(), f->size, f->pts);
                }
                else
                {
                    ok = writer->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
                }

                if (ok == true) { written++; }
                else { break; }
            }
            
            buffer->markAsRead(written);
            buffer->clearReadFrames();
            
            // If not all frames written, buffer the new one
            if (written < frames.size())
            {
                buffer->push(data, size, pts);
                return;
            }
        }
        
        // Determine reference channel for rotation check
        int refChannelId = channelId;
        
        if (isSubtitleChannel(channelId) == true || isAudioChannel(channelId) == true)
        {
            int fileIdx = isSubtitleChannel(channelId)
                        ? (channelId - NORMAL_SUB_CH_BASE)
                        : (channelId - NORMAL_AUDIO_CH_BASE);
            #ifdef USE_DVR_SPLIT
                refChannelId = fileIdx * DVR_CHANNELS_PER_FILE;
            #else
                refChannelId = m_channels[0];
            #endif
        }
        
        if (writer->shouldRotateChannel(refChannelId) == true)
        {
            buffer->push(data, size, pts);
        }
        else
        {
            bool ok = false;
            if (isAudioChannel(channelId))
            {
                ok = writer->writeAudio(data, size, pts);
            }
            else
            {
                ok = writer->writeFrame(channelId, data, size, pts);
            }

            if (ok == false)
            {
                buffer->push(data, size, pts);
            }
        }
    }
    else
    {
        buffer->push(data, size, pts);
    }
}

void DvrManager::pushEventData(int channelId, const uint8_t* data, size_t size, GstClockTime pts)
{
    auto it = m_eventBuffers.find(channelId);
    if (it == m_eventBuffers.end())
    {
        DVR_DEBUGF("Event buffer not found for channel %d", channelId);
        return;
    }

    CircularBuffer* buffer = it->second.get();
    buffer->push(data, size, pts);

    bool shouldStop = false;

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        
        if ((m_eventWriter != nullptr) && (m_eventWriter->isActive() == true))
        {
            size_t unread = buffer->getUnreadCount();
            if (unread > 0)
            {
                auto frames = buffer->getUnreadFrames();
                size_t written = 0;
                
                for (const auto& f : frames)
                {
                    if (m_eventWriter->shouldRotate() == true) break;
                    
                    bool ok = false;
                    if (isAudioChannel(f->channelId))
                    {
                        ok = m_eventWriter->writeAudio(f->data.get(), f->size, f->pts);
                    }
                    else
                    {
                        ok = m_eventWriter->writeFrame(f->channelId, f->data.get(), f->size, f->pts);
                    }

                    if (ok == true) { written++; }
                    else { break; }
                }
                
                buffer->markAsRead(written);
            }
            
            if (m_eventWriter->shouldRotate() == true) shouldStop = true;
        }
    }
    
    if (shouldStop == true) stopEventRecording();
}

#ifdef USE_DVR_SUB
    void DvrManager::setSubtitleText(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(m_subtitleMutex);
        m_subtitleText = text;
    }
#endif


/*--------------------------------------------------------------------------
 * Storage Management
 *--------------------------------------------------------------------------*/
void DvrManager::checkAndCleanStorage()
{
    if (m_isInitialized == false) return;

    bool expected = false;
    if (m_storageCleanupInProgress.compare_exchange_strong(expected, true) == false)
    {
        return;
    }

    struct CleanupGuard
    {
        std::atomic<bool>& flag;
        ~CleanupGuard() { flag.store(false); }
    } guard{m_storageCleanupInProgress};

    LOG_DVR_INFO("Checking Storage");
    
    struct FolderInfo
    {
        std::string path;
        std::string name;
        long        maxSize;
        long        currentSize;
        int         cleanCount;
    };
    
    std::vector<FolderInfo> folders =
    {
        {m_config->getNormalFullPath(),  "Normal",  m_config->maxSizeOfNormalFolder,  0, 0},
        {m_config->getEventFullPath(),   "Event",   m_config->maxSizeOfEventFolder,   0, 0},
        {m_config->getParkingFullPath(), "Parking", m_config->maxSizeOfParkingFolder, 0, 0}
    };
    
    bool anyOverLimit = false;
    for (auto& f : folders)
    {
        f.currentSize = getFolderSize(f.path);
        if ((f.currentSize * 100 / f.maxSize) > 95)
        {
            anyOverLimit = true;
        }
    }
    
    if (anyOverLimit == false)
    {
        return;
    }
    
    std::stringstream logRet;
    
    for (auto& f : folders)
    {
        while ((f.currentSize * 100 / f.maxSize) > 95)
        {
            logRet << cleanOldestFile(f.path);
            f.cleanCount++;
            f.currentSize = getFolderSize(f.path);
        }
    }
    
    LOG_DVR_INFO(logRet.str().c_str());
}

long DvrManager::getFolderSize(const std::string& path)
{
    if (std::filesystem::exists(path) == false)
    {
        return 0;
    }
    
    long totalSize = 0;
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(path, std::filesystem::directory_options::skip_permission_denied))
        {
            try
            {
                if (entry.is_symlink())
                {
                    continue;
                }
                if (entry.is_regular_file() == true)
                {
                    totalSize += std::filesystem::file_size(entry.path());
                }
                else if (entry.is_directory() == true)
                {
                    totalSize += getFolderSize(entry.path().string());
                }
            }
            catch (const std::exception& e)
            {
                // Ignore errors for individual entries (e.g. files deleted or permissions)
            }
        }
    }
    catch (const std::exception& e)
    {
        LOG_DVR_WARNINGF("Exception in getFolderSize: %s", e.what());
    }
    return totalSize;
}


struct FileEntry
{
    std::filesystem::path path;
    std::filesystem::file_time_type writeTime;
};

static void collectFilesRecursive(const std::filesystem::path& p, std::vector<FileEntry>& files)
{
    try
    {
        if (std::filesystem::exists(p) == false) return;
        for (const auto& entry : std::filesystem::directory_iterator(p, std::filesystem::directory_options::skip_permission_denied))
        {
            try
            {
                if (entry.is_symlink())
                {
                    continue;
                }
                if (entry.is_regular_file() == true)
                {
                    std::filesystem::path filePath = entry.path();
                    std::filesystem::file_time_type wTime = std::filesystem::last_write_time(filePath);
                    files.push_back({filePath, wTime});
                }
                else if (entry.is_directory() == true)
                {
                    collectFilesRecursive(entry.path(), files);
                }
            }
            catch (const std::exception& e)
            {
                // Ignore individual entry errors
            }
        }
    }
    catch (const std::exception& e)
    {
        // Ignore directory access errors
    }
}

std::string DvrManager::cleanOldestFile(const std::string& path)
{
    std::stringstream ret;
    
    if (std::filesystem::exists(path) == false)
    {
        ret << " Path does not exist: " << path << "\n";
        return ret.str();
    }
    
    std::vector<FileEntry> files;
    collectFilesRecursive(path, files);
    
    if (files.empty() == true)
    {
        ret << " No files to delete in: " << path << "\n";
        return ret.str();
    }
    
    std::sort(files.begin(), files.end(), [](const FileEntry& a, const FileEntry& b)
    {
        return a.writeTime < b.writeTime;
    });
    
    const auto& oldest = files.front().path;
    std::filesystem::path parentDir = oldest.parent_path();
    
    try
    {
        std::filesystem::remove(oldest);
        sync();
        ret << " Oldest file deleted: " << oldest << "\n";
    }
    catch (const std::exception& e)
    {
        ret << " Failed to remove oldest file: " << oldest << " (" << e.what() << ")\n";
    }
    
    // Clean up empty subdirectories left behind (e.g. Event/MANUAL/ with no files)
    try
    {
        if (parentDir != std::filesystem::path(path) &&
            std::filesystem::exists(parentDir) == true &&
            std::filesystem::is_empty(parentDir) == true)
        {
            std::filesystem::remove(parentDir);
            ret << " Empty directory removed: " << parentDir << "\n";
        }
    }
    catch (const std::exception& e)
    {
        // Ignore empty directory removal errors
    }

    return ret.str();
}

/*--------------------------------------------------------------------------
 * Utility Functions
 *--------------------------------------------------------------------------*/
bool DvrManager::isAllChannelsReady()
{
    bool allSeen = true;

    for (int ch : m_channels)
    {
        if (m_channelFirstFrameSeen.count(ch) == 0) { allSeen = false; break; }
    }

    if (allSeen == true)
    {
        m_channelWaitStarted = false;
        return true;
    }

    if (m_channelWaitStarted == false)
    {
        m_channelWaitStart   = std::chrono::steady_clock::now();
        m_channelWaitStarted = true;

        LOG_DVR_INFOF("Waiting for all channels (%zu/%zu ready, timeout %ds)...",
                      m_channelFirstFrameSeen.size(), m_channels.size(),
                      CHANNEL_WAIT_TIMEOUT_MS / 1000);
    }

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_channelWaitStart).count();

    if (ms >= CHANNEL_WAIT_TIMEOUT_MS)
    {
        LOG_DVR_WARNINGF("Channel wait timeout (%dms). Starting with %zu/%zu channels ready", CHANNEL_WAIT_TIMEOUT_MS, m_channelFirstFrameSeen.size(), m_channels.size());

        m_channelWaitStarted = false;
        return true;
    }

    return false;
}

std::string DvrManager::getTimeString(bool forFilename) const
{
    return TimeUtils::getCurrentTimeString(forFilename);
}

void DvrManager::calculateNumFiles()
{
    std::lock_guard<std::mutex> lock(m_normalMutex);
    #ifdef USE_DVR_SPLIT
        m_numFiles = (m_channels.size() + DVR_CHANNELS_PER_FILE - 1) / DVR_CHANNELS_PER_FILE;
        m_normalWriters.resize(m_numFiles);
        LOG_DVR_INFOF("Split mode: %d files (%zu channels per file)", m_numFiles, DVR_CHANNELS_PER_FILE);
    #else
        m_numFiles = 1;
        m_normalWriters.resize(1);
        LOG_DVR_INFOF("Single file mode (%zu channels)", m_channels.size());
    #endif
}

int DvrManager::getFileIndexForChannel(int channelId) const
{
    #ifdef USE_DVR_SPLIT
        return channelId / DVR_CHANNELS_PER_FILE;
    #else
        return 0;
    #endif
}

void DvrManager::logEvent(const std::string& logType, const EventData& eventData, const std::string& filePath)
{
    std::string logPath;
    
    if ((logType == "EVENT_START") || (logType == "EVENT_STOP") || (logType == "EVENT_QUEUE") || (logType == "EVENT_PREEMPTED"))
    {
        logPath = m_config->getEventFullPath() + "/event_log.txt";
    }
    else
    {
        logPath = m_config->getNormalFullPath() + "/recording_log.txt";
    }
    
    std::filesystem::path logDir = std::filesystem::path(logPath).parent_path();
    std::filesystem::create_directories(logDir);
    
    std::ofstream logFile(logPath, std::ios::app);
    if (logFile.is_open() == false)
    {
        LOG_DVR_ERRORF("Failed to open log file: %s", logPath.c_str());
        return;
    }
    
    // Write header if file is empty
    std::ifstream check(logPath);
    check.seekg(0, std::ios::end);
    if (check.tellg() == 0)
    {
        logFile << "==========================================================================\n"
                << "                         DVR EVENT LOG\n"
                << "==========================================================================\n"
                << std::left << std::setw(20) << "Time"
                << std::setw(10) << "Action"
                << std::setw(12) << "Type"
                << std::setw(8)  << "Prio."
                << std::setw(12) << "Pre(s)"
                << std::setw(12) << "Duration(s)"
                << "Details\n"
                << std::string(120, '-') << "\n";
    }
    check.close();
    
    std::string timeStr = getTimeString(false);
    std::string filename = std::filesystem::path(filePath).filename().string();
    
    if (logType == "EVENT_START")
    {
        int pre = m_config->recordingTimePreEvent;
        int total = m_config->recordingTimeTotalEvent;
        
        logFile << std::left << std::setw(20) << timeStr
                << std::setw(10) << "START"
                << std::setw(12) << eventData.getTypeName()
                << std::setw(8)  << eventData.getPriority()
                << std::setw(12) << std::to_string(pre)
                << std::setw(12) << (std::to_string(pre) + "+" + std::to_string(total - pre))
                << filename;
        
        if (eventData.additionalData.empty() == false)
        {
            logFile << " | " << eventData.additionalData;
        }
        
        logFile << "\n";
    }
    else if (logType == "EVENT_STOP")
    {
        auto dur = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - m_eventStartTime);
        
        logFile << std::left << std::setw(20) << timeStr
                << std::setw(10) << "COMPLETED"
                << std::setw(12) << eventData.getTypeName()
                << std::setw(8)  << eventData.getPriority()
                << std::setw(12) << "-"
                << std::setw(12) << std::to_string(dur.count())
                << "Recording finished successfully\n";
    }
    else if (logType == "EVENT_QUEUE")
    {
        int pre = m_config->recordingTimePreEvent;
        int total = m_config->recordingTimeTotalEvent;
        
        logFile << std::left << std::setw(20) << timeStr
                << std::setw(10) << "QUEUED"
                << std::setw(12) << eventData.getTypeName()
                << std::setw(8)  << eventData.getPriority()
                << std::setw(12) << std::to_string(pre)
                << std::setw(12) << (std::to_string(pre) + "+" + std::to_string(total - pre))
                << filename;
        
        if (eventData.additionalData.empty() == false)
        {
            logFile << " | " << eventData.additionalData;
        }
        
        logFile << "\n";
    }
    else if (logType == "EVENT_PREEMPTED")
    {
        auto dur = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - m_eventStartTime);
        
        logFile << std::left << std::setw(20) << timeStr
                << std::setw(10) << "PREEMPTED"
                << std::setw(12) << eventData.getTypeName()
                << std::setw(8)  << eventData.getPriority()
                << std::setw(12) << "-"
                << std::setw(12) << std::to_string(dur.count())
                << "Event interrupted by higher priority event | " << filename << "\n";
    }
    
    logFile.flush();
    logFile.close();
}

void DvrManager::storeVideoCaps(int channelId, GstCaps* caps)
{
    if (channelId < 0 || channelId >= MAX_BUFFER_NUM) return;
    std::lock_guard<std::mutex> lock(m_capsMutex);
    if (caps)
    {
        if (m_dvrVideoCaps[channelId])
        {
            if (m_dvrVideoCaps[channelId] == caps) return; // Prevent self-assignment double free
            if (gst_caps_is_equal(m_dvrVideoCaps[channelId], caps))
            {
                return;
            }
            gst_caps_unref(m_dvrVideoCaps[channelId]);
        }
        m_dvrVideoCaps[channelId] = gst_caps_copy(caps);
    }
}

void DvrManager::storeAudioCaps(GstCaps* caps)
{
    std::lock_guard<std::mutex> lock(m_capsMutex);
    if (caps)
    {
        if (m_dvrAudioCaps)
        {
            if (m_dvrAudioCaps == caps) return; // Prevent self-assignment double free
            if (gst_caps_is_equal(m_dvrAudioCaps, caps))
            {
                return;
            }
            gst_caps_unref(m_dvrAudioCaps);
        }
        m_dvrAudioCaps = gst_caps_copy(caps);
    }
}

GstCaps* DvrManager::getVideoCaps(int channelId) const
{
    if (channelId < 0 || channelId >= MAX_BUFFER_NUM) return nullptr;
    std::lock_guard<std::mutex> lock(m_capsMutex);
    return m_dvrVideoCaps[channelId] ? gst_caps_ref(m_dvrVideoCaps[channelId]) : nullptr;
}

GstCaps* DvrManager::getAudioCaps() const
{
    std::lock_guard<std::mutex> lock(m_capsMutex);
    return m_dvrAudioCaps ? gst_caps_ref(m_dvrAudioCaps) : nullptr;
}