#ifndef IO_PLATFORM_H
#define IO_PLATFORM_H

#include "constants.h"
#include <functional>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <mutex>
#include <termios.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <EGL/eglext.h>

#ifdef USE_GSTREAMER
    #include <gst/gst.h>
    #include <gst/gl/gl.h>
#endif // USE_GSTREAMER

// ============================================================================
// Platform-Specific Headers
// ============================================================================

#ifdef USE_X11
    #ifdef USE_GSTREAMER
        #include <gstreamer-1.0/gst/gl/x11/gstgldisplay_x11.h>
    #endif
    #include <X11/Xlib.h>
    #include <X11/Xatom.h>
    #include <X11/extensions/XInput2.h>
#endif

#ifdef USE_DRM
    #ifdef USE_GSTREAMER
        #include <gstreamer-1.0/gst/gl/egl/gstgldisplay_egl.h>
    #endif
    #include <xf86drm.h>
    #include <xf86drmMode.h>
    #include <gbm.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <libdrm/drm_fourcc.h>
    #include <linux/input.h>
#endif

namespace APP
{
namespace IO
{

// ============================================================================
// Forward Declarations
// ============================================================================

class InputReader;
class InputHandler;
class DisplayManager;

// ============================================================================
// Type Aliases
// ============================================================================

using InputEventCallback       = std::function<void(const InputEvent&)>;
using TerminalCommandCallback  = std::function<void(const std::string&)>;
using DeviceConnectionCallback = std::function<void(const std::string& deviceName, int eventNumber, DeviceType type, bool connected)>;

// ============================================================================
// Platform-Specific Structures
// ============================================================================

#ifdef USE_X11

struct X11Output
{
    Window     nativeWindow;
    EGLSurface surface;
    EGLContext context;
    
    X11Output();
};

#endif // USE_X11

#ifdef USE_DRM

struct TouchCalibration
{
    int32_t minX, maxX, minY, maxY;
    int32_t displayWidth, displayHeight;
    bool    needsMapping;
    
    TouchCalibration();
    float mapX(int32_t rawX) const;
    float mapY(int32_t rawY) const;
};

struct InputDevice
{
    int              fd, eventNumber;
    std::string      name;
    DeviceType       type;
    TouchCalibration touchCalibration;
    bool             isMTDevice;       // true = has ABS_MT_POSITION_X/Y; ignore ABS_X/ABS_Y

    InputDevice();
};

struct DrmOutput
{
    uint32_t        connectorId, encoderId, crtcId, frameBufferId;
    drmModeModeInfo displayMode;
    gbm_surface*    gbmSurface;
    EGLSurface      surface;
    EGLContext      context;
    gbm_bo*         previousBuffer;
    uint32_t        previousFrameBuffer;
    std::string     connectorName;
    bool            isActive;
    
    DrmOutput();
};

struct DisplayInfo
{
    std::string name;
    std::string status;
    bool        isActive;
    int         width, height, refreshRate;
    
    DisplayInfo();
};

#endif // USE_DRM

// ============================================================================
// InputReader - Platform input device handling
// ============================================================================

class InputReader
{
public:
    // Constructor & Destructor
    InputReader();
    ~InputReader();
    
    // Lifecycle
    bool    initialize();
    void    shutdown();
    
    // Input polling
    void    pollEvents(std::vector<InputEvent>& outEvents);
    
    // Configuration
    void    setDisplaySize(int32_t width, int32_t height);
    void    setTerminalCommandCallback(TerminalCommandCallback callback) { m_terminalCallback = callback; }
    void    setDeviceConnectionCallback(DeviceConnectionCallback callback) { m_deviceConnectionCallback = callback; }
    int32_t getDeviceCount() const { return m_deviceCount; }
    
    #ifdef USE_X11
        void setX11Window(Display* display, Window window);
    #endif
    
    #ifdef USE_DRM
        void notifyExistingDevices();
    #endif
    
private:
    // Common members - state
    int32_t                         m_deviceCount;
    int32_t                         m_displayWidth;
    int32_t                         m_displayHeight;
    bool                            m_terminalInputEnabled;
    
    // Common members - terminal
    int                             m_stdinFd;
    struct termios                  m_originalTermios;
    std::string                     m_commandBuffer;
    
    // Common members - callbacks
    TerminalCommandCallback         m_terminalCallback;
    DeviceConnectionCallback        m_deviceConnectionCallback;
    
    // Terminal input methods
    void    setupTerminalInput();
    void    shutdownTerminalInput();
    void    pollTerminalInput(std::vector<InputEvent>& outEvents);
    void    processTerminalCommand(const std::string& command, std::vector<InputEvent>& outEvents);
    KeyCode translateTerminalKey(char c);
    
    #ifdef USE_X11
        Display*               m_x11Display;
        Window                 m_x11Window;
        int                    m_xi2Opcode;
        std::map<int, int64_t> m_x11KeyDownTime;

        void    setupX11Input();
        void    pollX11Events(std::vector<InputEvent>& outEvents);
        KeyCode translateX11KeyCode(int x11KeySym);
    #endif
    
    #ifdef USE_DRM
        std::vector<InputDevice>                          m_connectedDevices;
        std::map<int, std::pair<std::string, DeviceType>> m_knownDevices;
        std::map<int, int64_t>                            m_keyDownTime;
        int                                               m_framesSinceLastScan;
        bool                                              m_scanningDevices;
        float                                             m_mouseX;
        float                                             m_mouseY;
        
        void    setupDrmInput();
        void    pollDrmEvents(std::vector<InputEvent>& outEvents);
        void    scanAndOpenDevices();
        void    checkDeviceHealth();
        void    calibrateTouchscreen(InputDevice& device);
        void    logDeviceConnected(const InputDevice& dev);
        void    logDeviceDisconnected(const InputDevice& dev);
        KeyCode translateLinuxKeyCode(int linuxKeyCode);
    #endif
    
    friend class DisplayManager;
    friend class Platform;
};

// ============================================================================
// InputHandler - Event dispatch and state tracking
// ============================================================================

class InputHandler
{
public:
    // Constructor & Destructor
    InputHandler();
    ~InputHandler();
    
    // Event handling
    void setCallback(InputEventCallback callback) { m_callback = callback; }
    void handleEvent(const InputEvent& event);
    
    // State queries
    const InputState& getState() const           { return m_state; }
    UI::Vec2          getCursorPosition() const  { return UI::Vec2(m_state.cursorX, m_state.cursorY); }
    bool              isPointerDown() const      { return m_state.isPointerDown(); }
    bool              isMouseButtonDown(MouseButton button) const { return m_state.isMouseButtonDown(button); }
    bool              supportsHover() const      { return m_state.supportsHover(); }
    bool              isKeyboardInput() const    { return m_state.isKeyboardInput(); }
    
private:
    InputEventCallback m_callback;
    InputState         m_state;
};

// ============================================================================
// DisplayManager - Display, EGL, and GStreamer GL management
// ============================================================================

class DisplayManager
{
public:
    // Constructor & Destructor
    DisplayManager();
    ~DisplayManager();
    
    // Lifecycle
    bool initialize(const char* title, int width, int height, int windowCount = 1);
    void shutdown();
    
    // Frame rendering
    void beginFrame(int outputIndex = 0);
    void endFrame(int outputIndex = 0);
    
    // Context management
    bool makeGLContextCurrent(int outputIndex = 0);
    bool releaseGLContext();
    
    // Display queries
    bool          shouldClose()                                    const { return m_shouldClose; }
    int           getOutputCount()                                 const;
    void          getDisplaySize(int& width, int& height, int outputIndex = 0) const;
    DisplayConfig getDisplayConfig(int outputIndex = 0)            const;

    // Power management
    void setDisplayPowerTimeout(int seconds = 0, bool useDpms = false);
    void notifyUserActivity();
    void updateDisplayPower();
    bool isDisplayOn() const { return m_displayOn; }

    // GStreamer accessors
    #ifdef USE_GSTREAMER
        GstGLDisplay* getGstGLDisplay() const { return m_gstGlDisplay; }
        GstGLContext* getGstGLContext() const { return m_gstGlContext; }
        GstContext*   getGstContext()   const { return m_gstContext; }
    #endif // USE_GSTREAMER
    
    // EGL accessors
    EGLDisplay getEGLDisplay()       const { return m_eglDisplay; }
    EGLContext getEGLSharedContext() const { return m_eglSharedContext; }
    EGLConfig  getEGLConfig()        const { return m_eglConfig; }
    EGLSurface getEGLSurface(int outputIndex = 0) const;
    EGLContext getEGLContext(int outputIndex = 0) const;
    
private:
    // State
    bool   m_shouldClose;

    // Power Management
    bool     m_displayOn         = true;
    bool     m_useDpms           = false;   // false = render black (instant wake); true = DPMS signal cut (slow wake)
    int      m_powerTimeoutSecs  = 0;
    time_t   m_lastActivityTime  = 0;
    time_t   m_displayOffTime    = 0;
    
    // EGL resources
    EGLDisplay m_eglDisplay;
    EGLConfig  m_eglConfig;
    EGLContext m_eglSharedContext;
    
    // GStreamer resources
    #ifdef USE_GSTREAMER
        GstGLDisplay* m_gstGlDisplay;
        GstGLContext* m_gstGlWrappedContext;
        GstGLContext* m_gstGlContext;
        GstContext*   m_gstContext;
    #endif // USE_GSTREAMER
    
    // Common methods
    bool initializeEGL();
    void shutdownEGL();
    #ifdef USE_GSTREAMER
        bool initializeGStreamerGL();
        void shutdownGStreamerGL();
    #endif // USE_GSTREAMER

    void setDisplayPower(bool on);
    
    #ifdef USE_X11
        Display*               m_x11Display;
        Window                 m_x11RootWindow;
        Atom                   m_wmDeleteMessage;
        std::vector<X11Output> m_x11Outputs;
        
        bool initializeX11Display(const char* title, int width, int height, int windowCount);
        void shutdownX11Display();
        bool createX11Window(const char* title, int width, int height, int windowIndex);
        void setDisplayPowerX11(bool on);
    #endif
    
    #ifdef USE_DRM
        int                      m_drmDeviceFd;
        gbm_device*              m_gbmDevice;
        std::vector<DrmOutput>   m_drmOutputs;
        std::vector<DisplayInfo> m_allDisplays;
        
        bool            initializeDrmDisplay(int width, int height);
        void            shutdownDrmDisplay();
        void            scanAllDisplays();
        void            swapDrmBuffers(int connectorIndex);
        std::string     getConnectorTypeName(uint32_t connectorType);
        drmModeModeInfo findBestDisplayMode(drmModeConnector* connector, uint16_t width, uint16_t height, uint32_t refreshRate);
        void            setDisplayPowerDrm(bool on);
    #endif
    
    friend class InputReader;
    friend class Platform;
};

// ============================================================================
// Platform - Singleton facade
// ============================================================================

class Platform
{
public:
    // Singleton
    static Platform& getInstance();
    
    // Lifecycle
    bool initialize(const char* title = "Application", int width = 1920, int height = 1080, int windowCount = 1);
    void shutdown();

    // Frame rendering
    void beginFrame(int outputIndex = 0);
    void endFrame(int outputIndex = 0);
    
    // Context management
    bool makeGLContextCurrent(int outputIndex = 0);
    void releaseGLContext();
    
    // Input
    void pollInput();
    
    #ifdef USE_X11
        void setActiveInputWindow(int outputIndex);
    #endif
    
    // Callbacks
    void setInputCallback(InputEventCallback callback);
    void setTerminalCommandCallback(TerminalCommandCallback callback);
    void setDeviceConnectionCallback(DeviceConnectionCallback callback);
    
    // Display queries
    bool          shouldClose()                                    const;
    int           getOutputCount()                                 const;
    void          getDisplaySize(int& width, int& height, int outputIndex = 0) const;
    DisplayConfig getDisplayConfig(int outputIndex = 0)            const;

    // Power management
    void setDisplayPowerTimeout(int seconds = 0, bool useDpms = false);
    void updateDisplayPower();
    void notifyUserActivity();
    bool isDisplayOn() const;
    
    // Component accessors
    InputHandler* getInputHandler() { return m_inputHandler.get(); }
    InputReader*  getInputReader()  { return m_inputReader.get(); }
    
    // GStreamer accessors
    #ifdef USE_GSTREAMER
        GstGLDisplay* getGstGLDisplay() const;
        GstGLContext* getGstGLContext() const;
        GstContext*   getGstContext()   const;
    #endif // USE_GSTREAMER

    // EGL accessors
    EGLDisplay getEGLDisplay()       const;
    EGLContext getEGLSharedContext() const;
    EGLConfig  getEGLConfig()        const;
    EGLSurface getEGLSurface(int outputIndex = 0) const;
    EGLContext getEGLContext(int outputIndex = 0) const;
    
private:
    // Singleton pattern
    Platform();
    ~Platform();
    Platform(const Platform&)            = delete;
    Platform& operator=(const Platform&) = delete;

    // Initialization logging
    void logInitializationInfo();
    
    // Members
    std::unique_ptr<DisplayManager> m_displayManager;
    std::unique_ptr<InputReader>    m_inputReader;
    std::unique_ptr<InputHandler>   m_inputHandler;
    std::vector<InputEvent>         m_eventBuffer;
    std::thread::id                 m_currentContextThread;
    mutable std::mutex              m_contextMutex;
};

} // namespace IO
} // namespace APP

#endif // IO_PLATFORM_H
