#ifndef VPB_FILE_PLAYER_H
#define VPB_FILE_PLAYER_H

#include "constants.h"
#include "ui_core.h"
#include "vpb_const.h"
#include "vpb_subtitle.h"
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/gl/gl.h>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

namespace VPB
{

struct PlayerConfig
{
    int32_t      selectedView  = 0;
    float        playbackSpeed = 1.0f;
    PipelineMode pipelineMode  = PipelineMode::GPU;
    float        volume        = 1.0f;
};

class FilePlayer
{
public:
    FilePlayer();
    ~FilePlayer();

    // Lifecycle
    bool initialize();
    void shutdown();

    // File
    bool loadFile(const std::string& path);
    void unloadFile();

    // Playback
    void play();
    void pause();
    void stop();
    void seek(int64_t positionMs);

    // Setters
    void setConfig(const PlayerConfig& config);
    void setSelectedView(int32_t view);
    void setPlaybackSpeed(float speed);
    void setPipelineMode(PipelineMode mode);
    void setVolume(float volume);
    void setStateChangeCallback(std::function<void(PlaybackState, PlaybackState)> cb) { m_stateChangeCb = cb; }

    // Getters
    bool                isFileLoaded()       const { return m_fileLoaded; }
    bool                hasAudio()           const { return m_metadata.audioTrackCount > 0; }
    PlaybackState       getState()           const { return m_state; }
    int64_t             getCurrentPosition() const;
    int64_t             getDuration()        const { return m_metadata.duration; }
    const FileMetadata& getMetadata()        const { return m_metadata; }
    const PlayerConfig& getConfig()          const { return m_config; }
    const SubtitleData* getCurrentSubtitle() const;
    std::string         getCurrentFilePath() const { return m_currentFilePath; }
    APP::UI::Texture*   getVideoTexture();

    // Update (call every frame on the main/render thread)
    void update(float deltaTime);

private:
    // Core state
    bool          m_initialized = false;
    bool          m_fileLoaded  = false;
    PlaybackState m_state       = PlaybackState::STOPPED;
    PlayerConfig  m_config      = {};
    FileMetadata  m_metadata    = {};
    std::string   m_currentFilePath;

    // GStreamer objects
    GstElement* m_pipeline     = nullptr;
    GstElement* m_videoSink    = nullptr;
    GstElement* m_subtitleSink = nullptr;
    GstBus*     m_bus          = nullptr;

    // View-switch debounce
    static constexpr float SWITCH_COMMIT_SEC = 0.4f;
    int32_t m_pendingView = -1;
    float   m_switchTimer = 0.0f;

    // Video texture
    APP::UI::Texture m_videoTexture = {};

    // Subtitle
    mutable std::mutex m_subtitleMutex;
    SubtitleData       m_subtitle    = {};
    bool               m_hasSubtitle = false;

    // Callback
    std::function<void(PlaybackState, PlaybackState)> m_stateChangeCb;

    // Pipeline helpers
    bool        readFileMetadata();
    std::string buildPipelineString() const;
    void        getViewChannels(int32_t view, std::vector<int32_t>& channels, bool& isGrid) const;
    bool        createPipeline();
    void        configureSinks();
    void        destroyPipeline();
    bool        rebuildPipeline(int64_t savedPos);
    void        applyViewToMixer();
    void        flushPipelineToPosition(int64_t positionMs);

    // View-switch helpers
    void beginViewSwitch(int32_t view);
    void commitViewSwitch();

    // Per-frame helpers (main thread only)
    void changeState(PlaybackState newState);
    void checkEndOfStream();
    void pullAndUploadCPUFrame();
    void pullAndUploadGPUFrame();

    // GStreamer callbacks
    static GstFlowReturn onNewSubtitleSample(GstAppSink*, gpointer);
};

} // namespace VPB

#endif // VPB_FILE_PLAYER_H
