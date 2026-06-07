/*==========================================================================
 * file_writer.cpp - Video/Audio File Writing Implementation
 *==========================================================================*/
#include "file_writer.hpp"

// std::mutex FileWriter::s_gstInitMutex;

/*--------------------------------------------------------------------------
 * Constructor / Destructor
 *--------------------------------------------------------------------------*/
FileWriter::FileWriter(const std::string& outputPath, std::shared_ptr<DvrConfig> config,
                       RecordingMode mode, const std::vector<int>& channels,
                       const std::map<int, int>& channelFpsMap)
    : m_pipeline(nullptr)
    , m_config(config), m_mode(mode), m_outputPath(outputPath)
    , m_channelIds(channels), m_channelFpsMap(channelFpsMap), m_isActive(false)
    , m_anchorPts(GST_CLOCK_TIME_NONE)
    , m_audioAnchorPts(GST_CLOCK_TIME_NONE)
    , m_subtitleAnchorPts(GST_CLOCK_TIME_NONE)
    , m_lastWrittenSubPts(GST_CLOCK_TIME_NONE)
{
    for (int chId : m_channelIds)
        m_channelStatsMap[chId] = ChannelStats(chId, getTargetFps(chId));

    // Audio channel stats — framesProcessed tracks chunk count; timestamp is fallback only.
    if (m_config->audioEnabled == true)
        m_channelStatsMap[AUDIO_CH_ID] = ChannelStats(AUDIO_CH_ID, 0);
}

FileWriter::~FileWriter()
{
    stop();

    for (auto& p : m_customCapsMap)
    {
        if (p.second != nullptr)
        {
            gst_caps_unref(p.second);
        }
    }
    m_customCapsMap.clear();

    if (m_customAudioCaps != nullptr)
    {
        gst_caps_unref(m_customAudioCaps);
        m_customAudioCaps = nullptr;
    }
}

/*--------------------------------------------------------------------------
 * Lifecycle Functions
 *--------------------------------------------------------------------------*/
bool FileWriter::initialize()
{
    std::string pipelineDesc = buildPipelineString();

    LOG_GST_PIPELINE("FileWriter", pipelineDesc.c_str());

    GError* error = nullptr;
    {
        auto t_parse = std::chrono::steady_clock::now();
        m_pipeline = gst_parse_launch(pipelineDesc.c_str(), &error);
        auto parseMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - t_parse).count();
        LOG_DVR_INFOF("[TIMING][%s] gst_parse_launch: %lldms", getModeName(), (long long)parseMs);
    }
    
    if (error != nullptr)
    {
        LOG_DVR_ERRORF("Failed to create pipeline: %s", error->message);
        g_error_free(error);
        return false;
    }
    
    if (m_pipeline == nullptr) { LOG_DVR_ERROR("Pipeline is NULL"); return false; }
    
    // Get video / subtitle appsrc elements
    for (int chId : m_channelIds)
    {
        std::string name = "dvr_source" + std::to_string(chId);
        GstElement* src  = gst_bin_get_by_name(GST_BIN(m_pipeline), name.c_str());
        
        if (src == nullptr)
        { LOG_DVR_ERRORF("appsrc '%s' not found", name.c_str()); return false; }
        
        m_appsrcMap[chId] = src;

        if (isVideoChannel(chId))
        {
            auto capsIt = m_customCapsMap.find(chId);
            if (capsIt != m_customCapsMap.end() && capsIt->second != nullptr)
            {
                g_object_set(G_OBJECT(src), "caps", capsIt->second, nullptr);
            }
        }
    }
    
    // Get audio appsrc element and store in the same map as video.
    if (m_config->audioEnabled == true)
    {
        GstElement* audioSrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "dvr_audio_src");
        if (audioSrc == nullptr)
        {
            LOG_DVR_WARNING("audio appsrc 'dvr_audio_src' not found");
        }
        else
        {
            m_appsrcMap[AUDIO_CH_ID] = audioSrc;
            if (m_customAudioCaps != nullptr)
            {
                g_object_set(G_OBJECT(audioSrc), "caps", m_customAudioCaps, nullptr);
            }
        }
    }
    
    return true;
}

bool FileWriter::start()
{
    if (m_pipeline == nullptr) { LOG_DVR_ERROR("Pipeline not initialized"); return false; }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    { LOG_DVR_ERRORF("Failed to start: %s", m_outputPath.c_str()); return false; }

    if (ret == GST_STATE_CHANGE_ASYNC)
    {
        auto t_playing = std::chrono::steady_clock::now();
        bool parsersReady = true;

        GstIterator* it = gst_bin_iterate_elements(GST_BIN(m_pipeline));
        GValue item = G_VALUE_INIT;
        while (gst_iterator_next(it, &item) == GST_ITERATOR_OK)
        {
            GstElement* elem = GST_ELEMENT(g_value_get_object(&item));
            GstElementFactory* f = gst_element_get_factory(elem);
            const gchar* fname = f ? GST_OBJECT_NAME(f) : nullptr;

            if (fname && (g_strcmp0(fname, "h265parse") == 0 || g_strcmp0(fname, "aacparse") == 0))
            {
                GstStateChangeReturn sr = gst_element_get_state(elem, nullptr, nullptr, 5 * GST_SECOND);
                LOG_DVR_INFOF("[TIMING][%s] %s ready: ret=%d", getModeName(), fname, (int)sr);
                if (sr != GST_STATE_CHANGE_SUCCESS && sr != GST_STATE_CHANGE_NO_PREROLL)
                    parsersReady = false;
            }
            g_value_reset(&item);
        }
        g_value_unset(&item);
        gst_iterator_free(it);

        auto playingMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_playing).count();

        LOG_DVR_INFOF("[TIMING][%s] parser wait: %lldms (ready=%s)", getModeName(), (long long)playingMs, parsersReady ? "yes" : "no");

        if (!parsersReady)
        { LOG_DVR_ERRORF("Parser elements failed to reach PLAYING: %s", m_outputPath.c_str()); return false; }
    }

    for (auto& p : m_channelStatsMap)
        p.second.reset();

    m_anchorPts      = GST_CLOCK_TIME_NONE;
    m_audioAnchorPts = GST_CLOCK_TIME_NONE;
    m_subtitleAnchorPts = GST_CLOCK_TIME_NONE;
    m_lastWrittenSubPts = GST_CLOCK_TIME_NONE;
    m_startTime      = std::chrono::steady_clock::now();
    m_isActive  = true;
    
    // Build channel info string (video channels only)
    std::stringstream info;
    bool first = true;
    for (int chId : m_channelIds)
    {
        if (isVideoChannel(chId) == false) continue;
        if (first == false) info << ", ";
        info << "Ch" << chId << ":" << getTargetFps(chId) << "fps";
        first = false;
    }
    
    bool hasAudio = (m_config->audioEnabled == true && m_appsrcMap.count(AUDIO_CH_ID) != 0);
    
    if (m_mode == RecordingMode::EVENT)
    {
        LOG_DVR_INFOF("%sRecording started [%s]: %s%s", LOG::Color::MAGENTA, getModeName(), m_outputPath.c_str(), LOG::Color::RESET);
        LOG_DVR_INFOF("%s  Duration: %d+%ds | Channels: %s | Audio: %s%s", LOG::Color::MAGENTA,
                      m_config->recordingTimePreEvent,
                      m_config->recordingTimeTotalEvent - m_config->recordingTimePreEvent,
                      info.str().c_str(), hasAudio ? "yes" : "no", LOG::Color::RESET);
    }
    else
    {
        LOG_DVR_INFOF("Recording started [%s]: %s", getModeName(), m_outputPath.c_str());
        LOG_DVR_INFOF("  Duration: %ds | Channels: %s | Audio: %s",
                      getTargetDuration(), info.str().c_str(), hasAudio ? "yes" : "no");
    }
    
    return true;
}

void FileWriter::stop()
{
    if (m_isActive == false && m_pipeline == nullptr) return;

    bool wasActive = m_isActive.exchange(false);

    auto t_stop_start = std::chrono::steady_clock::now();
    auto elapsed_ms   = [&t_stop_start]()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t_stop_start).count();
    };

    if (wasActive)
    {
        if (m_mode == RecordingMode::EVENT)
            LOG_DVR_INFOF("%sStopping [%s]: %s%s", LOG::Color::MAGENTA, getModeName(), m_outputPath.c_str(), LOG::Color::RESET);
        else
            LOG_DVR_INFOF("Stopping [%s]: %s", getModeName(), m_outputPath.c_str());
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        printStatistics();
    }

    GstElement* pipelineToDestroy = nullptr;
    std::map<int, GstElement*> appsrcMapToDestroy;

    {
        std::unique_lock<std::shared_mutex> lock(m_rwMutex);
        pipelineToDestroy = m_pipeline;
        m_pipeline = nullptr;
        appsrcMapToDestroy = std::move(m_appsrcMap);
        m_appsrcMap.clear();
    }

    // ── Send EOS so the muxer finalises the file cleanly ──────────────────
    if (pipelineToDestroy != nullptr)
    {
        GstState current, pending;
        GstStateChangeReturn stateRet = gst_element_get_state(pipelineToDestroy, &current, &pending, 0);
        bool pipelinePlaying = (stateRet != GST_STATE_CHANGE_FAILURE) && (current == GST_STATE_PLAYING);

        if (pipelinePlaying == true && wasActive)
        {
            bool eosSent = false;

            auto audioIt = appsrcMapToDestroy.find(AUDIO_CH_ID);
            if (audioIt != appsrcMapToDestroy.end() && audioIt->second != nullptr)
            {
                GstFlowReturn ret = gst_app_src_end_of_stream(GST_APP_SRC(audioIt->second));
                if (ret == GST_FLOW_OK)
                {
                    eosSent = true;
                    LOG_DVR_INFOF("stop() [%s] audio EOS sent (+%lldms)", getModeName(), (long long)elapsed_ms());
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                LOG_DVR_INFOF("stop() [%s] audio flush wait done (+%lldms)", getModeName(), (long long)elapsed_ms());
            }

            for (auto& p : appsrcMapToDestroy)
            {
                if (p.first == AUDIO_CH_ID) continue;
                if (p.second != nullptr)
                {
                    GstFlowReturn ret = gst_app_src_end_of_stream(GST_APP_SRC(p.second));
                    if (ret == GST_FLOW_OK) eosSent = true;
                }
            }
            LOG_DVR_INFOF("stop() [%s] video EOS sent: %s (+%lldms)",
                          getModeName(), eosSent ? "yes" : "no", (long long)elapsed_ms());

            if (eosSent == true)
            {
                GstBus* bus = gst_element_get_bus(pipelineToDestroy);
                if (bus != nullptr)
                {
                    GstMessage* msg = gst_bus_timed_pop_filtered(bus, 5 * GST_SECOND,
                        static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

                    if (msg != nullptr)
                    {
                        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
                        {
                            GError* err   = nullptr;
                            gchar*  debug = nullptr;
                            gst_message_parse_error(msg, &err, &debug);
                            if (err != nullptr && strstr(err->message, "No valid frames") == nullptr)
                                LOG_DVR_ERRORF("Pipeline error: %s", err->message);
                            g_clear_error(&err);
                            g_free(debug);
                        }
                        LOG_DVR_INFOF("stop() [%s] EOS received: type=%s (+%lldms)",
                                      getModeName(),
                                      GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS ? "EOS" : "ERROR",
                                      (long long)elapsed_ms());
                        gst_message_unref(msg);
                    }
                    else
                    {
                        LOG_DVR_WARNINGF("stop() [%s] EOS timed out (5s) (+%lldms)",
                                         getModeName(), (long long)elapsed_ms());

                        LOG_DVR_WARNINGF("stop() [%s] flushing pipeline to unblock stuck queue (+%lldms)",
                                         getModeName(), (long long)elapsed_ms());
                        gst_element_send_event(pipelineToDestroy, gst_event_new_flush_start());
                        std::this_thread::sleep_for(std::chrono::milliseconds(150));
                    }
                    gst_object_unref(bus);
                }
            }
        }

        // Always set state to NULL first (correct order)
        gst_element_set_state(pipelineToDestroy, GST_STATE_NULL);

        #if (1) // for debug pipeline, state check every 500ms which element was happened blocking
            for (int i = 0; i < 20; i++)  // max 10
            {
                GstStateChangeReturn sr = gst_element_get_state(pipelineToDestroy, nullptr, nullptr, 500 * GST_MSECOND);

                if (sr == GST_STATE_CHANGE_SUCCESS)
                {
                    LOG_DVR_INFOF("stop() NULL complete at %dms (+%lldms)", i * 500, (long long)elapsed_ms());
                    break;
                }

                GstIterator* it = gst_bin_iterate_elements(GST_BIN(pipelineToDestroy));
                GValue item = G_VALUE_INIT;
                while (gst_iterator_next(it, &item) == GST_ITERATOR_OK)
                {
                    GstElement* elem = GST_ELEMENT(g_value_get_object(&item));
                    GstState cur, pending;
                    gst_element_get_state(elem, &cur, &pending, 0);
                    if (cur != GST_STATE_NULL)
                        LOG_DVR_WARNINGF("  [blocking] %s: %s -> %s (+%lldms)", GST_ELEMENT_NAME(elem), gst_element_state_get_name(cur), gst_element_state_get_name(pending), (long long)elapsed_ms());
                    g_value_reset(&item);
                }
                g_value_unset(&item);
                gst_iterator_free(it);
            }
        #endif

        gst_element_get_state(pipelineToDestroy, nullptr, nullptr, 2 * GST_SECOND);
        LOG_DVR_INFOF("stop() [%s] NULL complete (+%lldms)", getModeName(), (long long)elapsed_ms());

        // Drop the extra refs from gst_bin_get_by_name() BEFORE destroying the pipeline.
        for (auto& p : appsrcMapToDestroy)
        {
            if (p.second != nullptr) { gst_object_unref(p.second); p.second = nullptr; }
        }
        appsrcMapToDestroy.clear();

        gst_object_unref(pipelineToDestroy);
    }
 
    if (wasActive)
    {
        if (m_mode == RecordingMode::EVENT)
            LOG_DVR_INFOF("%sRecording stopped [%s] (total: %lldms)%s",
                          LOG::Color::MAGENTA, getModeName(), (long long)elapsed_ms(), LOG::Color::RESET);
        else
            LOG_DVR_INFOF("Recording stopped [%s] (total: %lldms)", getModeName(), (long long)elapsed_ms());
    }
}
 
 


/*--------------------------------------------------------------------------
 * Write Operations
 *--------------------------------------------------------------------------*/
bool FileWriter::writeFrame(int channelId, const uint8_t* data, size_t size, GstClockTime pts)
{
    if (size == 0 || data == nullptr) 
    {
        return false;
    }
    std::string formatted;
    std::shared_lock<std::shared_mutex> lock(m_rwMutex);
    if (m_isActive == false) return false;

    auto statsIt = m_channelStatsMap.find(channelId);
    if (statsIt == m_channelStatsMap.end())
    {
        DVR_DEBUGF("Channel %d not found in stats", channelId);
        return false;
    }
    
    statsIt->second.framesReceived++;

    GstClockTime filePts;

    #ifdef USE_DVR_SUB
        if (isSubtitleChannel(channelId) == true)
        {
            GstClockTime subAnchor = m_subtitleAnchorPts.load();
            if (GST_CLOCK_TIME_IS_VALID(subAnchor) == false)
            {
                GstClockTime none = GST_CLOCK_TIME_NONE;
                if (m_subtitleAnchorPts.compare_exchange_strong(none, pts))
                    subAnchor = pts;
                else
                    subAnchor = m_subtitleAnchorPts.load();

                LOG_DVR_DEBUGF("Subtitle Anchor set: srcPts=%" GST_TIME_FORMAT, GST_TIME_ARGS(subAnchor));
            }

            if ((GST_CLOCK_TIME_IS_VALID(pts) == false) || (pts < subAnchor))
            {
                statsIt->second.framesDropped++;
                return true;
            }

            filePts = pts - subAnchor;

            GstClockTime lastWritten = m_lastWrittenSubPts.load();
            if (GST_CLOCK_TIME_IS_VALID(lastWritten))
            {
                GstClockTime minNextPts = lastWritten + 900 * GST_MSECOND;
                if (filePts < minNextPts)
                {
                    LOG_DVR_DEBUGF("Dropped duplicate/overlapping subtitle frame ch%d: pts=%" GST_TIME_FORMAT 
                                   " (must be >= %" GST_TIME_FORMAT ")",
                                   channelId, GST_TIME_ARGS(filePts), GST_TIME_ARGS(minNextPts));
                    statsIt->second.framesDropped++;
                    return true;
                }
            }
            m_lastWrittenSubPts.store(filePts);
        }
        else
    #endif
    {
        if (isVideoChannel(channelId) == true)
        {
            GstClockTime expected = GST_CLOCK_TIME_NONE;
            if (m_anchorPts.compare_exchange_strong(expected, pts))
                LOG_DVR_DEBUGF("Anchor set: ch%d srcPts=%" GST_TIME_FORMAT, channelId, GST_TIME_ARGS(pts));
        }

        GstClockTime anchor  = m_anchorPts.load();
        filePts = ((GST_CLOCK_TIME_IS_VALID(anchor) == true) && 
                   (GST_CLOCK_TIME_IS_VALID(pts) == true) && 
                   (pts >= anchor))
                  ? (pts - anchor) : statsIt->second.timestamp;
    }

    const uint8_t* actualData = data;
    size_t         actualSize = size;

    #ifdef USE_DVR_SUB
        if (isSubtitleChannel(channelId) == true)
        {
            std::string raw(reinterpret_cast<const char*>(data), size);
            if (raw.empty() == true) { statsIt->second.framesDropped++; return false; }
            formatted = raw;
            actualData = reinterpret_cast<const uint8_t*>(formatted.c_str());
            actualSize = formatted.size();
        }
    #endif

    auto appsrcIt = m_appsrcMap.find(channelId);
    if ((appsrcIt == m_appsrcMap.end()) || (appsrcIt->second == nullptr))
    { statsIt->second.framesDropped++; return false; }
    
    GstElement* appsrc = appsrcIt->second;
    
    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, actualSize, nullptr);
    if (buffer == nullptr) { statsIt->second.framesDropped++; return false; }
    
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_WRITE) == FALSE)
    {
        if(buffer != nullptr)
        {
            gst_buffer_unref(buffer);
        }
        statsIt->second.framesDropped++;
        return false;
    }

    std::memcpy(map.data, actualData, actualSize);
    gst_buffer_unmap(buffer, &map);
    
    GstClockTime frameDuration;
    if (isSubtitleChannel(channelId)) {
        frameDuration = 900 * GST_MSECOND; // 900ms subtitle duration (prevents overlap)
    } else {
        frameDuration = gst_util_uint64_scale_int(1, GST_SECOND, getTargetFps(channelId));
    }

    GST_BUFFER_PTS(buffer)      = filePts;
    GST_BUFFER_DTS(buffer)      = filePts;
    GST_BUFFER_DURATION(buffer) = frameDuration;
    
    if (m_isActive == false)
    {
        if(buffer != nullptr)
        {
            gst_buffer_unref(buffer);
        }
        statsIt->second.framesDropped++;
        return false;
    }

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
    if (ret != GST_FLOW_OK)
    {
        LOG_DVR_ERRORF("Failed to push buffer ch%d, ret=%d", channelId, ret);
        statsIt->second.framesDropped++;
        return false;
    }

    statsIt->second.timestamp += frameDuration;  // kept as fallback for subtitle/invalid-PTS paths
    statsIt->second.framesProcessed++;
    return true;
}

bool FileWriter::writeAudio(const uint8_t* data, size_t size, GstClockTime pts)
{
    if (size == 0 || data == nullptr) 
    {
        return false;
    }
    std::shared_lock<std::shared_mutex> lock(m_rwMutex);
    if (m_isActive == false || m_pipeline == nullptr)
    {
        return false;
    }

    if (m_isActive == false) return false;

    auto appsrcIt = m_appsrcMap.find(AUDIO_CH_ID);
    if ((appsrcIt == m_appsrcMap.end()) || (appsrcIt->second == nullptr)) return false;

    auto statsIt = m_channelStatsMap.find(AUDIO_CH_ID);
    if (statsIt == m_channelStatsMap.end()) return false;

    statsIt->second.framesReceived++;

    // Derive audio PTS from the video anchor when available so A/V sync is preserved.
    // Falls back to an independent audio anchor only before the first video frame is
    // written (normal-recording pre-fill phase, where m_anchorPts is not yet set).
    GstClockTime anchor = m_anchorPts.load();
    bool usingVideoAnchor = GST_CLOCK_TIME_IS_VALID(anchor);
    GstClockTime filePts;

    GstClockTime audioAnchor = m_audioAnchorPts.load();
    if (GST_CLOCK_TIME_IS_VALID(audioAnchor) == false)
    {
        GstClockTime none = GST_CLOCK_TIME_NONE;
        if (m_audioAnchorPts.compare_exchange_strong(none, pts))
            audioAnchor = pts;
        else
            audioAnchor = m_audioAnchorPts.load();

        LOG_DVR_DEBUGF("Audio Anchor set: srcPts=%" GST_TIME_FORMAT, GST_TIME_ARGS(audioAnchor));
    }

    // Drop frames that predate the established audio anchor or have invalid timestamps
    if ((GST_CLOCK_TIME_IS_VALID(pts) == false) || (pts < audioAnchor))
    {
        statsIt->second.framesDropped++;
        return true;
    }

    filePts = pts - audioAnchor;

    GstBuffer* buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
    if (buffer == nullptr) { statsIt->second.framesDropped++; return false; }

    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_WRITE) == FALSE)
    { 
        if(buffer != nullptr)
        {
            gst_buffer_unref(buffer);
        }
        statsIt->second.framesDropped++;  
        return false; 
    }

    std::memcpy(map.data, data, size);
    gst_buffer_unmap(buffer, &map);

    // Each AAC frame covers exactly 1024 PCM samples (fixed by the AAC standard).
    GstClockTime duration = gst_util_uint64_scale(1024, GST_SECOND, static_cast<guint64>(m_config->audioSampleRate));

    if (statsIt->second.framesProcessed < 5)
        LOG_DVR_DEBUGF("writeAudio #%zu: srcPts=%" GST_TIME_FORMAT " anchor(%s)=%" GST_TIME_FORMAT
                       " filePts=%" GST_TIME_FORMAT,
                      statsIt->second.framesProcessed.load(),
                      GST_TIME_ARGS(pts),
                      usingVideoAnchor ? "video" : "audio",
                      GST_TIME_ARGS(usingVideoAnchor ? anchor : m_audioAnchorPts.load()),
                      GST_TIME_ARGS(filePts));

    GST_BUFFER_PTS(buffer)      = filePts;
    GST_BUFFER_DTS(buffer)      = filePts;
    GST_BUFFER_DURATION(buffer) = duration;

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrcIt->second), buffer);
    if (ret != GST_FLOW_OK)
    { LOG_DVR_ERRORF("Failed to push AAC buffer, ret=%d", ret); statsIt->second.framesDropped++; return false; }

    statsIt->second.framesProcessed++;
    return true;
}

/*--------------------------------------------------------------------------
 * State Query Functions
 *--------------------------------------------------------------------------*/
bool FileWriter::shouldRotate() const
{
    // Primary: wall-clock elapsed time guarantees exact file duration regardless of frame drops.
    auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    int targetSec = (m_mode == RecordingMode::EVENT)
                    ? (m_config->recordingTimeTotalEvent - m_config->recordingTimePreEvent)
                    : m_config->recordingTime;

    if (elapsed >= std::chrono::seconds(targetSec)) return true;

    // Fallback: all video channels reached their target frame count (normal, no-drop case).
    for (int chId : m_channelIds)
    {
        if (isVideoChannel(chId) == false) continue;
        if (shouldRotateChannel(chId) == false) return false;
    }

    return true;
}

bool FileWriter::shouldRotateChannel(int channelId) const
{
    // Non-video channels (subtitle, audio) never drive rotation
    if (isVideoChannel(channelId) == false) return false;

    // Time-based primary check: consistent with shouldRotate()
    auto elapsed = std::chrono::steady_clock::now() - m_startTime;
    int targetSec = (m_mode == RecordingMode::EVENT)
                    ? (m_config->recordingTimeTotalEvent - m_config->recordingTimePreEvent)
                    : m_config->recordingTime;

    if (elapsed >= std::chrono::seconds(targetSec)) return true;

    auto it = m_channelStatsMap.find(channelId);
    if (it == m_channelStatsMap.end()) return false;

    return (static_cast<int>(it->second.framesProcessed) >= getTargetFrameCount(channelId));
}

/*--------------------------------------------------------------------------
 * Statistics
 *--------------------------------------------------------------------------*/
void FileWriter::printStatistics()
{
    auto   endTime = std::chrono::steady_clock::now();
    double durSec  = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - m_startTime).count() / 1000.0;

    LOG_DVR_SECTION("Recording Statistics");
    
    int expFps    = getTargetFps(m_channelIds[0]);
    int expFrames = getTargetFrameCount(m_channelIds[0]);
    int recW, recH;
    getTargetResolution(m_channelIds[0], recW, recH);

    // First row: Mode and Path
    LOG_DVR_INFOF("Mode: %s | Path: %s%s%s", getModeName(), LOG::Color::BG_DARK_YELLOW, m_outputPath.c_str(), LOG::Color::RESET);
    
    // Second row: Resolution, Expected frames, Actual duration
    if (m_mode == RecordingMode::EVENT)
    {
        int pre   = m_config->recordingTimePreEvent;
        int total = m_config->recordingTimeTotalEvent;
        LOG_DVR_INFOF("Resolution: %dx%d | Expected: %ds+%ds @ %dfps = %d+%d = %d frames/channel | Actual Duration: %.1fs",
                      recW, recH, pre, total - pre, expFps,
                      pre * expFps, (total - pre) * expFps, total * expFps, durSec);
    }
    else
    {
        LOG_DVR_INFOF("Resolution: %dx%d | Expected: %ds @ %dfps = %d frames/channel | Actual Duration: %.1fs",
                      recW, recH, getTargetDuration(), expFps, expFrames, durSec);
    }

    auto table = LOG::Logger::createTable();
    table.setTitle("Channel Statistics");
    table.setHeaders({"Channel", "FPS", "Received", "Processed", "Dropped", "Success"},
                     {12, 12, 12, 12, 12, 12});

    size_t totalRecv = 0, totalProc = 0, totalDrop = 0;
    
    for (int chId : m_channelIds)
    {
        auto it = m_channelStatsMap.find(chId);
        if (it == m_channelStatsMap.end()) continue;

        const auto& s = it->second;
        totalRecv += s.framesReceived;
        totalProc += s.framesProcessed;
        totalDrop += s.framesDropped;
        
        double rate = (s.framesReceived > 0) ? (s.framesProcessed.load() * 100.0 / s.framesReceived.load()) : 0.0;
        std::string name = isVideoChannel(chId) ? ("Ch" + std::to_string(s.channelId)) : "SUB";
        
        char fps[16], recv[16], proc[16], drop[16], succ[16];
        std::snprintf(fps,  sizeof(fps),  "%d",     s.targetFps);
        std::snprintf(recv, sizeof(recv), "%zu",    s.framesReceived.load());
        std::snprintf(proc, sizeof(proc), "%zu",    s.framesProcessed.load());
        std::snprintf(drop, sizeof(drop), "%zu",    s.framesDropped.load());
        std::snprintf(succ, sizeof(succ), "%.1f%%", rate);
        
        table.addRow({name, fps, recv, proc, drop, succ});
    }

    if ((m_config->audioEnabled == true) && (m_appsrcMap.count(AUDIO_CH_ID) != 0))
    {
        auto it = m_channelStatsMap.find(AUDIO_CH_ID);
        if (it != m_channelStatsMap.end())
        {
            const auto& s = it->second;
            double rate = (s.framesReceived > 0) ? (s.framesProcessed.load() * 100.0 / s.framesReceived.load()) : 0.0;
            char recv[16], proc[16], drop[16], succ[16];
            std::snprintf(recv, sizeof(recv), "%zu", s.framesReceived.load());
            std::snprintf(proc, sizeof(proc), "%zu", s.framesProcessed.load());
            std::snprintf(drop, sizeof(drop), "%zu", s.framesDropped.load());
            std::snprintf(succ, sizeof(succ), "%.1f%%", rate);
            table.addRow({"AUDIO", "-", recv, proc, drop, succ});
        }
    }

    double totalRate = (totalRecv > 0) ? (totalProc * 100.0 / totalRecv) : 0.0;
    
    char tRecv[16], tProc[16], tDrop[16], tSucc[16];
    std::snprintf(tRecv, sizeof(tRecv), "%zu",    totalRecv);
    std::snprintf(tProc, sizeof(tProc), "%zu",    totalProc);
    std::snprintf(tDrop, sizeof(tDrop), "%zu",    totalDrop);
    std::snprintf(tSucc, sizeof(tSucc), "%.1f%%", totalRate);
    
    table.addRow({"TOTAL", "-", tRecv, tProc, tDrop, tSucc});
    table.render();
}

/*--------------------------------------------------------------------------
 * Helper Functions
 *--------------------------------------------------------------------------*/
std::string FileWriter::buildPipelineString()
{
    std::stringstream ss;
    std::string muxName = "mp4mux";
    ss << muxName << " name=mux fragment-duration=1000 ! filesink name=fsink location=" << m_outputPath << " \n";
    int videoPadIdx = 0;

    for (size_t i = 0; i < m_channelIds.size(); i++)
    {
        int chId = m_channelIds[i];
        if (isSubtitleChannel(chId) == true)
        {
            ss << "appsrc name=dvr_source" << chId
               << " is-live=true do-timestamp=false format=time caps=\"text/x-raw,format=utf8\" ! queue max-size-buffers=120 max-size-time=0 max-size-bytes=0 ! mux.subtitle_0 \n";
        }
        else
        {
            ss << "appsrc name=dvr_source" << chId
               << " is-live=true do-timestamp=false format=time"
               << " ! queue max-size-buffers=120 max-size-time=0 max-size-bytes=0 ! mux.video_" << videoPadIdx << " \n";
            videoPadIdx++;
        }
    }

    if (m_config->audioEnabled == true)
    {
        ss << "appsrc name=dvr_audio_src is-live=true do-timestamp=false format=time"
           << " caps=audio/mpeg,mpegversion=4,stream-format=adts"
           << " ! queue max-size-buffers=200 max-size-time=0 max-size-bytes=0 flush-on-eos=true"
           << " ! mux.audio_0 \n";
    }
    
    return ss.str();
}

int FileWriter::getTargetFps(int channelId) const
{
    auto it = m_channelFpsMap.find(channelId);
    if (it != m_channelFpsMap.end()) return it->second;
    LOG_DVR_WARNINGF("Failed to get channel %d FPS", channelId);
    return 0;                                                                           
}

int FileWriter::getTargetDuration() const
{
    return (m_mode == RecordingMode::EVENT) ? m_config->recordingTimeTotalEvent : m_config->recordingTime;
}

int FileWriter::getTargetFrameCount(int channelId) const
{
    return getTargetFps(channelId) * getTargetDuration();
}

void FileWriter::getTargetResolution(int channelId, int& width, int& height) const
{
    bool isInternal = (channelId >= SVM_CAMERAS_NUM);
    if (isInternal == true)
    {
        width  = m_config->recordingWidthInternal;
        height = m_config->recordingHeightInternal;
    }
    else
    {
        width  = m_config->recordingWidth;
        height = m_config->recordingHeight;
    }
}

const char* FileWriter::getModeName() const
{
    switch (m_mode)
    {
        case RecordingMode::EVENT:  return "EVENT";
        case RecordingMode::NORMAL:
        default:                    return (m_config->isParkingMode == true) ? "PARKING" : "NORMAL";
    }
}

#ifdef USE_DVR_SUB
    std::string FileWriter::formatSubtitle(const std::string& text, GstClockTime filePts, size_t index)
    {
        uint64_t startMs = filePts / GST_MSECOND;
        uint64_t endMs   = startMs + 1000;

        auto msToSrt = [](uint64_t ms) -> std::string
        {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d,%03d",
                          static_cast<int>(ms / 3600000),
                          static_cast<int>((ms % 3600000) / 60000),
                          static_cast<int>((ms % 60000) / 1000),
                          static_cast<int>(ms % 1000));
            return std::string(buf);
        };
        
        std::ostringstream oss;
        oss << (index + 1) << "\n"
            << msToSrt(startMs) << " --> " << msToSrt(endMs) << "\n"
            << text << "\n\n";
        
        return oss.str();
    }
#endif

void FileWriter::setVideoCaps(int channelId, GstCaps* caps)
{
    if (caps == nullptr) return;
    auto it = m_customCapsMap.find(channelId);
    if (it != m_customCapsMap.end())
    {
        if (it->second == caps) return; // Prevent self-assignment double free
        if (it->second != nullptr)
        {
            gst_caps_unref(it->second);
        }
    }
    m_customCapsMap[channelId] = gst_caps_copy(caps);
}

void FileWriter::setAudioCaps(GstCaps* caps)
{
    if (caps == nullptr) return;
    if (m_customAudioCaps == caps) return; // Prevent self-assignment double free
    if (m_customAudioCaps != nullptr)
    {
        gst_caps_unref(m_customAudioCaps);
    }
    m_customAudioCaps = gst_caps_copy(caps);
}