/*==========================================================================
 * constants.h - Common Constants and Data Structures
 *==========================================================================*/
#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

    #include <iostream>
    #include <fstream>
    #include <sstream>
    #include <memory>
    #include <string>
    #include <vector>
    #include <array>
    #include <stdexcept>
    #include <complex>
    #include <iomanip>
    #include <queue>
    #include <deque>
    #include <map>
    #include <unordered_map>
    #include <algorithm>
    #include <filesystem>
    #include <chrono>
    #include <mutex>
    #include <numeric>
    #include <functional>
    #include <atomic>
    #include <thread>
    #include <future>

    #define _USE_MATH_DEFINES
    #include <cmath>
    #include <ctime>
    #include <cstring>
    #include <cstdio>
    #include <cerrno>
    #include <cassert>
    #include <csignal>

    #include <fcntl.h>
    #include <time.h>
    #include <math.h>
    #include <dirent.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/stat.h>

    // Linux-only system headers
    #if !defined(_WIN32) || !defined(_WIN64)
        #include <sys/poll.h>
        #include <execinfo.h>
        #include <signal.h>
        #include <unistd.h>
        #include <termios.h>
        #include <pthread.h>
        #include <pwd.h>
        #include <sched.h>
        #include <linux/dma-heap.h>
        #include <linux/input.h>
        #include <linux/videodev2.h>
        #include <sys/ioctl.h>
        #include <sys/mman.h>
        #include <sys/time.h>

        // GLib
        #include <glib.h>
        #include <glib/gthread.h>

        // CAN socket
        #include <linux/can.h>
        #include <linux/can/raw.h>
        #include <sys/socket.h>
        #include <net/if.h>

        // GStreamer
        #include <gstreamer-1.0/gst/gst.h>
        #include <gstreamer-1.0/gst/gl/gl.h>
        #include <gstreamer-1.0/gst/gl/gstglmemory.h>
        #include <gstreamer-1.0/gst/gstcaps.h>
        #include <gstreamer-1.0/gst/app/gstappsink.h>
        #include <gstreamer-1.0/gst/app/gstappsrc.h>
    #endif // _WIN32

    // JSON
    #include <nlohmann/json.hpp>

    // OpenCV
    #include "opencv2/opencv.hpp"
    #include "opencv2/core.hpp"
    #include "opencv2/core/types_c.h"
    #include "opencv2/core/core_c.h"
    #include "opencv2/imgproc.hpp"
    #include "opencv2/imgproc/imgproc_c.h"
    #include "opencv2/imgcodecs.hpp"
    #include "opencv2/highgui.hpp"
    #include "opencv2/calib3d.hpp"
    #include "opencv2/videoio.hpp"

    // Platform specific
    #if defined(_WIN32) || defined(_WIN64)
        #include <windows.h>
        #include <direct.h>
        #include <gl/gl3w.h>
        #include <glfw/glfw3.h>
    #else
        #include <GLES3/gl3.h>
        #include <GLES2/gl2ext.h>
        #include <EGL/egl.h>
    #endif

    // GLM
    #if defined(_WIN32) || defined(_WIN64)
        #pragma warning(disable:4201)
    #endif
    #include "glm/glm.hpp"
    #include "glm/gtc/matrix_transform.hpp"
    #include "glm/gtc/type_ptr.hpp"
    #include <glm/gtx/quaternion.hpp>
    #if defined(_WIN32) || defined(_WIN64)
        #pragma warning(default:4201)
    #endif

    using namespace std;
    using namespace cv;
    using ordered_json = nlohmann::ordered_json;

/*--------------------------------------------------------------------------
 * Configuration Files
 *--------------------------------------------------------------------------*/
const std::string app_conf_file      = std::string("app_conf.json");
const std::string dvr_conf_file      = std::string("dvr_conf.json");
const std::string pipeline_conf_file = std::string("pipeline.txt");

const std::string DEFAULT_VIDEO_PATH = std::string("/userdata/VideoFiles/Normal/");
const std::string DEFAULT_VIDEO_FILE = std::string("test.mp4");

/*--------------------------------------------------------------------------
 * Camera Configuration
 *--------------------------------------------------------------------------*/
#ifndef SVM_CAMERAS_NUM
    #if defined(EMBEDDED_DEVICE)
        #define SVM_CAMERAS_NUM     (int) (4)   // embedded camera or recorded video
    #else
        #if defined(USE_CAMERA)
            #define SVM_CAMERAS_NUM (int) (1)   // real pc camera
        #else
            #define SVM_CAMERAS_NUM (int) (4)   // recorded video
        #endif
    #endif
#endif

#ifndef ADD_CAMERAS_NUM
    #if defined(USE_SVM)
        #define ADD_CAMERAS_NUM     (int) (1)
    #else
        #define ADD_CAMERAS_NUM     (int) (0)
    #endif
#endif

#ifndef EXT_CAMERAS_NUM
    #if defined(USE_DVR)
        #if defined(EMBEDDED_DEVICE)
            #define EXT_CAMERAS_NUM (int) (4)   // embedded camera
        #else
            #define EXT_CAMERAS_NUM (int) (0)   // real pc camera
        #endif
    #else
        #define EXT_CAMERAS_NUM     (int) (0)
    #endif
#endif

#ifndef MAX_BUFFER_NUM
    #define MAX_BUFFER_NUM          (int) (SVM_CAMERAS_NUM + ADD_CAMERAS_NUM + EXT_CAMERAS_NUM)
#endif

/*--------------------------------------------------------------------------
 * Image/Window Dimensions
 *--------------------------------------------------------------------------*/
#ifndef IMG_WIDTH
    #if defined(EMBEDDED_DEVICE)
        #define IMG_WIDTH           (int) (1920)    // embedded camera
    #else
        #if defined(USE_CAMERA)
            #define IMG_WIDTH       (int) (1280)    // real pc camera
        #else
            #define IMG_WIDTH       (int) (1920)    // recorded video
        #endif
    #endif
#endif

#ifndef IMG_HEIGHT
    #if defined(EMBEDDED_DEVICE)
        #define IMG_HEIGHT          (int) (1080)    // embedded camera
    #else
        #if defined(USE_CAMERA)
            #define IMG_HEIGHT      (int) (720)     // real pc camera
        #else
            #define IMG_HEIGHT      (int) (1080)    // recorded video
        #endif
    #endif
#endif

#ifndef WND_WIDTH
    #define WND_WIDTH               (int) (1920)
#endif
#ifndef WND_HEIGHT
    #define WND_HEIGHT              (int) (1080)
#endif

#ifndef SRC_FRAMERATE
    #define SRC_FRAMERATE           (int) (30)
#endif

#ifndef PROJECT_SRC_DIR
    #define PROJECT_SRC_DIR ".."
#endif

#define GPIO_PINS_NUM               (int) (4)
#define MOD_FRAMERATE               (int) (15)

/*--------------------------------------------------------------------------
 * Resource Paths
 *--------------------------------------------------------------------------*/
inline const std::string _RESOURCES_PATH_ = std::string(PROJECT_SRC_DIR) + std::string("/resources");
inline const std::string _AUDIOS_PATH_    = std::string(_RESOURCES_PATH_) + std::string("/audios");
inline const std::string _FONTS_PATH_     = std::string(_RESOURCES_PATH_) + std::string("/fonts");
inline const std::string _RKNNS_PATH_     = std::string(_RESOURCES_PATH_) + std::string("/rknns");
inline const std::string _SHADERS_PATH_   = std::string(_RESOURCES_PATH_) + std::string("/shaders");
inline const std::string _TEXTURES_PATH_  = std::string(_RESOURCES_PATH_) + std::string("/textures");

/*--------------------------------------------------------------------------
 * Basic Data Structures
 *--------------------------------------------------------------------------*/
struct XY
{
    union { float x; float width;  };
    union { float y; float height; };

    XY(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}

    XY operator*(float scalar)    const { return XY(x * scalar,   y * scalar);  }
    XY operator*(const XY& other) const { return XY(x * other.x,  y * other.y); }
    XY operator+(const XY& other) const { return XY(x + other.x,  y + other.y); }
};

struct XYZ
{
    float x, y, z;

    XYZ(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}

    XYZ operator*(float scalar)     const { return XYZ(x * scalar,  y * scalar,  z * scalar);  }
    XYZ operator*(const XYZ& other) const { return XYZ(x * other.x, y * other.y, z * other.z); }
    XYZ operator+(const XYZ& other) const { return XYZ(x + other.x, y + other.y, z + other.z); }
};

/*--------------------------------------------------------------------------
 * Calib types
 *--------------------------------------------------------------------------*/
static constexpr int CALIB_STEP_COUNT = 7;

enum class CalibStepStatus { LOCKED, AVAILABLE, COMPLETE };

struct CalibState
{
    int viewMode      = 0;
    int camIndex      = 0;
    int selectedPtIdx = 0;
    XY  selectedPtPos = {};

    bool keyPressed      = false;
    bool pointUpdated    = false;
    bool contourUpdated  = false;
    bool enterAdjustMode = false;

    std::array<CalibStepStatus, CALIB_STEP_COUNT> stepStates    = {};
    std::array<CalibStepStatus, SVM_CAMERAS_NUM>  camStepStates = {};
    uint8_t featureCamsComplete = 0;
    bool    stepDenied          = false;
};

/*--------------------------------------------------------------------------
 * SVM types
 *--------------------------------------------------------------------------*/

struct CameraInput
{
    float orbitAzimuth   = 0.0f;  // 1-finger drag X  (px)
    float orbitElevation = 0.0f;  // 1-finger drag Y  (px)
    float zoomDelta      = 0.0f;  // pinch spread delta (px)
    bool  resetCamera    = false; // restore xml defaults
};

// Abstract interface that uses to drive the scene animator.
struct ISvmAnimator
{
    virtual ~ISvmAnimator() = default;

    virtual void pushCameraInput   (const CameraInput& input)                   = 0;
    virtual void getCurrentPose    (glm::vec3& outPos, glm::vec3& outOri) const = 0;
    virtual void resetToDefaultPose()                                           = 0;
    virtual void setInteractive    (bool enable)                                = 0;
    virtual void setActiveViewMode (int viewModeInt)                            = 0;
    virtual void nudgeRadius       (float metres)                               = 0;

    virtual bool isInteractive() const = 0;
};

/*--------------------------------------------------------------------------
 * Unified Frame Data Structure - Used by all modules
 *--------------------------------------------------------------------------*/
struct FrameData
{
    int                        channelId;
    std::unique_ptr<uint8_t[]> data;
    size_t                     size;
    GstClockTime               pts;

    FrameData(int channel, const uint8_t* rawData, size_t dataSize, GstClockTime sourcePts = GST_CLOCK_TIME_NONE)
        : channelId(channel)
        , data(std::make_unique<uint8_t[]>(dataSize))
        , size(dataSize)
        , pts(sourcePts)
    { std::memcpy(data.get(), rawData, dataSize); }

    FrameData(FrameData&& other) noexcept
        : channelId(other.channelId)
        , data(std::move(other.data))
        , size(other.size)
        , pts(other.pts)
    { other.size = 0; }

    FrameData(const FrameData&)            = delete;
    FrameData& operator=(const FrameData&) = delete;
    FrameData& operator=(FrameData&&)      = delete;

    std::shared_ptr<FrameData> clone() const
    { return std::make_shared<FrameData>(channelId, data.get(), size, pts); }
};

/*--------------------------------------------------------------------------
 * CAN Data Structures
 *--------------------------------------------------------------------------*/
enum TURN_SIGNAL
{
    TURN_SIGNAL_OFF       = 0,
    TURN_SIGNAL_LEFT      = 1,
    TURN_SIGNAL_RIGHT     = 2,
    TURN_SIGNAL_EMERGENCY = 3
};

enum GEAR
{
    GEAR_REVERSE = -1,
    GEAR_NEUTRAL =  0,
    GEAR_DRIVING =  1,
    GEAR_PARKING =  2
};

struct VEHICLE_STATUS
{
    bool isStationary, isDriving, isReverse, isBTOActivate, isDoorOpen;
};

/*--------------------------------------------------------------------------
 * Vehicle Models
 *--------------------------------------------------------------------------*/
enum VEHICLE_MODEL
{
    Simulator        = 1, TataDaewoo       = 2, HyundaiElecCity  = 3,
    KiaBongo3Ev      = 4, TataMotorsEbus   = 5, HyundaiNewCounty = 6
};

/*--------------------------------------------------------------------------
 * Texture and Sprite Info
 *--------------------------------------------------------------------------*/
struct TEXTURE_INFO
{
    int     width, height, nchannel;
    GLenum  color_format;
    cv::Mat texture;
};

struct SPRITE_INFO
{
    int          type;
    cv::Point    size, position;
    TEXTURE_INFO texture_info;
    unsigned int texture_id;
};

/*--------------------------------------------------------------------------
 * File System Structures
 *--------------------------------------------------------------------------*/
struct FileInfo
{
    std::string name, extension, size, modification_date;
    bool        is_directory;
};

/*--------------------------------------------------------------------------
 * GStreamer Structures
 *--------------------------------------------------------------------------*/
struct TrackInfo
{
    gint num_audio_tracks, num_video_tracks, num_subtitle_tracks;
};

struct GstData
{
    GstElement* pipeline      = nullptr;
    GstBus*     bus           = nullptr;
    GstElement* inputSelector = nullptr;

    GstElement* sinkGL [MAX_BUFFER_NUM] = {nullptr};
    GstElement* sinkMod[MAX_BUFFER_NUM] = {nullptr};
    GstElement* sinkDvr[MAX_BUFFER_NUM] = {nullptr};
    GstElement* sinkSub                 = nullptr;
    GstElement* sinkAudio               = nullptr;

    bool isValid() const { return pipeline != nullptr; }

    void cleanup(bool stopPipeline = true)
    {
        if (pipeline == nullptr) { cleanupResources(); return; }

        if (stopPipeline == true)
        {
            gst_element_send_event(pipeline, gst_event_new_eos());
            gst_element_set_state(pipeline, GST_STATE_PAUSED);
            gst_element_set_state(pipeline, GST_STATE_READY);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            // Block until the transition completes; fall back to a short sleep on ASYNC.
            GstStateChangeReturn ret = gst_element_get_state(pipeline, nullptr, nullptr, 3 * GST_SECOND);
            if (ret == GST_STATE_CHANGE_ASYNC) usleep(100 * 1000);
        }
        cleanupResources();
    }

private:
    void cleanupResources()
    {
        auto unref = [](GstElement*& e) { if (e != nullptr) { gst_object_unref(e); e = nullptr; } };
        auto unrefBus = [](GstBus*& b) { if (b != nullptr) { gst_object_unref(b); b = nullptr; } };
        unrefBus(bus);
        unref(sinkSub);
        unref(sinkAudio);
        for (int i = 0; i < MAX_BUFFER_NUM; i++) { unref(sinkGL[i]); unref(sinkMod[i]); unref(sinkDvr[i]); }
        unref(inputSelector);
        unref(pipeline);
    }
};

/*--------------------------------------------------------------------------
 * Warning System Enums
 *--------------------------------------------------------------------------*/
enum WARNING_FUNCTION_TYPE { W_MOBS = 0, W_LCA  = 1, W_BTO  = 2, W_NUM  = 3 };
enum WARNING_ZONE_TYPE
{
    W_MOIS_WARNING    = 0, W_MOIS_MONITORING = 1,
    W_BSIS_WARNING    = 2, W_BSIS_MONITORING = 3,
    W_RAISED          = 4
};

/*--------------------------------------------------------------------------
 * AICam Specific
 *--------------------------------------------------------------------------*/
#ifdef USE_AICAM
    constexpr int AI_CAM_NUM               = SVM_CAMERAS_NUM;
    constexpr int AI_CAM_ROI_LEVEL_MAX_NUM = 2;
    constexpr int AI_CAM_ROI_PTS_NUM       = 4;
#endif

/*--------------------------------------------------------------------------
 * Utility Macros
 *--------------------------------------------------------------------------*/
#define PREV(x, max) (((x) > 0)    ? ((x) - 1) : (max))
#define NEXT(x, max) (((x) < (max))? ((x) + 1) : (0))

template <typename T>
std::string to_mystring(const T value, const int n = 2, const int w = 4, const char c = '0')
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(n) << std::setw(w) << std::setfill(c) << value;
    return out.str();
}

/*--------------------------------------------------------------------------
 * Time Utility Functions
 *--------------------------------------------------------------------------*/
#if !defined(_WIN32) || !defined(_WIN64)
namespace TimeUtils
{
    inline time_t steadyNow()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec;
    }

    inline double nowSec()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
    }

    inline int64_t nowMs()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
    }

    inline int64_t nowNs()
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
    }

    inline std::string getCurrentTimeString(bool forFilename = true)
    {
        auto now    = std::chrono::system_clock::now();
        auto timeT  = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&timeT);
        char buf[64];
        if (forFilename == true)
            std::snprintf(buf, sizeof(buf), "%02d%02d%02d_%02d%02d%02d",
                (tm->tm_year + 1900) % 100, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec);
        else
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec);
        return std::string(buf);
    }

    inline std::string formatSeconds(double seconds)
    {
        int    hours   = static_cast<int>(seconds / 3600);
        int    minutes = static_cast<int>(std::fmod(seconds, 3600) / 60);
        double secs    = std::fmod(seconds, 60);
        std::stringstream ss;
        if (hours != 0) ss << std::setfill('0') << std::setw(2) << hours << ":";
        ss << std::setfill('0') << std::setw(2) << minutes << ":"
           << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(5) << secs;
        return ss.str();
    }

    inline std::string formatMilliseconds(int64_t ms) { return formatSeconds(ms / 1000.0); }
}

/*--------------------------------------------------------------------------
 * Performance Timer - High-resolution timing utility
 *--------------------------------------------------------------------------*/
class PerformanceTimer
{
public:
    enum class TimeUnit { SECONDS, MILLISECONDS, MICROSECONDS, NANOSECONDS };

    static int start()
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        int id = s_nextId++;
        s_timers[id] = TimerEntry();
        return id;
    }

    static double getDuration(int timerId, TimeUnit unit = TimeUnit::NANOSECONDS, bool autoDelete = true)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_timers.find(timerId);
        if (it == s_timers.end() || it->second.isActive == false) return 0.0;

        auto   end      = std::chrono::high_resolution_clock::now();
        double duration = 0.0;
        switch (unit)
        {
            case TimeUnit::SECONDS:      duration = std::chrono::duration_cast<std::chrono::duration<double>>(end - it->second.startTime).count(); break;
            case TimeUnit::MILLISECONDS: duration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - it->second.startTime).count(); break;
            case TimeUnit::MICROSECONDS: duration = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(end - it->second.startTime).count(); break;
            case TimeUnit::NANOSECONDS:  duration = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - it->second.startTime).count()); break;
        }

        if (autoDelete == true) s_timers.erase(it);
        else                    it->second.isActive = false;
        return duration;
    }

    static uint64_t getDurationNs(int timerId, bool autoDelete = true)
    {
        return static_cast<uint64_t>(getDuration(timerId, TimeUnit::NANOSECONDS, autoDelete));
    }

    static void   deleteTimer(int timerId) { std::lock_guard<std::mutex> lock(s_mutex); s_timers.erase(timerId); }
    static void   clearAll()               { std::lock_guard<std::mutex> lock(s_mutex); s_timers.clear(); }
    static size_t getActiveCount()         { std::lock_guard<std::mutex> lock(s_mutex); return s_timers.size(); }

private:
    struct TimerEntry
    {
        std::chrono::high_resolution_clock::time_point startTime;
        bool isActive;
        TimerEntry() : startTime(std::chrono::high_resolution_clock::now()), isActive(true) {}
    };

    static inline std::map<int, TimerEntry> s_timers;
    static inline std::mutex                s_mutex;
    static inline int                       s_nextId = 1;
};
#endif // #if !defined(_WIN32) || !defined(_WIN64)

// ============================================================================
// Telemetry Data Structs — shared across CAN, UART, DVR, VPB processes
// ============================================================================

#define INVALID_RETURN(x) if (valid == false) return x

struct DateTimeData
{
    bool        valid    = false;
    std::string datetime = {};

    std::string toString() const
    {
        INVALID_RETURN("N/A");
        return datetime;
    }
};

struct GPSData
{
    bool  valid     = false;
    float latitude  = 0.0f;
    float longitude = 0.0f;

    std::string toString() const
    {
        INVALID_RETURN("N/A N/A");
        char buf[64];
        snprintf(buf, sizeof(buf), "%+011.06f %+011.06f", latitude, longitude);
        return buf;
    }
};

struct IMUData
{
    bool  valid  = false;
    float accelX = 0.0f, accelY = 0.0f, accelZ = 0.0f;
    float gyroX  = 0.0f, gyroY  = 0.0f, gyroZ  = 0.0f;

    std::string toString() const
    {
        INVALID_RETURN("N/A N/A N/A N/A N/A N/A");
        char buf[128];
        snprintf(buf, sizeof(buf), "%.3f %.3f %.3f %.3f %.3f %.3f",
                 accelX, accelY, accelZ, gyroX, gyroY, gyroZ);
        return buf;
    }
};

struct OBDData
{
    bool    valid         = false;
    int32_t speedKmh      = 0;
    int32_t rpm           = 0;
    int32_t gearPos       = 0;
    int32_t turnSignal    = 0;
    int32_t steeringAngle = 0;

    std::string toString() const
    {
        INVALID_RETURN("N/A N/A N/A N/A N/A");
        char buf[128];
        snprintf(buf, sizeof(buf), "%d %d %d %d %d",
                 speedKmh, rpm, gearPos, turnSignal, steeringAngle);
        return buf;
    }
};

#undef INVALID_RETURN

/*--------------------------------------------------------------------------
 * Seqlock: Lock-free for readers; writers serialize via atomic seq counter
 *--------------------------------------------------------------------------*/

template<typename T>
struct Seqlock
{
    void write(const T& in)
    {
        m_seq.fetch_add(1, std::memory_order_release);
        m_data = in;
        m_seq.fetch_add(1, std::memory_order_release);
    }

    // Patch any subset of fields inside fn without a full struct copy.
    // fn signature: [](T& d) { d.field = value; }
    template<typename F>
    void patch(F&& fn)
    {
        m_seq.fetch_add(1, std::memory_order_release);
        fn(m_data);
        m_seq.fetch_add(1, std::memory_order_release);
    }

    T read() const
    {
        T out;
        uint32_t s1 = 0;
        uint32_t s2 = 0;
        
        do
        {
            s1 = m_seq.load(std::memory_order_acquire);
            if ((s1 & 1) != 0) continue;   // writer mid-flight, spin
            out = m_data;
            s2  = m_seq.load(std::memory_order_acquire);
        } while (s1 != s2);                // retry if writer overlapped read
        return out;
    }

private:
    std::atomic<uint32_t> m_seq{ 0 };
    T                     m_data{};
};

/*--------------------------------------------------------------------------
 * DataUtils helpers
 *--------------------------------------------------------------------------*/
namespace DataUtils
{

    inline void gps_update(Seqlock<GPSData>& gps, float lat, float lon)
    {
        gps.patch([&](GPSData& d) { d.latitude = lat; d.longitude = lon; d.valid = true; });
    }
    inline void gps_invalidate(Seqlock<GPSData>& gps) { gps.patch([](GPSData& d) { d.valid = false; }); }


    inline void imu_update(Seqlock<IMUData>& sensor, float ax, float ay, float az, float gx, float gy, float gz)
    {
        sensor.patch([&](IMUData& d)
        {
            d.accelX = ax; d.accelY = ay; d.accelZ = az;
            d.gyroX  = gx; d.gyroY  = gy; d.gyroZ  = gz;
            d.valid  = true;
        });
    }
    inline void imu_invalidate(Seqlock<IMUData>& sensor) { sensor.patch([](IMUData& d) { d.valid = false; }); }


    inline void obd_update(Seqlock<OBDData>& obd, int32_t speed, int32_t rpm, int32_t gear, int32_t signal, int32_t angle)
    {
        obd.patch([&](OBDData& d)
        {
            d.speedKmh = speed; d.rpm = rpm; d.gearPos = gear;
            d.turnSignal = signal; d.steeringAngle = angle;
            d.valid = true;
        });
    }
    inline void obd_set_speed  (Seqlock<OBDData>& obd, int32_t v) { obd.patch([&](OBDData& d) { d.speedKmh      = v; d.valid = true; });  }
    inline void obd_set_rpm    (Seqlock<OBDData>& obd, int32_t v) { obd.patch([&](OBDData& d) { d.rpm           = v; d.valid = true; });  }
    inline void obd_set_gear   (Seqlock<OBDData>& obd, int32_t v) { obd.patch([&](OBDData& d) { d.gearPos       = v; d.valid = true; });  }
    inline void obd_set_signal (Seqlock<OBDData>& obd, int32_t v) { obd.patch([&](OBDData& d) { d.turnSignal    = v; d.valid = true; });  }
    inline void obd_set_angle  (Seqlock<OBDData>& obd, int32_t v) { obd.patch([&](OBDData& d) { d.steeringAngle = v; d.valid = true; });  }
    inline void obd_invalidate (Seqlock<OBDData>& obd)            { obd.patch([](OBDData& d)  { d.valid = false;                      }); }

    
    inline void datetime_update(Seqlock<DateTimeData>& datetime, const std::string& dt)
    {
        datetime.patch([&](DateTimeData& d) { d.datetime = dt; d.valid = true; });
    }
    inline void datetime_invalidate(Seqlock<DateTimeData>& datetime) { datetime.patch([](DateTimeData& d) { d.valid = false; }); }
}

// ============================================================================
// APP namespace
// ============================================================================

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>

#ifdef USE_IMGUI
    #include "imgui.h"
    #include "backends/imgui_impl_opengl3.h"
#endif

namespace APP
{

/*--------------------------------------------------------------------------
 * IO Module - Input/Output Constants
 *--------------------------------------------------------------------------*/
namespace IO
{
    enum class DeviceType { KEYBOARD, MOUSE, TOUCHSCREEN, DPAD };

    enum class Source : uint32_t
    {
        KEYBOARD    = 1 << 0,
        MOUSE       = 1 << 1,
        TOUCHSCREEN = 1 << 2,
        DPAD        = 1 << 3
    };

    enum class KeyCode : int32_t
    {
        UNKNOWN = 0,

        // Navigation
        DPAD_UP = 19, DPAD_DOWN = 20, DPAD_LEFT = 21, DPAD_RIGHT = 22, DPAD_CENTER = 23,

        // System
        HOME = 3, BACK = 4, ENTER = 66, ESCAPE = 111, SPACE = 62, MENU = 82,
        TAB = 61, BACKSPACE = 67,
        
        #if !defined(_WIN32) || !defined(_WIN64)
            DELETE = 112,
        #endif

        // Letters
        A = 29, B = 30, C = 31, D = 32, E = 33, F = 34, G = 35, H = 36,
        I = 37, J = 38, K = 39, L = 40, M = 41, N = 42, O = 43, P = 44,
        Q = 45, R = 46, S = 47, T = 48, U = 49, V = 50, W = 51, X = 52,
        Y = 53, Z = 54,

        // Numbers
        NUM_0 = 7, NUM_1 = 8, NUM_2 = 9, NUM_3 = 10, NUM_4 = 11,
        NUM_5 = 12, NUM_6 = 13, NUM_7 = 14, NUM_8 = 15, NUM_9 = 16,

        // Function keys
        F1 = 131, F2 = 132, F3 = 133, F4 = 134, F5 = 135,  F6 = 136,
        F7 = 137, F8 = 138, F9 = 139, F10 = 140, F11 = 141, F12 = 142,

        // Modifiers
        SHIFT_LEFT = 59, SHIFT_RIGHT = 60,
        CTRL_LEFT = 113, CTRL_RIGHT = 114,
        ALT_LEFT = 57, ALT_RIGHT = 58,
        META_LEFT = 117, META_RIGHT = 118,

        // Media
        MEDIA_PLAY_PAUSE = 85, MEDIA_STOP = 86, MEDIA_NEXT = 87, MEDIA_PREVIOUS = 88,
        VOLUME_UP = 24, VOLUME_DOWN = 25, VOLUME_MUTE = 91,
        CHANNEL_UP = 166, CHANNEL_DOWN = 167,

        // Symbols
        LEFT_BRACKET = 71, RIGHT_BRACKET = 72, MINUS = 69, EQUAL = 70,
        SEMICOLON = 74, APOSTROPHE = 75, COMMA = 55, PERIOD = 56,
        SLASH = 76, BACKSLASH = 73, GRAVE = 68,

        // Navigation
        PAGE_UP = 92, PAGE_DOWN = 93, HOME_KEY = 122, END = 123, INSERT = 124
    };

    enum class MouseButton  : int32_t { NONE = 0, LEFT = 1, RIGHT = 2, MIDDLE = 3 };
    enum class KeyAction    : int32_t { UP = 0, DOWN = 1, MULTIPLE = 2 };
    enum class MotionAction : int32_t
    {
        DOWN         = 0,
        UP           = 1,
        MOVE         = 2,
        CANCEL       = 3,
        SCROLL       = 8,
        HOVER_ENTER  = 9,
        HOVER_MOVE   = 7,
        HOVER_EXIT   = 10,
        POINTER_DOWN = 11,
        POINTER_UP   = 12,
    };

    static constexpr int MAX_TOUCH_POINTS = 10;

    struct TouchPoint
    {
        int32_t id;
        float   x, y;
        TouchPoint() : id(-1), x(0.0f), y(0.0f) {}
    };

    struct KeyEvent
    {
        KeyCode   keyCode;
        KeyAction action;
        Source    source;
        int32_t   deviceId;
        int64_t   eventTime;
        float     holdTime;

        KeyEvent()
            : keyCode(KeyCode::UNKNOWN), action(KeyAction::UP)
            , source(Source::KEYBOARD), deviceId(-1), eventTime(0), holdTime(0.0f)
        {}
    };

    struct MotionEvent
    {
        MotionAction action;
        Source       source;
        int32_t      deviceId;
        int64_t      eventTime;
        float        x, y;
        float        scrollX, scrollY;
        MouseButton  button;
        int32_t      pointerCount;
        int32_t      actionPointerIndex;
        TouchPoint   pointers[MAX_TOUCH_POINTS];
        float        scrollDelta;

        MotionEvent()
            : action(MotionAction::UP), source(Source::MOUSE), deviceId(-1), eventTime(0)
            , x(0.0f), y(0.0f), scrollX(0.0f), scrollY(0.0f)
            , button(MouseButton::NONE), pointerCount(0), actionPointerIndex(0)
            , scrollDelta(10.0f)
        {}

        bool isMouse()  const { return source == Source::MOUSE; }
        bool isTouch()  const { return source == Source::TOUCHSCREEN; }
        bool canHover() const { return isMouse(); }
    };

    struct InputEvent
    {
        enum class Type { KEY, MOTION };
        Type type;
        union { KeyEvent key; MotionEvent motion; };

        InputEvent() : type(Type::KEY), key() {}

        static InputEvent createKeyEvent   (const KeyEvent&    k) { InputEvent e; e.type = Type::KEY;    e.key    = k; return e; }
        static InputEvent createMotionEvent(const MotionEvent& m) { InputEvent e; e.type = Type::MOTION; e.motion = m; return e; }
    };

    struct DisplayConfig
    {
        int32_t width, height, refreshRate;
        float   density;
        DisplayConfig() : width(1920), height(1080), refreshRate(60), density(1.0f) {}
    };

    enum class InteractionState : uint32_t
    {
        NONE     = 0,
        HOVERED  = 1 << 0,
        FOCUSED  = 1 << 1,
        PRESSED  = 1 << 2,
        SELECTED = 1 << 3,
        DISABLED = 1 << 4
    };

    inline InteractionState operator|(InteractionState a, InteractionState b) { return static_cast<InteractionState>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
    inline InteractionState operator&(InteractionState a, InteractionState b) { return static_cast<InteractionState>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b)); }
    inline InteractionState operator~(InteractionState a) { return static_cast<InteractionState>(~static_cast<uint32_t>(a)); }
    inline bool hasState(InteractionState flags, InteractionState check) { return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(check)) != 0; }
    
    struct InputState
    {
        float   cursorX, cursorY;
        Source  lastSource;
        bool    mouseLeft, mouseRight, mouseMiddle;
        int32_t touchCount;

        InputState()
            : cursorX(0.0f), cursorY(0.0f), lastSource(Source::MOUSE)
            , mouseLeft(false), mouseRight(false), mouseMiddle(false)
            , touchCount(0)
        {}

        void updateFromKeyEvent   (const KeyEvent&    event);
        void updateFromMotionEvent(const MotionEvent& event);
        bool isMouseButtonDown    (MouseButton button) const;
        bool isPointerDown()   const { return mouseLeft || touchCount > 0; }
        bool isTouchActive()   const { return touchCount > 0; }
        bool supportsHover()   const { return lastSource == Source::MOUSE; }
        bool isKeyboardInput() const { return lastSource == Source::KEYBOARD || lastSource == Source::DPAD; }
    };

} // namespace IO

/*--------------------------------------------------------------------------
 * UI Module - User Interface Constants
 *--------------------------------------------------------------------------*/
namespace UI
{
    struct Vec2
    {
        float x, y;

        Vec2() : x(0.0f), y(0.0f) {}
        Vec2(float x_, float y_) : x(x_), y(y_) {}

        Vec2  operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
        Vec2  operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
        Vec2  operator*(float s)       const { return Vec2(x * s,   y * s);   }
        Vec2  operator/(float s)       const { return Vec2(x / s,   y / s);   }

        float length()                const { return std::sqrt(x * x + y * y); }
        float distance(const Vec2& o) const { return (*this - o).length(); }
        Vec2  normalized()            const { float l = length(); return l > 0 ? (*this) / l : Vec2(); }

        #ifdef USE_IMGUI
            ImVec2 toImGui()                 const { return ImVec2(x, y); }
            static Vec2 fromImGui(const ImVec2& v) { return Vec2(v.x, v.y); }
        #endif
    };

    struct Vec3
    {
        float x, y, z;

        Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
        Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

        Vec3  operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
        Vec3  operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
        Vec3  operator*(float s)       const { return { x * s,   y * s,   z * s   }; }
        Vec3  operator/(float s)       const { return { x / s,   y / s,   z / s   }; }

        float length()                const { return std::sqrt(x*x + y*y + z*z); }
        float distance(const Vec3& o) const { return (*this - o).length(); }
        Vec3  normalized()            const { float l = length(); return l > 0 ? (*this) / l : Vec3(); }
        Vec3  cross(const Vec3& o)    const { return { y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x }; }
        float dot(const Vec3& o)      const { return x*o.x + y*o.y + z*o.z; }
        Vec2  xy()                    const { return { x, y }; }
    };

    struct RectF
    {
        float left, top, right, bottom;

        RectF() : left(0), top(0), right(0), bottom(0) {}
        RectF(float l, float t, float r, float b) : left(l), top(t), right(r), bottom(b) {}

        static RectF fromLTRB(float l, float t, float r, float b) { return RectF(l, t, r, b); }
        static RectF fromXYWH(float x, float y, float w, float h) { return RectF(x, y, x + w, y + h); }

        float width()   const { return right - left; }
        float height()  const { return bottom - top; }
        Vec2  size()    const { return Vec2(width(), height()); }
        float centerX() const { return left + width()  * 0.5f; }
        float centerY() const { return top  + height() * 0.5f; }
        Vec2  center()  const { return Vec2(centerX(), centerY()); }

        bool contains(float x, float y) const { return x >= left && x < right && y >= top && y < bottom; }
        bool contains(const Vec2& p)    const { return contains(p.x, p.y); }
        bool intersects(const RectF& o) const { return !(left >= o.right || right <= o.left || top >= o.bottom || bottom <= o.top); }

        RectF inset(float dx, float dy)  const { return RectF(left + dx, top + dy, right - dx, bottom - dy); }
        RectF inset(float a)             const { return inset(a, a); }
        RectF offset(float dx, float dy) const { return RectF(left + dx, top + dy, right + dx, bottom + dy); }
    };

    struct Color4
    {
        float r, g, b, a;

        Color4() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
        Color4(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}

        static Color4 fromRGBA(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        { return Color4(r_/255.0f, g_/255.0f, b_/255.0f, a_/255.0f); }

        Color4 withAlpha  (float alpha)   const { return Color4(r, g, b, alpha); }
        Color4 withOpacity(float opacity) const { return Color4(r, g, b, a * opacity); }
        Color4 lerp(const Color4& t, float s) const
        {
            s = std::clamp(s, 0.0f, 1.0f);
            return Color4(r + (t.r - r) * s, g + (t.g - g) * s, b + (t.b - b) * s, a + (t.a - a) * s);
        }

        #ifdef USE_IMGUI
            ImVec4 toImGui()  const { return ImVec4(r, g, b, a); }
            ImU32  toImU32()  const { return IM_COL32((int)(r*255), (int)(g*255), (int)(b*255), (int)(a*255)); }
        #endif
    };

    namespace Color
    {
        static const Color4 Transparent = Color4::fromRGBA(  0,   0,   0,   0);
        static const Color4 Black       = Color4::fromRGBA(  0,   0,   0, 255);
        static const Color4 White       = Color4::fromRGBA(255, 255, 255, 255);
        static const Color4 Red         = Color4::fromRGBA(255,   0,   0, 255);
        static const Color4 Green       = Color4::fromRGBA(  0, 255,   0, 255);
        static const Color4 Blue        = Color4::fromRGBA(  0,   0, 255, 255);
        static const Color4 Yellow      = Color4::fromRGBA(255, 255,   0, 255);
        static const Color4 Cyan        = Color4::fromRGBA(  0, 255, 255, 255);
        static const Color4 Magenta     = Color4::fromRGBA(255,   0, 255, 255);
        static const Color4 Gray        = Color4::fromRGBA(128, 128, 128, 255);
        static const Color4 Orange      = Color4::fromRGBA(255, 165,   0, 255);
        static const Color4 Purple      = Color4::fromRGBA(128,   0, 128, 255);
        static const Color4 Pink        = Color4::fromRGBA(255, 192, 203, 255);

        static const Color4 DarkBackground       = Color4::fromRGBA( 24,  28,  35, 255);
        static const Color4 DarkSurface          = Color4::fromRGBA( 33,  38,  43, 200);
        static const Color4 CardBackground       = Color4::fromRGBA( 48,  54,  79, 180);
        static const Color4 CardBackgroundHover  = Color4::fromRGBA( 68,  76, 111, 200);
        static const Color4 CardBackgroundActive = Color4::fromRGBA( 78,  86, 126, 220);

        static const Color4 AccentPrimary   = Color4::fromRGBA(  0, 124, 226, 255);
        static const Color4 AccentSecondary = Color4::fromRGBA(160, 100, 255, 220);
        static const Color4 AccentTertiary  = Color4::fromRGBA(255, 100, 180, 220);
        static const Color4 AccentSuccess   = Color4::fromRGBA( 80, 230, 130, 220);
        static const Color4 AccentWarning   = Color4::fromRGBA(255, 180,  80, 220);
        static const Color4 AccentError     = Color4::fromRGBA(255, 100, 100, 220);
        static const Color4 AccentInfo      = Color4::fromRGBA(100, 180, 255, 220);

        static const Color4 TextPrimary   = Color4::fromRGBA(255, 255, 255, 255);
        static const Color4 TextSecondary = Color4::fromRGBA(200, 210, 230, 255);
        static const Color4 TextMuted     = Color4::fromRGBA(150, 160, 180, 255);
        static const Color4 TextDisabled  = Color4::fromRGBA(100, 110, 130, 255);

        static const Color4 Border      = Color4::fromRGBA( 80,  90, 120, 200);
        static const Color4 BorderFocus = Color4::fromRGBA(  0, 124, 226, 255);
        static const Color4 BorderHover = Color4::fromRGBA(140, 160, 255, 200);

        static const Color4 Overlay      = Color4::fromRGBA(0, 0, 0, 100);
        static const Color4 OverlayLight = Color4::fromRGBA(0, 0, 0,  50);
        static const Color4 OverlayDark  = Color4::fromRGBA(0, 0, 0, 200);
    }

    static constexpr float BORDER_W = 8.0f;

    namespace TextSize
    {
        static const float ExtraSmall = 12.0f;
        static const float Small      = 13.0f;
        static const float Normal     = 16.0f;
        static const float Medium     = 18.0f;
        static const float Large      = 20.0f;
        static const float ExtraLarge = 24.0f;
        static const float Huge       = 32.0f;
        static const float Title      = 36.0f;
        static const float Display    = 48.0f;
    }

    enum class CharacterSet { ASCII, LATIN_EXT, CJK_BASIC, CJK_FULL, KOREAN, CUSTOM };

    struct CodepointRange
    {
        uint32_t first, last;
        CodepointRange() : first(0), last(0) {}
        CodepointRange(uint32_t f, uint32_t l) : first(f), last(l) {}
    };

    struct FontConfig
    {
        std::string id, name, path;
        float       size;
        CharacterSet charset;
        int         atlasWidth, atlasHeight;
        std::vector<CodepointRange> customRanges;

        FontConfig() : size(36.0f), charset(CharacterSet::ASCII), atlasWidth(1024), atlasHeight(1024) {}
        FontConfig(const std::string& id_, const std::string& name_, const std::string& path_,
                   float size_, CharacterSet charset_, int w, int h)
            : id(id_), name(name_), path(path_), size(size_), charset(charset_), atlasWidth(w), atlasHeight(h)
        {}
    };

    struct TextProps
    {
        std::string fontId;
        float       size;

        TextProps() : fontId("default"), size(24.0f) {}
        TextProps(const std::string& id, float sz) : fontId(id), size(sz) {}
        bool operator==(const TextProps& o) const { return fontId == o.fontId && std::abs(size - o.size) < 0.01f; }
    };

    struct Paint
    {
        Color4     bgColor, fgColor;
        bool       filled;
        float      strokeWidth, cornerRadius, opacity;
        TextProps  textProps;

        Paint()                                     : bgColor(Color::White), fgColor(Color::Transparent), filled(true), strokeWidth(1.0f), cornerRadius(0.0f), opacity(1.0f), textProps() {}
        Paint(const Color4& bg)                     : bgColor(bg),           fgColor(Color::Transparent), filled(true), strokeWidth(1.0f), cornerRadius(0.0f), opacity(1.0f), textProps() {}
        Paint(const Color4& bg, const Color4& fg, float strokeW = 1.0f)
                                                    : bgColor(bg), fgColor(fg), filled(true), strokeWidth(strokeW), cornerRadius(0.0f), opacity(1.0f), textProps() {}

        Color4 getEffectiveBgColor() const { return bgColor.withOpacity(opacity); }
        Color4 getEffectiveFgColor() const { return fgColor.withOpacity(opacity); }
    };

    enum class Visibility   { VISIBLE, INVISIBLE, GONE };
    enum class Orientation  { HORIZONTAL, VERTICAL };
    enum class ScaleType    { FIT_XY, FIT_CENTER, CENTER_CROP, CENTER_INSIDE };

    enum class Gravity
    {
        NO_GRAVITY        = 0,
        LEFT              = 1 << 0,
        RIGHT             = 1 << 1,
        CENTER_HORIZONTAL = 1 << 2,
        TOP               = 1 << 3,
        BOTTOM            = 1 << 4,
        CENTER_VERTICAL   = 1 << 5,
        CENTER            = CENTER_HORIZONTAL | CENTER_VERTICAL,
        TOP_LEFT          = TOP    | LEFT,
        TOP_RIGHT         = TOP    | RIGHT,
        BOTTOM_LEFT       = BOTTOM | LEFT,
        BOTTOM_RIGHT      = BOTTOM | RIGHT
    };

    inline Gravity operator|(Gravity a, Gravity b) { return static_cast<Gravity>(static_cast<int>(a) | static_cast<int>(b)); }
    inline Gravity operator&(Gravity a, Gravity b) { return static_cast<Gravity>(static_cast<int>(a) & static_cast<int>(b)); }
    inline bool hasGravity(Gravity flags, Gravity check) { return (static_cast<int>(flags) & static_cast<int>(check)) != 0; }
    
    // Layout
    constexpr int MATCH_PARENT = -1;
    constexpr int WRAP_CONTENT = -2;

    struct LayoutParams
    {
        int   width, height;
        float weight;
        Gravity gravity;
        float marginLeft, marginTop, marginRight, marginBottom;
        float paddingLeft, paddingTop, paddingRight, paddingBottom;

        LayoutParams()
            : width(WRAP_CONTENT), height(WRAP_CONTENT), weight(0.0f), gravity(Gravity::NO_GRAVITY)
            , marginLeft(0), marginTop(0), marginRight(0), marginBottom(0)
            , paddingLeft(0), paddingTop(0), paddingRight(0), paddingBottom(0)
        {}
        virtual ~LayoutParams() = default;

        void setMargin (float a)                         { marginLeft  = marginTop   = marginRight  = marginBottom  = a; }
        void setMargin (float h, float v)                { marginLeft  = marginRight = h; marginTop  = marginBottom = v; }
        void setMargin (float l, float t, float r, float b) { marginLeft = l; marginRight = r; marginTop = t; marginBottom = b; }
        void setPadding(float a)                         { paddingLeft = paddingTop  = paddingRight = paddingBottom = a; }
        void setPadding(float h, float v)                { paddingLeft = paddingRight = h; paddingTop = paddingBottom = v; }
        void setPadding(float l, float t, float r, float b) { paddingLeft = l; paddingRight = r; paddingTop = t; paddingBottom = b; }

        float getMarginHorizontal()  const { return marginLeft  + marginRight;  }
        float getMarginVertical()    const { return marginTop   + marginBottom;  }
        float getPaddingHorizontal() const { return paddingLeft + paddingRight;  }
        float getPaddingVertical()   const { return paddingTop  + paddingBottom; }

        bool isMatchParentWidth()  const { return width  == MATCH_PARENT; }
        bool isWrapContentWidth()  const { return width  == WRAP_CONTENT; }
        bool isExactWidth()        const { return width  > 0; }
        bool isMatchParentHeight() const { return height == MATCH_PARENT; }
        bool isWrapContentHeight() const { return height == WRAP_CONTENT; }
        bool isExactHeight()       const { return height > 0; }
        bool hasWeight()           const { return weight > 0.0f; }
    };

    enum class MeasureSpecMode { UNSPECIFIED, EXACTLY, AT_MOST };

    struct MeasureSpec
    {
        float          size;
        MeasureSpecMode mode;

        MeasureSpec() : size(0), mode(MeasureSpecMode::UNSPECIFIED) {}
        MeasureSpec(float s, MeasureSpecMode m) : size(s), mode(m) {}

        static MeasureSpec makeUnspecified()        { return MeasureSpec(0,    MeasureSpecMode::UNSPECIFIED); }
        static MeasureSpec makeExactly(float size)  { return MeasureSpec(size, MeasureSpecMode::EXACTLY);     }
        static MeasureSpec makeAtMost (float size)  { return MeasureSpec(size, MeasureSpecMode::AT_MOST);     }
    };

} // namespace UI

/*--------------------------------------------------------------------------
 * AI Module
 *--------------------------------------------------------------------------*/

namespace AI
{
    struct BoundingBox
    {
        float       x, y, width, height; // Normalized in [0-1] relative to original image
        int         classId, domainId;   // Raw model class id & Project domain id; -1 = unknown
        UI::Color4  color;
        std::string label;
        float       confidence;

        BoundingBox()
            : x(0.0f), y(0.0f), width(0.0f), height(0.0f), classId(-1), domainId(-1)
            , color(UI::Color::AccentPrimary), label(""), confidence(1.0f)
        {}

        BoundingBox(float x_, float y_, float w_, float h_,
                    int classId_ = -1, int domainId_ = -1,
                    const UI::Color4& color_ = UI::Color::AccentPrimary,
                    const std::string& label_ = "", float confidence_ = 1.0f)
            : x(x_), y(y_), width(w_), height(h_), classId(classId_), domainId(domainId_)
            , color(color_), label(label_), confidence(confidence_)
        {}

        UI::Vec2 topLeft()  const { return { x,         y          }; }
        UI::Vec2 topRight() const { return { x + width, y          }; }
        UI::Vec2 botLeft()  const { return { x,         y + height }; }
        UI::Vec2 botRight() const { return { x + width, y + height }; }

        // Convert from image coordinates to normalized [0-1]
        static BoundingBox fromImageCoords(float imgX, float imgY, float imgW, float imgH,
                                           float imageWidth, float imageHeight,
                                           int classId = -1, int domainId = -1,
                                           const UI::Color4& color = UI::Color::AccentPrimary,
                                           const std::string& label = "", float confidence = 1.0f)
        {
            return BoundingBox(imgX / imageWidth, imgY / imageHeight,
                               imgW / imageWidth, imgH / imageHeight,
                               classId, domainId, color, label, confidence);
        }
    };

    struct ROIZone
    {
        std::vector<UI::Vec2> points; // Normalized points [0-1] relative to image
        UI::Color4            fillColor, strokeColor;
        float                 strokeWidth;
        bool                  visible;

        ROIZone() : strokeWidth(5.0f), visible(true) {}
        ROIZone(const std::vector<UI::Vec2>& pts,
                const UI::Color4& fill, const UI::Color4& stroke,
                float width = 5.0f, bool vis = true)
            : points(pts), fillColor(fill), strokeColor(stroke), strokeWidth(width), visible(vis)
        {}
    };

    struct DomainClass
    {
        int         classId, domainId; // Raw model class id & Project domain id; -1 = unknown
        const char* label;
        UI::Color4  color;
    };

    inline const DomainClass DOMAIN_CLASSES[] =
    {
    //  classId  domainId  label       color
        {  0,      0,   "Person",   UI::Color::AccentError   },  // person
        {  1,      1,   "Bicycle",  UI::Color::AccentWarning },  // bicycle
        {  2,      1,   "Bicycle",  UI::Color::AccentWarning },  // motorcycle
        {  3,      2,   "Vehicle",  UI::Color::AccentInfo    },  // car
        {  4,      2,   "Vehicle",  UI::Color::AccentInfo    },  // bus
        {  5,      2,   "Vehicle",  UI::Color::AccentInfo    },  // truck
    };

    inline const DomainClass* getByClassId(int classId)
    {
        for (const DomainClass& c : DOMAIN_CLASSES)
            if (c.classId == classId) return &c;
        return nullptr;
    }

    // ── Geometry helpers ──────────────────────────────────────────────────────
    // All helpers operate in any consistent 2-D coordinate space (normalised [0,1], pixel, etc.)

    // // Returns true when point P lies on segment P0→P1 (including endpoints).
    // inline bool ptOnSegment(UI::Vec2 P, UI::Vec2 P0, UI::Vec2 P1)
    // {
    //     UI::Vec2 a = P0 - P, b = P1 - P;
    //     float det  = a.x * b.y - b.x * a.y;   // cross product (= 0 ↔ collinear)
    //     float prod = a.x * b.x + a.y * b.y;   // dot product   (< 0 ↔ P between P0,P1)
    //     return (det == 0.0f && prod < 0.0f)
    //         || (a.x == 0.0f && a.y == 0.0f)    // P == P0
    //         || (b.x == 0.0f && b.y == 0.0f);   // P == P1
    // }

    // // Returns true when point P is inside (or on the border of) a polygon
    // inline bool ptInPolygon(const UI::Vec2* verts, int n, UI::Vec2 P, bool includeBorder = true)
    // {
    //     std::complex<float> sum(0.0f, 0.0f);
    //     for (int i = 0; i < n; ++i)
    //     {
    //         UI::Vec2 v0 = verts[i];
    //         UI::Vec2 v1 = verts[(i + 1) % n];
    //         if (ptOnSegment(P, v0, v1) == true) return includeBorder;
    //         sum += std::log(
    //             (std::complex<float>(v1.x, v1.y) - std::complex<float>(P.x, P.y)) /
    //             (std::complex<float>(v0.x, v0.y) - std::complex<float>(P.x, P.y)));
    //     }
    //     return std::abs(sum) > 1.0f;
    // }

    // Returns true when point P is inside (or on the border of) a polygon
    inline bool ptInPolygon(const UI::Vec2* verts, int n, UI::Vec2 P, bool includeBorder = true)
    {
        int winding = 0;
        for (int i = 0; i < n; ++i)
        {
            UI::Vec2 v0 = verts[i];
            UI::Vec2 v1 = verts[(i + 1) % n];

            // Border check: is P on this edge?
            if (includeBorder == true)
            {
                // Cross product of (v1-v0) × (P-v0) == 0 means collinear.
                float cross = (v1.x - v0.x) * (P.y - v0.y) - (v1.y - v0.y) * (P.x - v0.x);
                if (cross == 0.0f)
                {
                    float minX = v0.x < v1.x ? v0.x : v1.x;
                    float maxX = v0.x > v1.x ? v0.x : v1.x;
                    float minY = v0.y < v1.y ? v0.y : v1.y;
                    float maxY = v0.y > v1.y ? v0.y : v1.y;
                    if (P.x >= minX && P.x <= maxX && P.y >= minY && P.y <= maxY) return true;
                }
            }

            // Winding number: count upward/downward edge crossings of horizontal ray.
            if (v0.y <= P.y)
            {
                if (v1.y > P.y)   // upward crossing
                {
                    float cross = (v1.x - v0.x) * (P.y - v0.y) - (v1.y - v0.y) * (P.x - v0.x);
                    if (cross > 0.0f) ++winding;
                }
            }
            else
            {
                if (v1.y <= P.y)  // downward crossing
                {
                    float cross = (v1.x - v0.x) * (P.y - v0.y) - (v1.y - v0.y) * (P.x - v0.x);
                    if (cross < 0.0f) --winding;
                }
            }
        }
        return winding != 0;
    }

    // Convenience overload for std::vector.
    inline bool ptInPolygon(const std::vector<UI::Vec2>& poly, UI::Vec2 P, bool includeBorder = true)
    {
        return ptInPolygon(poly.data(), static_cast<int>(poly.size()), P, includeBorder);
    }

    // Convenience overload for std::array<UI::Vec2, N>.
    template <std::size_t N>
    inline bool ptInPolygon(const std::array<UI::Vec2, N>& poly, UI::Vec2 P, bool includeBorder = true)
    {
        return ptInPolygon(poly.data(), static_cast<int>(N), P, includeBorder);
    }

    inline bool ptInPolygonXY(const UI::Vec3* poly, int n, UI::Vec3 P, bool includeBorder = true)
    {
        // Project to XY, reuse existing 2D function
        std::vector<UI::Vec2> proj(n);
        for (int i = 0; i < n; ++i) proj[i] = UI::Vec2(poly[i].x, poly[i].y);
        return ptInPolygon(proj.data(), n, UI::Vec2(P.x, P.y), includeBorder);
    }

    inline bool ptInPolygonXY(const std::vector<UI::Vec3>& poly, UI::Vec3 P, bool includeBorder = true)
    {
        return ptInPolygonXY(poly.data(), static_cast<int>(poly.size()), P, includeBorder);
    }

    // Returns true when 2 segment are intersect
    inline bool segsIntersect(UI::Vec2 A0, UI::Vec2 A1, UI::Vec2 P0, UI::Vec2 P1)
    {
        UI::Vec2 da = A1 - A0;
        UI::Vec2 db = P1 - P0;

        float denom = da.x * db.y - da.y * db.x;
        if (denom == 0.0f) return false;   // parallel or collinear

        UI::Vec2 gap = P0 - A0;
        float t = (gap.x * db.y - gap.y * db.x) / denom;
        float u = (gap.x * da.y - gap.y * da.x) / denom;

        return (t > 0.0f && t < 1.0f && u > 0.0f && u < 1.0f);
    }

    // Returns true when bounding-box bb and zone polygon share any common area.
    inline bool bboxOverlapsZone(const BoundingBox& bb, const std::vector<UI::Vec2>& zonePts)
    {
        if (zonePts.empty() == true) return false;

        // Test 1: bbox corners inside zone
        if (ptInPolygon(zonePts, bb.topLeft())  == true) return true;
        if (ptInPolygon(zonePts, bb.topRight()) == true) return true;
        if (ptInPolygon(zonePts, bb.botLeft())  == true) return true;
        if (ptInPolygon(zonePts, bb.botRight()) == true) return true;

        // Test 2: zone vertices inside axis-aligned bbox polygon
        const std::array<UI::Vec2, 4> bboxPoly = { bb.botLeft(), bb.topLeft(), bb.topRight(), bb.botRight() };
        for (const UI::Vec2& p : zonePts)
            if (ptInPolygon(bboxPoly, p) == true) return true;

        // Test 3: edge-crossing (convex tips overlapping)
        const std::array<std::pair<UI::Vec2,UI::Vec2>, 4> bboxEdges = {{
            { bb.topLeft(),  bb.topRight() }, { bb.topRight(), bb.botRight() },
            { bb.botRight(), bb.botLeft()  }, { bb.botLeft(),  bb.topLeft()  },
        }};
        const int nZone = static_cast<int>(zonePts.size());
        for (const auto& be : bboxEdges)
            for (int i = 0; i < nZone; ++i)
                if (segsIntersect(be.first, be.second, zonePts[i], zonePts[(i + 1) % nZone]) == true)
                    return true;

        // // Test 3: vertical centre-line samples inside zone
        // float zoneMinY = zonePts[0].y, zoneMaxY = zonePts[0].y;
        // for (const UI::Vec2& p : zonePts)
        // {
        //     if (p.y < zoneMinY) zoneMinY = p.y;
        //     if (p.y > zoneMaxY) zoneMaxY = p.y;
        // }
        // int N = static_cast<int>(bb.height / ((zoneMaxY - zoneMinY) * 0.5f));
        // if (N < 1) N = 1;
        // float cx = bb.x + bb.width * 0.5f;
        // for (int j = 0; j < N; ++j)
        // {
        //     float cy = bb.y + bb.height * (static_cast<float>(j) / static_cast<float>(N));
        //     if (ptInPolygon(zonePts, { cx, cy }) == true) return true;
        // }

        return false;
    }

    // Returns the index of the first ROI level (lowest index = highest danger)
    // that bbox overlaps, or -1 if no overlap, roiLevels[i] is the polygon for level i.
    inline int bboxWarnLevel(const BoundingBox& bb, const std::vector<std::vector<UI::Vec2>>& roiLevels)
    {
        for (int lvl = 0; lvl < static_cast<int>(roiLevels.size()); ++lvl)
            if (bboxOverlapsZone(bb, roiLevels[lvl]) == true) return lvl;
        return -1;
    }

} // namespace AI

/*--------------------------------------------------------------------------
 * IPC Module - Shared-memory frame header (atomic-only, no semaphores)
 *--------------------------------------------------------------------------*/

namespace IPC
{
    struct FrameHdr
    {
        std::atomic<uint32_t> width, height, bytesPerPixel;
        std::atomic<uint64_t> frameNumber, timestamp;
        std::atomic<uint32_t> dataSize;
        std::atomic<bool>     isValid, writerBusy;
        std::atomic<int>      readerCount;

        FrameHdr()
        {
            width.store(0); height.store(0); bytesPerPixel.store(4);
            frameNumber.store(0); timestamp.store(0); dataSize.store(0);
            isValid.store(false); writerBusy.store(false); readerCount.store(0);
        }
    };
} // namespace IPC

/*--------------------------------------------------------------------------
 * GST Module - GStreamer source configuration
 *--------------------------------------------------------------------------*/

namespace GST
{
    struct SrcCfg
    {
        int         id, width, height, fps;
        std::string name, pipeline;

        SrcCfg() : id(0), width(1920), height(1080), fps(30) {}
        SrcCfg(int id_, const std::string& name_, const std::string& pipeline_,
               int w_ = 1920, int h_ = 1080, int f_ = 30)
            : id(id_), width(w_), height(h_), fps(f_), name(name_), pipeline(pipeline_)
        {}
    };
} // namespace GST

/*--------------------------------------------------------------------------
 * WKR Module - Worker data types (sensors, alerts, I/O commands)
 *--------------------------------------------------------------------------*/

namespace WKR
{
    enum class SensorType { GPS, ACCELEROMETER, GYROSCOPE, TEMPERATURE, PRESSURE, COLLISION, PROXIMITY, CUSTOM };

    struct SensorData
    {
        SensorType         type;
        uint64_t           timestamp;
        std::vector<float> values;

        SensorData() : type(SensorType::CUSTOM), timestamp(0) {}
        SensorData(SensorType t, const std::vector<float>& v) : type(t), values(v)
        {
            timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    };

    enum class AlertLevel { INFO, WARNING, CRITICAL };
    enum class AlertType  { COLLISION, SPEED_LIMIT, PARKING_MOTION, TEMPERATURE, SYSTEM_ERROR, CUSTOM };

    struct AlertData
    {
        AlertType   type;
        AlertLevel  level;
        std::string message;
        uint64_t    timestamp;

        AlertData() : type(AlertType::CUSTOM), level(AlertLevel::INFO), timestamp(0) {}
        AlertData(AlertType t, AlertLevel l, const std::string& msg) : type(t), level(l), message(msg)
        {
            timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    };

    enum class IOType { GPIO, SERIAL, CAN_BUS, I2C, SPI };

    struct IOCmd
    {
        std::string      target;   // e.g. "GPIO_17", "CAN0"
        std::string      action;   // e.g. "SET_HIGH", "SEND", "TOGGLE", "PULSE"
        std::vector<int> data;
    };
} // namespace WKR


using namespace IO;
using namespace UI;
using namespace IPC;
using namespace GST;
using namespace WKR;

} // namespace APP

#endif // __CONSTANTS_H__
