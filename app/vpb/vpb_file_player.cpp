#include "io_platform.h"
#include "vpb_file_player.h"
#include <gst/video/video.h>
#include <gst/gl/gl.h>
#include <gst/pbutils/pbutils.h>
#include <GLES2/gl2.h>
#include <iostream>
#include <sstream>
#include <cstring>
#include "app_vars.h"

namespace VPB
{

// ============================================================================
// Lifecycle
// ============================================================================

FilePlayer::FilePlayer() {}

FilePlayer::~FilePlayer() { shutdown(); }

bool FilePlayer::initialize()
{
    if (m_initialized == true) return true;

    if (gst_is_initialized() == FALSE)
    {
        std::cerr << "FilePlayer: GStreamer must be initialized first" << std::endl;
        return false;
    }

    m_initialized = true;
    return true;
}

void FilePlayer::shutdown()
{
    if (m_initialized == false) return;
    unloadFile();
    m_initialized = false;
}

// ============================================================================
// File
// ============================================================================

bool FilePlayer::loadFile(const std::string& path)
{
    if (m_fileLoaded == true) unloadFile();

    if (FileUtils::fileExists(path) == false)
    {
        std::cerr << "FilePlayer: File not found: " << path << std::endl;
        return false;
    }

    m_currentFilePath     = path;
    m_config.selectedView = 0;

    if (readFileMetadata() == false) return false;

    if (createPipeline() == false)
    {
        m_currentFilePath.clear();
        m_metadata = FileMetadata();
        return false;
    }

    m_fileLoaded = true;
    changeState(PlaybackState::STOPPED);

    const char* modeStr = (m_config.pipelineMode == PipelineMode::GPU) ? "GPU" : "CPU";
    std::cout << "FilePlayer: loaded " << path << " [" << modeStr << "] v=" << m_metadata.videoTrackCount
              << " a=" << m_metadata.audioTrackCount << " s=" << m_metadata.subtitleTrackCount
              << " dur=" << (m_metadata.duration / 1000.0f) << "s" << std::endl;
    return true;
}

void FilePlayer::unloadFile()
{
    if (m_fileLoaded == false) return;

    stop();
    destroyPipeline();

    {
        std::lock_guard<std::mutex> lk(m_subtitleMutex);
        m_subtitle    = SubtitleData();
        m_hasSubtitle = false;
    }

    m_currentFilePath = {};
    m_metadata        = {};
    m_fileLoaded      = false;
    m_pendingView     = -1;
    m_switchTimer     = 0.0f;
}

// ============================================================================
// Playback
// ============================================================================

void FilePlayer::play()
{
    if ((m_fileLoaded == false) || (m_state == PlaybackState::PLAYING) || (m_pipeline == nullptr)) return;

    GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        std::cerr << "FilePlayer: Failed to set PLAYING" << std::endl;
        changeState(PlaybackState::ERROR);
        return;
    }

    if (ret == GST_STATE_CHANGE_ASYNC) gst_element_get_state(m_pipeline, nullptr, nullptr, 200 * GST_MSECOND);

    // Re-apply mixer alpha settings after PLAYING transition because glvideomixer
    // resets pad properties when the GL context becomes active.
    if (m_config.pipelineMode == PipelineMode::GPU)
        applyViewToMixer();

    changeState(PlaybackState::PLAYING);
}

void FilePlayer::pause()
{
    if ((m_fileLoaded == false) || (m_state != PlaybackState::PLAYING) || (m_pipeline == nullptr)) return;
    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    gst_element_get_state(m_pipeline, nullptr, nullptr, 200 * GST_MSECOND);
    changeState(PlaybackState::PAUSED);
}

void FilePlayer::stop()
{
    if (m_fileLoaded == false) return;

    if (m_pipeline != nullptr)
    {
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        gst_element_get_state(m_pipeline, nullptr, nullptr, 200 * GST_MSECOND);
        gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), 0);
    }

    {
        std::lock_guard<std::mutex> lk(m_subtitleMutex);
        m_subtitle    = SubtitleData();
        m_hasSubtitle = false;
    }

    changeState(PlaybackState::STOPPED);

    if (m_videoTexture.id != 0)
    {
        glFinish();
        glDeleteTextures(1, &m_videoTexture.id);
        m_videoTexture = {};
    }
}

void FilePlayer::seek(int64_t positionMs)
{
    if ((m_fileLoaded == false) || (m_pipeline == nullptr)) return;
    gst_element_seek(m_pipeline, static_cast<gdouble>(m_config.playbackSpeed),
        GST_FORMAT_TIME,
        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET,  positionMs * GST_MSECOND,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
}

// ============================================================================
// Setters
// ============================================================================

void FilePlayer::setConfig(const PlayerConfig& config)
{
    bool viewOrModeChanged = (config.selectedView != m_config.selectedView) ||
                             (config.pipelineMode != m_config.pipelineMode);
    bool speedChanged      = (config.playbackSpeed != m_config.playbackSpeed);

    m_config = config;

    if ((speedChanged == true) && (m_fileLoaded == true) && (m_pipeline != nullptr))
        seek(getCurrentPosition());

    if ((viewOrModeChanged == true) && (m_fileLoaded == true))
        beginViewSwitch(m_config.selectedView);
}

void FilePlayer::setSelectedView(int32_t view)
{
    if (m_fileLoaded == false) return;

    if (m_pendingView == -1)
        beginViewSwitch(view);
    else if (view != m_pendingView)
    {
        m_pendingView = view;
        m_switchTimer = 0.0f;
    }
}

void FilePlayer::setPlaybackSpeed(float speed)
{
    if ((speed <= 0.0f) || (speed == m_config.playbackSpeed)) return;
    m_config.playbackSpeed = speed;
    if ((m_fileLoaded == true) && (m_pipeline != nullptr))
        seek(getCurrentPosition());
}

void FilePlayer::setPipelineMode(PipelineMode mode)
{
    if ((mode == m_config.pipelineMode) || (m_fileLoaded == false)) return;
    m_config.pipelineMode = mode;
    beginViewSwitch(m_config.selectedView);
}

void FilePlayer::setVolume(float volume)
{
    m_config.volume = std::max(0.0f, std::min(1.0f, volume));
    if (m_pipeline == nullptr) return;
    GstElement* vol = gst_bin_get_by_name(GST_BIN(m_pipeline), "volumectl");
    if (vol != nullptr)
    {
        g_object_set(G_OBJECT(vol), "volume", static_cast<gdouble>(m_config.volume), nullptr);
        gst_object_unref(vol);
    }
}

// ============================================================================
// Getters
// ============================================================================

int64_t FilePlayer::getCurrentPosition() const
{
    if ((m_fileLoaded == false) || (m_pipeline == nullptr) || (m_state == PlaybackState::STOPPED)) return 0;
    gint64 pos = 0;
    if (gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos) == TRUE)
        return pos / GST_MSECOND;
    return 0;
}

const SubtitleData* FilePlayer::getCurrentSubtitle() const
{
    std::lock_guard<std::mutex> lk(m_subtitleMutex);
    return (m_hasSubtitle == true) ? &m_subtitle : nullptr;
}

APP::UI::Texture* FilePlayer::getVideoTexture()
{
    return m_videoTexture.isValid() ? &m_videoTexture : nullptr;
}

// ============================================================================
// Update (main thread only)
// ============================================================================

void FilePlayer::update(float deltaTime)
{
    if (m_fileLoaded == false) return;

    if (m_pendingView != -1)
    {
        m_switchTimer += deltaTime;
        if (m_switchTimer >= SWITCH_COMMIT_SEC)
            commitViewSwitch();
        return;
    }

    if (m_state == PlaybackState::PLAYING)
    {
        if (m_config.pipelineMode == PipelineMode::CPU) pullAndUploadCPUFrame();
        else                                            pullAndUploadGPUFrame();
    }

    if (m_bus != nullptr)
    {
        GstMessage* msg = nullptr;
        while ((msg = gst_bus_pop(m_bus)) != nullptr)
        {
            switch (GST_MESSAGE_TYPE(msg))
            {
                case GST_MESSAGE_ERROR:
                {
                    GError* err = nullptr; gchar* dbg = nullptr;
                    gst_message_parse_error(msg, &err, &dbg);
                    std::cerr << "FilePlayer: pipeline error: " << err->message;
                    if (dbg != nullptr) std::cerr << " | " << dbg;
                    std::cerr << std::endl;
                    g_error_free(err); g_free(dbg);
                    changeState(PlaybackState::ERROR);
                    break;
                }
                case GST_MESSAGE_EOS:
                    stop();
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
    }

    checkEndOfStream();
}

// ============================================================================
// Pipeline helpers
// ============================================================================

static void padAddedProbeCallback(GstElement* src, GstPad* newPad, gpointer userData)
{
    GstCaps* caps = gst_pad_get_current_caps(newPad);
    if (caps == nullptr)
    {
        g_printerr("Error: Could not get pad capabilities.\n");
        return;
    }
    
    GstStructure* structure = gst_caps_get_structure(caps, 0);
    if (structure == nullptr)
    {
        g_printerr("Error: Could not get structure from capabilities.\n");
        gst_caps_unref(caps);
        return;
    }
    
    const gchar* mimeType = gst_structure_get_name(structure);
    VPB::FileMetadata* metadata = static_cast<VPB::FileMetadata*>(userData);
    
    if (g_str_has_prefix(mimeType, "audio/") == TRUE)
    {
        metadata->audioTrackCount++;
    }
    else if (g_str_has_prefix(mimeType, "video/") == TRUE)
    {
        if (metadata->videoTrackCount == 0)
        {
            gint width = 0, height = 0;
            if (gst_structure_get_int(structure, "width", &width) == TRUE &&
                gst_structure_get_int(structure, "height", &height) == TRUE)
            {
                metadata->width = width;
                metadata->height = height;
            }
            
            const GValue* framerateValue = gst_structure_get_value(structure, "framerate");
            if (framerateValue != nullptr && GST_VALUE_HOLDS_FRACTION(framerateValue) == TRUE)
            {
                gint fpsNum = gst_value_get_fraction_numerator(framerateValue);
                gint fpsDenom = gst_value_get_fraction_denominator(framerateValue);
                if (fpsDenom > 0)
                {
                    metadata->framerate = (float)fpsNum / (float)fpsDenom;
                }
            }
        }
        metadata->videoTrackCount++;
    }
    else if (g_str_has_prefix(mimeType, "text/") == TRUE || 
             g_str_has_prefix(mimeType, "subtitle/") == TRUE ||
             g_str_has_prefix(mimeType, "application/x-subtitle") == TRUE)
    {
        metadata->subtitleTrackCount++;
    }
    
    gst_caps_unref(caps);
}

bool FilePlayer::readFileMetadata()
{
    gchar* pipelineStr = g_strdup_printf("filesrc location=%s ! qtdemux ! fakesink", 
                                          m_currentFilePath.c_str());
    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineStr, &error);
    g_free(pipelineStr);
    
    if (error != nullptr)
    {
        std::cerr << "    Failed to create duration pipeline: " << error->message << std::endl;
        g_error_free(error);
    }
    else if (pipeline != nullptr)
    {
        if (gst_element_set_state(pipeline, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE)
        {
            gst_element_get_state(pipeline, nullptr, nullptr, 10 * GST_SECOND);
            
            GstFormat fmt = GST_FORMAT_TIME;
            gint64 duration = 0;
            if (gst_element_query_duration(pipeline, fmt, &duration) == TRUE)
            {
                m_metadata.duration = duration / GST_MSECOND;
            }
        }
        
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
    
    pipeline = gst_pipeline_new("track-info-pipeline");
    GstElement* uriDecodeBin = gst_element_factory_make("uridecodebin", "uri-decoder");
    
    if (pipeline != nullptr && uriDecodeBin != nullptr)
    {
        gchar* uriPath = g_filename_to_uri(m_currentFilePath.c_str(), nullptr, nullptr);
        if (uriPath != nullptr)
        {
            g_object_set(G_OBJECT(uriDecodeBin), "uri", uriPath, nullptr);
            g_free(uriPath);
            
            gst_bin_add(GST_BIN(pipeline), uriDecodeBin);
            
            m_metadata.videoTrackCount = 0;
            m_metadata.audioTrackCount = 0;
            m_metadata.subtitleTrackCount = 0;
            
            g_signal_connect(uriDecodeBin, "pad-added", G_CALLBACK(padAddedProbeCallback), &m_metadata);
            
            if (gst_element_set_state(pipeline, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE)
            {
                gst_element_get_state(pipeline, nullptr, nullptr, 10 * GST_SECOND);
            }
            
            gst_element_set_state(pipeline, GST_STATE_NULL);
        }
        
        gst_object_unref(pipeline);
    }
    
    m_metadata.isValid = (m_metadata.videoTrackCount > 0);
    
    return m_metadata.isValid;
}

// bool FilePlayer::readFileMetadata()
// {
//     GError*        err        = nullptr;
//     GstDiscoverer* discoverer = gst_discoverer_new(10 * GST_SECOND, &err);
//     if (discoverer == nullptr)
//     {
//         std::cerr << "FilePlayer: GstDiscoverer create failed: "
//                   << (err ? err->message : "unknown") << std::endl;
//         g_clear_error(&err);
//         return false;
//     }

//     gchar* uri = g_filename_to_uri(m_currentFilePath.c_str(), nullptr, &err);
//     if (uri == nullptr)
//     {
//         std::cerr << "FilePlayer: URI build failed: "
//                   << (err ? err->message : "unknown") << std::endl;
//         g_clear_error(&err);
//         g_object_unref(discoverer);
//         return false;
//     }

//     GstDiscovererInfo* info = gst_discoverer_discover_uri(discoverer, uri, &err);
//     g_free(uri);
//     g_object_unref(discoverer);

//     if (info == nullptr)
//     {
//         std::cerr << "FilePlayer: Discovery failed: "
//                   << (err ? err->message : "unknown") << std::endl;
//         g_clear_error(&err);
//         return false;
//     }
//     g_clear_error(&err);

//     m_metadata          = FileMetadata();
//     m_metadata.duration = static_cast<int64_t>(gst_discoverer_info_get_duration(info)) / GST_MSECOND;

//     GList* streams = gst_discoverer_info_get_stream_list(info);
//     for (GList* l = streams; l != nullptr; l = l->next)
//     {
//         GstDiscovererStreamInfo* s = GST_DISCOVERER_STREAM_INFO(l->data);
//         if (GST_IS_DISCOVERER_VIDEO_INFO(s) == TRUE)
//         {
//             if (m_metadata.videoTrackCount == 0)
//             {
//                 GstDiscovererVideoInfo* v = GST_DISCOVERER_VIDEO_INFO(s);
//                 m_metadata.width  = static_cast<int32_t>(gst_discoverer_video_info_get_width(v));
//                 m_metadata.height = static_cast<int32_t>(gst_discoverer_video_info_get_height(v));
//                 guint n = gst_discoverer_video_info_get_framerate_num(v);
//                 guint d = gst_discoverer_video_info_get_framerate_denom(v);
//                 m_metadata.framerate = (d > 0) ? (static_cast<float>(n) / static_cast<float>(d)) : 0.0f;
//             }
//             m_metadata.videoTrackCount++;
//         }
//         else if (GST_IS_DISCOVERER_AUDIO_INFO(s) == TRUE)    m_metadata.audioTrackCount++;
//         else if (GST_IS_DISCOVERER_SUBTITLE_INFO(s) == TRUE) m_metadata.subtitleTrackCount++;
//     }
//     gst_discoverer_stream_info_list_free(streams);
//     gst_discoverer_info_unref(info);

//     m_metadata.isValid = (m_metadata.videoTrackCount > 0);
//     return m_metadata.isValid;
// }

std::string FilePlayer::buildPipelineString() const
{
    const char* queueLimit = "";
    const char* decodeElement = " ! decodebin";

    #ifdef EMBEDDED_DEVICE
        // queueLimit = " max-size-buffers=1";
        decodeElement = " ! h265parse ! mppvideodec";
    #endif

    std::ostringstream oss;
    oss << "filesrc location=" << m_currentFilePath << " ! qtdemux name=demux ";

    if (m_config.pipelineMode == PipelineMode::GPU)
    {
        int32_t numCh = std::min(m_metadata.videoTrackCount, 4);
        for (int32_t i = 0; i < numCh; ++i)
        {
            oss << "demux.video_" << i
                << " ! queue" << queueLimit << decodeElement
                << " ! glupload ! glcolorconvert ! video/x-raw(memory:GLMemory),format=RGBA"
                << " ! mix.sink_" << i << " ";
        }
        oss << "glvideomixer name=mix"
            << " ! video/x-raw(memory:GLMemory),width=1920,height=1080,format=RGBA"
            << " ! appsink name=videosink";
    }
    else
    {
        std::vector<int32_t> channels;
        bool                 isGrid = false;
        getViewChannels(m_config.selectedView, channels, isGrid);

        if ((isGrid == true) && (channels.size() > 1))
        {
            for (size_t i = 0; i < channels.size(); ++i)
            {
                oss << "demux.video_" << channels[i]
                    << " ! queue" << queueLimit << decodeElement
                    << " ! videoconvert ! videoscale ! video/x-raw,width=960,height=540,format=RGBA"
                    << " ! mix.sink_" << i << " ";
            }
            const int pos[4][4] = { {0,0,960,540}, {960,0,960,540}, {0,540,960,540}, {960,540,960,540} };
            oss << "compositor name=mix ";
            for (size_t i = 0; i < channels.size(); ++i)
                oss << "sink_" << i << "::xpos=" << pos[i][0] << " sink_" << i << "::ypos=" << pos[i][1] << " "
                    << "sink_" << i << "::width=" << pos[i][2] << " sink_" << i << "::height=" << pos[i][3] << " ";
            oss << "! video/x-raw,width=1920,height=1080,format=RGBA ! appsink name=videosink";
        }
        else
        {
            int ch = channels.empty() ? 0 : channels[0];
            oss << "demux.video_" << ch
                << " ! queue" << queueLimit << decodeElement
                << " ! videoconvert ! videoscale ! video/x-raw,format=RGBA ! appsink name=videosink";
        }
    }

    if (m_metadata.audioTrackCount > 0)
        oss << " demux.audio_0 ! queue ! decodebin ! audioconvert ! audioresample"
            << " ! volume name=volumectl volume=" << m_config.volume
            << " ! alsasink device=plughw:2,0";

    if (m_metadata.subtitleTrackCount > 0)
        oss << " demux.subtitle_0 ! queue ! appsink name=subsink";

    return oss.str();
}

void FilePlayer::getViewChannels(int32_t view, std::vector<int32_t>& channels, bool& isGrid) const
{
    channels.clear();
    isGrid = false;

    if ((view >= 0) && (view <= 7))
    {
        channels.push_back((view < m_metadata.videoTrackCount) ? view : 0);
    }
    else if (view == 8)
    {
        for (int i = 0; (i < 4) && (i < m_metadata.videoTrackCount); ++i) channels.push_back(i);
        isGrid = (channels.size() > 1);
    }
    else if (view == 9)
    {
        for (int i = 4; (i < 8) && (i < m_metadata.videoTrackCount); ++i) channels.push_back(i);
        isGrid = (channels.size() > 1);
    }
    else
    {
        channels.push_back(0);
    }
}

bool FilePlayer::createPipeline()
{
    GError* err = nullptr;
    m_pipeline  = gst_parse_launch(buildPipelineString().c_str(), &err);
    if (err != nullptr)
    {
        std::cerr << "FilePlayer: Pipeline parse error: " << err->message << std::endl;
        g_error_free(err);
        return false;
    }
    if (m_pipeline == nullptr) return false;

    if (m_config.pipelineMode == PipelineMode::GPU)
    {
        GstContext* ctx = APP::IO::Platform::getInstance().getGstContext();
        if (ctx != nullptr) gst_element_set_context(m_pipeline, ctx);
    }

    configureSinks();
    m_bus = gst_element_get_bus(m_pipeline);

    if (m_config.pipelineMode == PipelineMode::GPU)
        applyViewToMixer();

    return true;
}

void FilePlayer::configureSinks()
{
    m_videoSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "videosink");
    if (m_videoSink == nullptr) { std::cerr << "FilePlayer: videosink not found" << std::endl; return; }

    GstCaps* caps = nullptr;
    if (m_config.pipelineMode == PipelineMode::CPU)
    {
        caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA", nullptr);
    }
    else
    {
        caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGBA",
                                   "texture-target", G_TYPE_STRING, "2D", nullptr);
        gst_caps_set_features(caps, 0, gst_caps_features_new("memory:GLMemory", nullptr));
    }

    g_object_set(G_OBJECT(m_videoSink),
                 "emit-signals", FALSE, "sync", TRUE,
                 "max-buffers",  1,     "drop", TRUE,
                 "caps",         caps,  nullptr);
    gst_caps_unref(caps);

    if (m_metadata.subtitleTrackCount > 0)
    {
        m_subtitleSink = gst_bin_get_by_name(GST_BIN(m_pipeline), "subsink");
        if (m_subtitleSink != nullptr)
        {
            g_object_set(G_OBJECT(m_subtitleSink),
                         "emit-signals", FALSE, "sync", TRUE,
                         "max-buffers",  1,     "drop", TRUE, nullptr);

            static GstAppSinkCallbacks subtitleCbs = { nullptr, nullptr, onNewSubtitleSample };
            gst_app_sink_set_callbacks(GST_APP_SINK(m_subtitleSink), &subtitleCbs, this, nullptr);
        }
    }
}

void FilePlayer::destroyPipeline()
{
    if (m_pipeline == nullptr)
    {
        if (m_bus != nullptr) { gst_object_unref(m_bus); m_bus = nullptr; }
        return;
    }

    if (m_subtitleSink != nullptr)
    {
        static GstAppSinkCallbacks emptyCbs = { nullptr, nullptr, nullptr };
        gst_app_sink_set_callbacks(GST_APP_SINK(m_subtitleSink), &emptyCbs, nullptr, nullptr);
    }

    if (m_bus != nullptr)
    {
        gst_bus_set_flushing(m_bus, TRUE);
        GstMessage* msg = nullptr;
        while ((msg = gst_bus_pop(m_bus)) != nullptr) gst_message_unref(msg);
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    gst_element_get_state(m_pipeline, nullptr, nullptr, 2 * GST_SECOND);

    if (m_videoSink    != nullptr) { gst_object_unref(m_videoSink);    m_videoSink    = nullptr; }
    if (m_subtitleSink != nullptr) { gst_object_unref(m_subtitleSink); m_subtitleSink = nullptr; }
    gst_object_unref(m_pipeline);
    m_pipeline = nullptr;

    if (m_videoTexture.id != 0)
    {
        glFinish();
        glDeleteTextures(1, &m_videoTexture.id);
        m_videoTexture = {};
    }
}

bool FilePlayer::rebuildPipeline(int64_t savedPos)
{
    if (m_fileLoaded == false) return false;

    destroyPipeline();

    if (createPipeline() == false)
    {
        std::cerr << "FilePlayer: Rebuild failed" << std::endl;
        changeState(PlaybackState::ERROR);
        return false;
    }

    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    gst_element_get_state(m_pipeline, nullptr, nullptr, 500 * GST_MSECOND);
    if (savedPos > 0) seek(savedPos);

    changeState(PlaybackState::PAUSED);
    return true;
}

void FilePlayer::applyViewToMixer()
{
    if ((m_pipeline == nullptr) || (m_config.pipelineMode != PipelineMode::GPU)) return;

    GstElement* mix = gst_bin_get_by_name(GST_BIN(m_pipeline), "mix");
    if (mix == nullptr) return;

    std::vector<int32_t> channels;
    bool                 isGrid = false;
    getViewChannels(m_config.selectedView, channels, isGrid);

    int32_t totalCh = std::min(m_metadata.videoTrackCount, 4);

    for (int32_t i = 0; i < totalCh; ++i)
    {
        std::string padName = "sink_" + std::to_string(i);
        GstPad* pad = gst_element_get_static_pad(mix, padName.c_str());
        if (pad == nullptr) continue;

        bool active = false;
        for (int32_t ch : channels)
        {
            if (ch == i) { active = true; break; }
        }

        if (isGrid == true)
        {
            const int pos[4][4] = { {0,0,960,540}, {960,0,960,540}, {0,540,960,540}, {960,540,960,540} };
            g_object_set(G_OBJECT(pad),
                         "xpos",   pos[i][0], "ypos",   pos[i][1],
                         "width",  pos[i][2], "height", pos[i][3],
                         "alpha",  active ? 1.0 : 0.0,
                         nullptr);
        }
        else
        {
            g_object_set(G_OBJECT(pad),
                         "xpos", 0, "ypos", 0, "width", 1920, "height", 1080,
                         "alpha", active ? 1.0 : 0.0,
                         nullptr);
        }

        gst_object_unref(pad);
    }

    gst_object_unref(mix);
}

void FilePlayer::flushPipelineToPosition(int64_t positionMs)
{
    if (m_pipeline == nullptr) return;

    gint64 target = (positionMs > 0) ? (positionMs * GST_MSECOND) : 0;
    gboolean ok = gst_element_seek(
        m_pipeline,
        static_cast<gdouble>(m_config.playbackSpeed),
        GST_FORMAT_TIME,
        static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET,  target,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

    if (ok == FALSE)
        std::cerr << "FilePlayer: flush-seek after view switch failed" << std::endl;

    if (m_bus != nullptr)
    {
        GstMessage* msg = nullptr;
        while ((msg = gst_bus_pop(m_bus)) != nullptr) gst_message_unref(msg);
    }
}

// ============================================================================
// View-switch helpers
// ============================================================================

void FilePlayer::beginViewSwitch(int32_t view)
{
    if ((m_pipeline != nullptr) && (m_state == PlaybackState::PLAYING))
    {
        gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
        gst_element_get_state(m_pipeline, nullptr, nullptr, 200 * GST_MSECOND);
    }

    const char* mode = (m_config.pipelineMode == PipelineMode::GPU) ? "GPU" : "CPU";
    std::cout << "FilePlayer: view " << m_config.selectedView << " -> " << view
              << " [" << mode << "] pending..." << std::endl;

    m_pendingView = view;
    m_switchTimer = 0.0f;
}

void FilePlayer::commitViewSwitch()
{
    int64_t savedPos      = getCurrentPosition();
    int32_t prevView      = m_config.selectedView;
    bool    resume        = (m_state == PlaybackState::PLAYING);
    m_config.selectedView = m_pendingView;
    m_pendingView         = -1;
    m_switchTimer         = 0.0f;

    if (m_config.pipelineMode == PipelineMode::GPU)
    {
        applyViewToMixer();
        flushPipelineToPosition(savedPos);
        if (m_state != PlaybackState::STOPPED) changeState(PlaybackState::PAUSED);
        std::cout << "FilePlayer: view " << prevView << " -> " << m_config.selectedView
                  << " [GPU/mixer]" << std::endl;
    }
    else
    {
        rebuildPipeline(savedPos);
        std::cout << "FilePlayer: view " << prevView << " -> " << m_config.selectedView
                  << " [CPU/rebuild]" << std::endl;
    }

    if ((resume == true) && (m_state != PlaybackState::ERROR))
        play();
}

// ============================================================================
// Per-frame helpers (main thread only)
// ============================================================================

void FilePlayer::changeState(PlaybackState newState)
{
    if (m_state == newState) return;
    PlaybackState old = m_state;
    m_state = newState;
    if (m_stateChangeCb != nullptr) m_stateChangeCb(old, newState);
}

void FilePlayer::checkEndOfStream()
{
    if (m_state != PlaybackState::PLAYING) return;
    int64_t dur = getDuration();
    if ((dur > 0) && (getCurrentPosition() >= dur - 100)) stop();
}

void FilePlayer::pullAndUploadCPUFrame()
{
    if (m_videoSink == nullptr) return;

    GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(m_videoSink), 0);
    if (sample == nullptr) return;

    GstBuffer* buf  = gst_sample_get_buffer(sample);
    GstCaps*   caps = gst_sample_get_caps(sample);

    if ((buf != nullptr) && (caps != nullptr))
    {
        GstStructure* st = gst_caps_get_structure(caps, 0);
        int w = 0, h = 0;
        gst_structure_get_int(st, "width",  &w);
        gst_structure_get_int(st, "height", &h);

        if ((w > 0) && (h > 0))
        {
            GstMapInfo map;
            if (gst_buffer_map(buf, &map, GST_MAP_READ) == TRUE)
            {
                size_t sz = static_cast<size_t>(w * h * 4);
                if (map.size >= sz)
                    m_videoTexture.updateGPU(map.data, w, h);
                gst_buffer_unmap(buf, &map);
            }
        }
    }

    gst_sample_unref(sample);
}

void FilePlayer::pullAndUploadGPUFrame()
{
    if (m_videoSink == nullptr) return;

    GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(m_videoSink), 0);
    if (sample == nullptr) return;

    GstBuffer* buf  = gst_sample_get_buffer(sample);
    GstCaps*   caps = gst_sample_get_caps(sample);

    if ((buf != nullptr) && (caps != nullptr))
    {
        GstStructure* st = gst_caps_get_structure(caps, 0);
        int w = 0, h = 0;
        gst_structure_get_int(st, "width",  &w);
        gst_structure_get_int(st, "height", &h);

        if ((w > 0) && (h > 0))
        {
            GstMemory* mem = gst_buffer_peek_memory(buf, 0);
            if (gst_is_gl_memory(mem) == TRUE)
            {
                GstGLMemory* glMem = GST_GL_MEMORY_CAST(mem);
                #if GST_CHECK_VERSION(1, 18, 0)
                    guint srcTex = gst_gl_memory_get_texture_id(glMem);
                #else
                    guint srcTex = glMem->tex_id;
                #endif

                if (srcTex != 0)
                {
                    if ((m_videoTexture.id == 0) ||
                        (m_videoTexture.width  != w) ||
                        (m_videoTexture.height != h))
                    {
                        if (m_videoTexture.id != 0)
                        {
                            glFinish();
                            glDeleteTextures(1, &m_videoTexture.id);
                        }
                        glGenTextures(1, &m_videoTexture.id);
                        glBindTexture(GL_TEXTURE_2D, m_videoTexture.id);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                        m_videoTexture.width        = w;
                        m_videoTexture.height       = h;
                        m_videoTexture.isExtTexture = false;
                    }

                    GLuint fbo = 0;
                    glGenFramebuffers(1, &fbo);
                    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_TEXTURE_2D, srcTex, 0);
                    glBindTexture(GL_TEXTURE_2D, m_videoTexture.id);
                    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glDeleteFramebuffers(1, &fbo);
                }
            }
        }
    }

    gst_sample_unref(sample);
}

// ============================================================================
// GStreamer callbacks
// ============================================================================

GstFlowReturn FilePlayer::onNewSubtitleSample(GstAppSink* appsink, gpointer userData)
{
    FilePlayer* p = static_cast<FilePlayer*>(userData);

    GstSample* sample = gst_app_sink_try_pull_sample(appsink, 0);
    if (sample == nullptr) return GST_FLOW_OK;

    GstBuffer* buf = gst_sample_get_buffer(sample);
    if (buf != nullptr)
    {
        GstMapInfo map;
        if (gst_buffer_map(buf, &map, GST_MAP_READ) == TRUE)
        {
            GstClockTime pts    = GST_BUFFER_PTS(buf);
            int64_t timestampMs = (GST_CLOCK_TIME_IS_VALID(pts)) ? static_cast<int64_t>(pts / GST_MSECOND) : 0;

            std::lock_guard<std::mutex> lk(p->m_subtitleMutex);
            SubtitleParser::parse(std::string(reinterpret_cast<const char*>(map.data), map.size), p->m_subtitle);
            p->m_subtitle.timestamp = timestampMs;
            p->m_hasSubtitle        = true;
            gst_buffer_unmap(buf, &map);
        }
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

} // namespace VPB
