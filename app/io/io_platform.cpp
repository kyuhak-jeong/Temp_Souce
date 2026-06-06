#include "io_platform.h"
#include "logger.h"
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <ctime>

#ifdef USE_X11
    #include <X11/keysym.h>
    #include <X11/XKBlib.h>
#endif

#ifdef USE_DRM
    #include <dirent.h>
#endif

// #define IO_DEBUG 1
#ifdef IO_DEBUG
    #define LOG_IO_INPUT_EVENT(...) LOG_IO_DEBUGF(__VA_ARGS__)
#else
    #define LOG_IO_INPUT_EVENT(...) ((void)0)
#endif

namespace APP
{
namespace IO
{

// ============================================================================
// Platform-Specific Structure Implementations
// ============================================================================

#ifdef USE_X11

X11Output::X11Output()
    : nativeWindow(0), surface(EGL_NO_SURFACE), context(EGL_NO_CONTEXT)
{}

#endif // USE_X11

#ifdef USE_DRM

TouchCalibration::TouchCalibration()
    : minX(0), maxX(1920), minY(0), maxY(1080)
    , displayWidth(1920), displayHeight(1080)
    , needsMapping(false)
{}

float TouchCalibration::mapX(int32_t rawX) const
{
    if (needsMapping == false) return static_cast<float>(rawX);
    float normalized = static_cast<float>(rawX - minX) / static_cast<float>(maxX - minX);
    return normalized * displayWidth;
}

float TouchCalibration::mapY(int32_t rawY) const
{
    if (needsMapping == false) return static_cast<float>(rawY);
    float normalized = static_cast<float>(rawY - minY) / static_cast<float>(maxY - minY);
    return normalized * displayHeight;
}

InputDevice::InputDevice()
    : fd(-1), eventNumber(-1), type(DeviceType::KEYBOARD), isMTDevice(false)
{}

DrmOutput::DrmOutput()
    : connectorId(0), encoderId(0), crtcId(0), frameBufferId(0)
    , displayMode{}, gbmSurface(nullptr)
    , surface(EGL_NO_SURFACE), context(EGL_NO_CONTEXT)
    , previousBuffer(nullptr), previousFrameBuffer(0)
    , isActive(false)
{}

DisplayInfo::DisplayInfo()
    : isActive(false), width(0), height(0), refreshRate(0)
{}

#endif // USE_DRM

// ============================================================================
// InputReader Implementation
// ============================================================================

InputReader::InputReader()
    : m_deviceCount(0)
    , m_displayWidth(1920)
    , m_displayHeight(1080)
    , m_terminalInputEnabled(false)
    , m_stdinFd(-1)
    , m_terminalCallback(nullptr)
    , m_deviceConnectionCallback(nullptr)
    #ifdef USE_X11
        , m_x11Display(nullptr), m_x11Window(0), m_xi2Opcode(0)
    #endif
    #ifdef USE_DRM
        , m_framesSinceLastScan(0), m_scanningDevices(false)
        , m_mouseX(m_displayWidth / 2.0f), m_mouseY(m_displayHeight / 2.0f)
    #endif
{
    memset(&m_originalTermios, 0, sizeof(m_originalTermios));
}

InputReader::~InputReader() { shutdown(); }

bool InputReader::initialize()
{
    #ifdef USE_X11
        setupX11Input();
    #endif
    #ifdef USE_DRM
        setupDrmInput();
    #endif
    setupTerminalInput();
    return true;
}

void InputReader::shutdown()
{
    shutdownTerminalInput();

    #ifdef USE_DRM
        for (auto& dev : m_connectedDevices)
        {
            if (dev.fd >= 0) { close(dev.fd); dev.fd = -1; }
        }
        m_connectedDevices.clear();
        m_knownDevices.clear();
    #endif

    m_deviceCount              = 0;
    m_terminalCallback         = nullptr;
    m_deviceConnectionCallback = nullptr;
}

void InputReader::pollEvents(std::vector<InputEvent>& outEvents)
{
    #ifdef USE_X11
        pollX11Events(outEvents);
    #endif
    #ifdef USE_DRM
        pollDrmEvents(outEvents);
    #endif
    pollTerminalInput(outEvents);
}

void InputReader::setDisplaySize(int32_t width, int32_t height)
{
    m_displayWidth  = width;
    m_displayHeight = height;

    #ifdef USE_DRM
        m_mouseX = static_cast<float>(width)  / 2.0f;
        m_mouseY = static_cast<float>(height) / 2.0f;
        LOG_IO_DEBUGF("Display size set to %dx%d, mouse initialized to (%.1f, %.1f)", width, height, m_mouseX, m_mouseY);
    #endif
}

// ----------------------------------------------------------------------------
// InputReader - Terminal Input
// ----------------------------------------------------------------------------

void InputReader::setupTerminalInput()
{
    m_terminalInputEnabled = false;
    m_stdinFd              = STDIN_FILENO;
    m_commandBuffer.clear();

    if (isatty(m_stdinFd) == 0) { LOG_IO_DEBUG("stdin is not a terminal, terminal input disabled"); return; }

    if (tcgetattr(m_stdinFd, &m_originalTermios) != 0) { LOG_IO_ERROR("Failed to get terminal attributes"); return; }

    struct termios raw = m_originalTermios;
    raw.c_lflag    &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(m_stdinFd, TCSANOW, &raw) != 0) { LOG_IO_ERROR("Failed to set terminal to raw mode"); return; }

    int flags = fcntl(m_stdinFd, F_GETFL, 0);
    if (flags == -1 || fcntl(m_stdinFd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        LOG_IO_ERROR("Failed to set stdin to non-blocking");
        tcsetattr(m_stdinFd, TCSANOW, &m_originalTermios);
        return;
    }

    m_terminalInputEnabled = true;
    LOG_IO_SUCCESS("Terminal keyboard enabled");
}

void InputReader::shutdownTerminalInput()
{
    if (m_terminalInputEnabled == false) return;

    if (m_stdinFd >= 0 && isatty(m_stdinFd) != 0)
    {
        if (m_commandBuffer.empty() == false) std::cout << std::endl;
        tcsetattr(m_stdinFd, TCSANOW, &m_originalTermios);
        int flags = fcntl(m_stdinFd, F_GETFL, 0);
        if (flags != -1) fcntl(m_stdinFd, F_SETFL, flags & ~O_NONBLOCK);
    }

    m_terminalInputEnabled = false;
    m_commandBuffer.clear();
    LOG_IO_INFO("Terminal keyboard disabled");
}

void InputReader::pollTerminalInput(std::vector<InputEvent>& outEvents)
{
    if (m_terminalInputEnabled == false) return;

    char    buffer[256];
    ssize_t bytesRead = read(m_stdinFd, buffer, sizeof(buffer) - 1);
    if (bytesRead <= 0) return;

    for (ssize_t i = 0; i < bytesRead; ++i)
    {
        char c = buffer[i];

        if (c == '\n' || c == '\r')
        {
            std::cout << std::endl;
            if (m_commandBuffer.empty() == false)
            {
                processTerminalCommand(m_commandBuffer, outEvents);
                m_commandBuffer.clear();
            }
            std::cout << "> " << std::flush;
            continue;
        }

        if (c == 127 || c == 8)
        {
            if (m_commandBuffer.empty() == false)
            {
                m_commandBuffer.pop_back();
                std::cout << "\b \b" << std::flush;
            }
            continue;
        }

        if (c == 27)
        {
            std::cout << "\r" << std::string(m_commandBuffer.length() + 2, ' ') << "\r";
            m_commandBuffer.clear();
            std::cout << "> " << std::flush;
            continue;
        }

        if (std::isprint(c) != 0)
        {
            m_commandBuffer.push_back(c);
            std::cout << c << std::flush;
        }
    }
}

void InputReader::processTerminalCommand(const std::string& command, std::vector<InputEvent>& outEvents)
{
    if (command.empty() == true) return;

    size_t start = command.find_first_not_of(" \t");
    size_t end   = command.find_last_not_of(" \t");
    if (start == std::string::npos) return;

    std::string cmd = command.substr(start, end - start + 1);

    if (m_terminalCallback != nullptr) { m_terminalCallback(cmd); return; }

    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    for (char c : cmd)
    {
        KeyCode keyCode = translateTerminalKey(c);
        if (keyCode == KeyCode::UNKNOWN) continue;
        KeyEvent keyEvent;
        keyEvent.keyCode   = keyCode;
        keyEvent.action    = KeyAction::DOWN;
        keyEvent.source    = Source::KEYBOARD;
        keyEvent.deviceId  = -2;
        keyEvent.eventTime = 0;
        outEvents.push_back(InputEvent::createKeyEvent(keyEvent));
    }
}

KeyCode InputReader::translateTerminalKey(char c)
{
    if (c >= 'a' && c <= 'z') return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + (c - 'a'));
    if (c >= 'A' && c <= 'Z') return static_cast<KeyCode>(static_cast<int>(KeyCode::A) + (c - 'A'));
    if (c >= '0' && c <= '9') return static_cast<KeyCode>(static_cast<int>(KeyCode::NUM_0) + (c - '0'));

    switch (c)
    {
        case '\n': case '\r': return KeyCode::ENTER;
        case ' ':             return KeyCode::SPACE;
        case 127: case 8:     return KeyCode::BACK;
        case 27:              return KeyCode::ESCAPE;
        case '[':             return KeyCode::LEFT_BRACKET;
        case ']':             return KeyCode::RIGHT_BRACKET;
        case '-':             return KeyCode::MINUS;
        case '=':             return KeyCode::EQUAL;
        case ',':             return KeyCode::COMMA;
        case '.':             return KeyCode::PERIOD;
        default:              return KeyCode::UNKNOWN;
    }
}

// ----------------------------------------------------------------------------
// InputReader - X11 Input
// ----------------------------------------------------------------------------

#ifdef USE_X11

void InputReader::setX11Window(Display* display, Window window)
{
    m_x11Display = display;
    m_x11Window  = window;
    LOG_IO_DEBUGF("InputReader: X11 window set (Window ID: %lu)", window);
}

void InputReader::setupX11Input()
{
    if (m_x11Display == nullptr) return;

    int event = 0, error = 0;
    if (XQueryExtension(m_x11Display, "XInputExtension", &m_xi2Opcode, &event, &error) == 0)
    {
        LOG_IO_ERROR("X Input extension not available");
        return;
    }

    int major = 2, minor = 2;
    if (XIQueryVersion(m_x11Display, &major, &minor) != Success) { LOG_IO_ERROR("XI2 not available"); return; }

    XIEventMask eventMask;
    unsigned char mask[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask, XI_KeyPress);
    XISetMask(mask, XI_KeyRelease);
    XISetMask(mask, XI_ButtonPress);
    XISetMask(mask, XI_ButtonRelease);
    XISetMask(mask, XI_Motion);
    XISetMask(mask, XI_Enter);
    XISetMask(mask, XI_Leave);
    XISetMask(mask, XI_TouchBegin);
    XISetMask(mask, XI_TouchUpdate);
    XISetMask(mask, XI_TouchEnd);

    eventMask.deviceid = XIAllMasterDevices;
    eventMask.mask_len = sizeof(mask);
    eventMask.mask     = mask;
    XISelectEvents(m_x11Display, m_x11Window, &eventMask, 1);
    XFlush(m_x11Display);

    m_deviceCount = 2;
    LOG_IO_SUCCESS("X11 input initialized");
}

void InputReader::pollX11Events(std::vector<InputEvent>& outEvents)
{
    if (m_x11Display == nullptr) return;

    while (XPending(m_x11Display) > 0)
    {
        XEvent xev;
        XNextEvent(m_x11Display, &xev);

        if (xev.xcookie.type != GenericEvent || xev.xcookie.extension != m_xi2Opcode) continue;
        if (XGetEventData(m_x11Display, &xev.xcookie) == 0) continue;

        XIDeviceEvent* xiEvent = (XIDeviceEvent*)xev.xcookie.data;

        switch (xev.xcookie.evtype)
        {
            case XI_KeyPress:
            case XI_KeyRelease:
            {
                KeySym  keySym  = XkbKeycodeToKeysym(m_x11Display, xiEvent->detail, 0, 0);
                KeyCode keyCode = translateX11KeyCode(keySym);
                if (keyCode != KeyCode::UNKNOWN)
                {
                    const int     iKey  = static_cast<int>(keyCode);
                    const int64_t evMs  = static_cast<int64_t>(xiEvent->time); // milliseconds

                    KeyEvent keyEvent;
                    keyEvent.keyCode   = keyCode;
                    keyEvent.source    = Source::KEYBOARD;
                    keyEvent.deviceId  = xiEvent->deviceid;
                    keyEvent.eventTime = xiEvent->time;

                    if (xev.xcookie.evtype == XI_KeyPress)
                    {
                        auto it = m_x11KeyDownTime.find(iKey);
                        if (it == m_x11KeyDownTime.end())
                        {
                            // First press — genuine DOWN
                            m_x11KeyDownTime[iKey] = evMs;
                            keyEvent.action   = KeyAction::DOWN;
                            keyEvent.holdTime = 0.0f;
                        }
                        else
                        {
                            // Key already tracked → X11 auto-repeat KeyPress, emit as MULTIPLE
                            keyEvent.action   = KeyAction::MULTIPLE;
                            keyEvent.holdTime = static_cast<float>(evMs - it->second) / 1000.0f;
                        }
                    }
                    else // XI_KeyRelease
                    {
                        m_x11KeyDownTime.erase(iKey);
                        keyEvent.action   = KeyAction::UP;
                        keyEvent.holdTime = 0.0f;
                    }

                    outEvents.push_back(InputEvent::createKeyEvent(keyEvent));
                }
                break;
            }

            case XI_ButtonPress:
            case XI_ButtonRelease:
            {
                MotionEvent me;
                me.action   = (xev.xcookie.evtype == XI_ButtonPress) ? MotionAction::DOWN : MotionAction::UP;
                me.source   = Source::MOUSE;
                me.deviceId = xiEvent->deviceid;
                me.eventTime = xiEvent->time;
                me.x        = xiEvent->event_x;
                me.y        = xiEvent->event_y;

                if      (xiEvent->detail == 1) me.button = MouseButton::LEFT;
                else if (xiEvent->detail == 3) me.button = MouseButton::RIGHT;
                else if (xiEvent->detail == 2) me.button = MouseButton::MIDDLE;
                else if (xiEvent->detail == 4 || xiEvent->detail == 5)
                {
                    me.action  = MotionAction::SCROLL;
                    me.scrollY = (xiEvent->detail == 4) ? 1.0f : -1.0f;
                }
                outEvents.push_back(InputEvent::createMotionEvent(me));
                break;
            }

            case XI_Motion:
            {
                MotionEvent me;
                me.action    = MotionAction::MOVE;
                me.source    = Source::MOUSE;
                me.deviceId  = xiEvent->deviceid;
                me.eventTime = xiEvent->time;
                me.x         = xiEvent->event_x;
                me.y         = xiEvent->event_y;
                outEvents.push_back(InputEvent::createMotionEvent(me));
                break;
            }

            case XI_Enter:
            case XI_Leave:
            {
                MotionEvent me;
                me.action = (xev.xcookie.evtype == XI_Enter) ? MotionAction::HOVER_ENTER : MotionAction::HOVER_EXIT;
                me.source = Source::MOUSE;
                me.x      = xiEvent->event_x;
                me.y      = xiEvent->event_y;
                outEvents.push_back(InputEvent::createMotionEvent(me));
                break;
            }

            case XI_TouchBegin:
            case XI_TouchUpdate:
            case XI_TouchEnd:
            {
                MotionEvent me;
                me.source             = Source::TOUCHSCREEN;
                me.deviceId           = xiEvent->deviceid;
                me.eventTime          = xiEvent->time;
                me.x                  = static_cast<float>(xiEvent->event_x);
                me.y                  = static_cast<float>(xiEvent->event_y);
                me.button             = MouseButton::LEFT;
                me.pointerCount       = 1;
                me.actionPointerIndex = 0;
                me.pointers[0].id     = xiEvent->detail;
                me.pointers[0].x      = me.x;
                me.pointers[0].y      = me.y;

                if      (xev.xcookie.evtype == XI_TouchBegin)  me.action = MotionAction::DOWN;
                else if (xev.xcookie.evtype == XI_TouchUpdate) me.action = MotionAction::MOVE;
                else                                           me.action = MotionAction::UP;

                outEvents.push_back(InputEvent::createMotionEvent(me));
                break;
            }
        }

        XFreeEventData(m_x11Display, &xev.xcookie);
    }
}

KeyCode InputReader::translateX11KeyCode(int x11KeySym)
{
    switch (x11KeySym)
    {
        case XK_Up:    return KeyCode::DPAD_UP;
        case XK_Down:  return KeyCode::DPAD_DOWN;
        case XK_Left:  return KeyCode::DPAD_LEFT;
        case XK_Right: return KeyCode::DPAD_RIGHT;

        case XK_Return:    return KeyCode::ENTER;
        case XK_Escape:    return KeyCode::ESCAPE;
        case XK_space:     return KeyCode::SPACE;
        case XK_BackSpace: return KeyCode::BACKSPACE;
        case XK_Menu:      return KeyCode::MENU;
        case XK_Home:      return KeyCode::HOME;

        case XK_a: case XK_A: return KeyCode::A;
        case XK_b: case XK_B: return KeyCode::B;
        case XK_c: case XK_C: return KeyCode::C;
        case XK_d: case XK_D: return KeyCode::D;
        case XK_e: case XK_E: return KeyCode::E;
        case XK_f: case XK_F: return KeyCode::F;
        case XK_g: case XK_G: return KeyCode::G;
        case XK_h: case XK_H: return KeyCode::H;
        case XK_i: case XK_I: return KeyCode::I;
        case XK_j: case XK_J: return KeyCode::J;
        case XK_k: case XK_K: return KeyCode::K;
        case XK_l: case XK_L: return KeyCode::L;
        case XK_m: case XK_M: return KeyCode::M;
        case XK_n: case XK_N: return KeyCode::N;
        case XK_o: case XK_O: return KeyCode::O;
        case XK_p: case XK_P: return KeyCode::P;
        case XK_q: case XK_Q: return KeyCode::Q;
        case XK_r: case XK_R: return KeyCode::R;
        case XK_s: case XK_S: return KeyCode::S;
        case XK_t: case XK_T: return KeyCode::T;
        case XK_u: case XK_U: return KeyCode::U;
        case XK_v: case XK_V: return KeyCode::V;
        case XK_w: case XK_W: return KeyCode::W;
        case XK_x: case XK_X: return KeyCode::X;
        case XK_y: case XK_Y: return KeyCode::Y;
        case XK_z: case XK_Z: return KeyCode::Z;

        case XK_0: return KeyCode::NUM_0; case XK_1: return KeyCode::NUM_1; case XK_2: return KeyCode::NUM_2;
        case XK_3: return KeyCode::NUM_3; case XK_4: return KeyCode::NUM_4; case XK_5: return KeyCode::NUM_5;
        case XK_6: return KeyCode::NUM_6; case XK_7: return KeyCode::NUM_7; case XK_8: return KeyCode::NUM_8;
        case XK_9: return KeyCode::NUM_9;

        case XK_bracketleft:  return KeyCode::LEFT_BRACKET;
        case XK_bracketright: return KeyCode::RIGHT_BRACKET;
        case XK_minus:        return KeyCode::MINUS;
        case XK_equal:        return KeyCode::EQUAL;
        case XK_comma:        return KeyCode::COMMA;
        case XK_period:       return KeyCode::PERIOD;

        default: return KeyCode::UNKNOWN;
    }
}

#endif // USE_X11

// ----------------------------------------------------------------------------
// InputReader - DRM Input
// ----------------------------------------------------------------------------

#ifdef USE_DRM

#define NBITS(x)             ((((x)-1)/BITS_PER_LONG)+1)
#define BITS_PER_LONG        (sizeof(long) * 8)
#define test_bit(bit, array) ((array[bit/BITS_PER_LONG] >> (bit%BITS_PER_LONG)) & 1)

struct InputDeviceInfo
{
    std::string path, name;
    int         eventNumber, score;
    DeviceType  type;
    bool        isMTDevice;

    InputDeviceInfo() : eventNumber(-1), score(0), type(DeviceType::KEYBOARD), isMTDevice(false) {}
};

static constexpr int32_t kRawUninitialized = INT32_MIN;

struct TouchSlot
{
    int32_t trackingId = -1;
    int32_t rawX       = 0;
    int32_t rawY       = 0;
};

struct TouchDeviceState
{
    static constexpr int MAX_SLOTS = MAX_TOUCH_POINTS;

    TouchSlot slots[MAX_SLOTS];
    int32_t   currentSlot = 0;
    TouchSlot prevSlots[MAX_SLOTS];

    int activeCount() const
    {
        int n = 0;
        for (const auto& s : slots)
            if (s.trackingId >= 0 && s.rawX != kRawUninitialized && s.rawY != kRawUninitialized) ++n;
        return n;
    }

    int prevActiveCount() const
    {
        int n = 0;
        for (const auto& s : prevSlots) n += (s.trackingId >= 0) ? 1 : 0;
        return n;
    }

    int fillPointers(MotionEvent& ev, const TouchCalibration& cal) const
    {
        struct Entry { int32_t trackingId; float x, y; };
        Entry entries[MAX_SLOTS];
        int   n = 0;
        for (int i = 0; i < MAX_SLOTS; ++i)
        {
            if (slots[i].trackingId < 0) continue;
            if (slots[i].rawX == kRawUninitialized || slots[i].rawY == kRawUninitialized) continue;
            entries[n++] = { slots[i].trackingId, cal.mapX(slots[i].rawX), cal.mapY(slots[i].rawY) };
        }
        for (int i = 0; i < n - 1; ++i)
            for (int j = i + 1; j < n; ++j)
                if (entries[j].trackingId < entries[i].trackingId) std::swap(entries[i], entries[j]);

        ev.pointerCount = n;
        for (int i = 0; i < n; ++i) { ev.pointers[i].id = entries[i].trackingId; ev.pointers[i].x = entries[i].x; ev.pointers[i].y = entries[i].y; }
        return 0;
    }

    void snapshotToPrev() { memcpy(prevSlots, slots, sizeof(slots)); }
};

static std::map<int, TouchDeviceState> g_touchState;

void InputReader::setupDrmInput()
{
    m_framesSinceLastScan = 0;
    m_scanningDevices     = false;
    m_mouseX              = static_cast<float>(m_displayWidth)  / 2.0f;
    m_mouseY              = static_cast<float>(m_displayHeight) / 2.0f;
    scanAndOpenDevices();
    LOG_IO_SUCCESSF("DRM input initialized (%d devices)", m_deviceCount);
}

void InputReader::pollDrmEvents(std::vector<InputEvent>& outEvents)
{
    if (++m_framesSinceLastScan >= 60) { scanAndOpenDevices(); m_framesSinceLastScan = 0; }
    if (m_connectedDevices.empty() == true) return;

    for (auto& dev : m_connectedDevices)
    {
        struct input_event ev;
        ssize_t    bytesRead = 0;
        float      mouseAccX = 0.0f, mouseAccY = 0.0f;
        TouchDeviceState& ts = g_touchState[dev.eventNumber];

        while ((bytesRead = read(dev.fd, &ev, sizeof(ev))) == sizeof(ev))
        {
            int64_t evTime = ev.time.tv_sec * 1000 + ev.time.tv_usec / 1000;

            if (dev.type == DeviceType::TOUCHSCREEN)
            {
                if (ev.type == EV_ABS)
                {
                    int s = ts.currentSlot;
                    if (ev.code == ABS_MT_SLOT)
                        ts.currentSlot = std::clamp(ev.value, 0, TouchDeviceState::MAX_SLOTS - 1);
                    else if (ev.code == ABS_MT_TRACKING_ID)
                    {
                        ts.slots[s].trackingId = ev.value;
                        if (ev.value >= 0) { ts.slots[s].rawX = kRawUninitialized; ts.slots[s].rawY = kRawUninitialized; }
                    }
                    else if (ev.code == ABS_MT_POSITION_X || (ev.code == ABS_X && dev.isMTDevice == false))
                        ts.slots[s].rawX = ev.value;
                    else if (ev.code == ABS_MT_POSITION_Y || (ev.code == ABS_Y && dev.isMTDevice == false))
                        ts.slots[s].rawY = ev.value;
                }
                else if (ev.type == EV_SYN && ev.code == SYN_REPORT)
                {
                    int prevCount = ts.prevActiveCount();
                    int nowCount  = ts.activeCount();

                    if (prevCount == 0 && nowCount == 0) { ts.snapshotToPrev(); continue; }

                    auto makeBase = [&](bool useCurrentSlots) -> MotionEvent
                    {
                        MotionEvent me;
                        me.source    = Source::TOUCHSCREEN;
                        me.deviceId  = dev.eventNumber;
                        me.eventTime = evTime;
                        me.button    = MouseButton::LEFT;

                        if (useCurrentSlots == true)
                        {
                            int pi = ts.fillPointers(me, dev.touchCalibration);
                            me.x   = (me.pointerCount > 0) ? me.pointers[pi].x : 0.0f;
                            me.y   = (me.pointerCount > 0) ? me.pointers[pi].y : 0.0f;
                        }
                        else
                        {
                            me.pointerCount = 0;
                            for (int i = 0; i < TouchDeviceState::MAX_SLOTS; ++i)
                            {
                                if (ts.prevSlots[i].trackingId < 0) continue;
                                int idx             = me.pointerCount++;
                                me.pointers[idx].id = ts.prevSlots[i].trackingId;
                                me.pointers[idx].x  = dev.touchCalibration.mapX(ts.prevSlots[i].rawX);
                                me.pointers[idx].y  = dev.touchCalibration.mapY(ts.prevSlots[i].rawY);
                            }
                            if (me.pointerCount > 0) { me.x = me.pointers[0].x; me.y = me.pointers[0].y; }
                        }
                        return me;
                    };

                    MotionEvent me;
                    if      (prevCount == 0 && nowCount > 0) { me = makeBase(true);  me.action = MotionAction::DOWN;         LOG_IO_INPUT_EVENT("Touch DOWN count=%d pos=(%.1f,%.1f)", nowCount, me.x, me.y); }
                    else if (nowCount == 0)                  { me = makeBase(false); me.action = MotionAction::UP;           LOG_IO_INPUT_EVENT("Touch UP count was=%d pos=(%.1f,%.1f)", prevCount, me.x, me.y); }
                    else if (nowCount > prevCount)           { me = makeBase(true);  me.action = MotionAction::POINTER_DOWN; LOG_IO_INPUT_EVENT("Touch POINTER_DOWN count=%d", nowCount); }
                    else if (nowCount < prevCount)           { me = makeBase(true);  me.action = MotionAction::POINTER_UP;   LOG_IO_INPUT_EVENT("Touch POINTER_UP count=%d", nowCount); }
                    else                                     { me = makeBase(true);  me.action = MotionAction::MOVE;         LOG_IO_INPUT_EVENT("Touch MOVE count=%d pos=(%.1f,%.1f)", nowCount, me.x, me.y); }

                    outEvents.push_back(InputEvent::createMotionEvent(me));
                    ts.snapshotToPrev();
                }
            }
            else if (ev.type == EV_KEY)
            {
                if (ev.code == BTN_LEFT || ev.code == BTN_RIGHT || ev.code == BTN_MIDDLE)
                {
                    MotionEvent me;
                    me.action    = (ev.value == 1) ? MotionAction::DOWN : MotionAction::UP;
                    me.source    = Source::MOUSE;
                    me.deviceId  = dev.eventNumber;
                    me.eventTime = evTime;
                    me.x         = m_mouseX;
                    me.y         = m_mouseY;
                    if      (ev.code == BTN_LEFT)   me.button = MouseButton::LEFT;
                    else if (ev.code == BTN_RIGHT)  me.button = MouseButton::RIGHT;
                    else                            me.button = MouseButton::MIDDLE;
                    LOG_IO_INPUT_EVENT("Mouse button %d %s at (%.1f, %.1f)", static_cast<int>(me.button), (ev.value == 1) ? "DOWN" : "UP", m_mouseX, m_mouseY);
                    outEvents.push_back(InputEvent::createMotionEvent(me));
                }
                else
                {
                    KeyCode keyCode = translateLinuxKeyCode(ev.code);
                    if (keyCode != KeyCode::UNKNOWN)
                    {
                        KeyEvent ke;
                        ke.keyCode  = keyCode;
                        ke.action   = (ev.value == 1) ? KeyAction::DOWN
                                    : (ev.value == 2) ? KeyAction::MULTIPLE
                                                      : KeyAction::UP;
                        ke.source   = Source::KEYBOARD;
                        ke.deviceId = dev.eventNumber;
                        ke.eventTime = evTime;

                        if (ke.action == KeyAction::DOWN)
                        {
                            m_keyDownTime[static_cast<int>(keyCode)] = evTime;
                            ke.holdTime = 0.0f;
                        }
                        else if (ke.action == KeyAction::MULTIPLE)
                        {
                            auto it = m_keyDownTime.find(static_cast<int>(keyCode));
                            ke.holdTime = (it != m_keyDownTime.end())
                                        ? static_cast<float>(evTime - it->second) / 1000.0f
                                        : 0.0f;
                        }
                        else // UP
                        {
                            m_keyDownTime.erase(static_cast<int>(keyCode));
                            ke.holdTime = 0.0f;
                        }

                        LOG_IO_INPUT_EVENT("Key %s: code=%d holdTime=%.2fs",
                            (ke.action == KeyAction::DOWN)     ? "DOWN"
                          : (ke.action == KeyAction::MULTIPLE) ? "REPEAT"
                                                               : "UP",
                            static_cast<int>(keyCode), ke.holdTime);
                        outEvents.push_back(InputEvent::createKeyEvent(ke));
                    }
                }
            }
            else if (ev.type == EV_REL && dev.type == DeviceType::MOUSE)
            {
                if      (ev.code == REL_X)    mouseAccX += static_cast<float>(ev.value);
                else if (ev.code == REL_Y)    mouseAccY += static_cast<float>(ev.value);
                else if (ev.code == REL_WHEEL || ev.code == REL_HWHEEL)
                {
                    MotionEvent me;
                    me.action    = MotionAction::SCROLL;
                    me.source    = Source::MOUSE;
                    me.deviceId  = dev.eventNumber;
                    me.eventTime = evTime;
                    me.x         = m_mouseX;
                    me.y         = m_mouseY;
                    if (ev.code == REL_WHEEL) me.scrollY = static_cast<float>(ev.value);
                    else                      me.scrollX = static_cast<float>(ev.value);
                    LOG_IO_INPUT_EVENT("Mouse scroll %.1f/%.1f at (%.1f, %.1f)", me.scrollX, me.scrollY, me.x, me.y);
                    outEvents.push_back(InputEvent::createMotionEvent(me));
                }
            }
            else if (ev.type == EV_SYN && ev.code == SYN_REPORT && dev.type == DeviceType::MOUSE)
            {
                if (std::abs(mouseAccX) > 0.01f || std::abs(mouseAccY) > 0.01f)
                {
                    m_mouseX = std::clamp(m_mouseX + mouseAccX, 0.0f, static_cast<float>(m_displayWidth));
                    m_mouseY = std::clamp(m_mouseY + mouseAccY, 0.0f, static_cast<float>(m_displayHeight));
                    MotionEvent me;
                    me.action    = MotionAction::MOVE;
                    me.source    = Source::MOUSE;
                    me.deviceId  = dev.eventNumber;
                    me.eventTime = evTime;
                    me.x         = m_mouseX;
                    me.y         = m_mouseY;
                    LOG_IO_INPUT_EVENT("Mouse MOVE to (%.1f, %.1f) [delta: %.1f, %.1f]", me.x, me.y, mouseAccX, mouseAccY);
                    outEvents.push_back(InputEvent::createMotionEvent(me));
                    mouseAccX = mouseAccY = 0.0f;
                }
            }
        }

        if (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            logDeviceDisconnected(dev);
            g_touchState.erase(dev.eventNumber);
            close(dev.fd);
            dev.fd = -1;
        }
    }

    m_connectedDevices.erase(
        std::remove_if(m_connectedDevices.begin(), m_connectedDevices.end(),
                       [](const InputDevice& d) { return d.fd == -1; }),
        m_connectedDevices.end());

    m_deviceCount = static_cast<int>(m_connectedDevices.size());
}

void InputReader::scanAndOpenDevices()
{
    if (m_scanningDevices == true) return;
    m_scanningDevices = true;

    std::vector<InputDeviceInfo> scannedInfo;
    DIR* dir = opendir("/dev/input");
    if (dir == nullptr)
    {
        LOG_IO_ERROR("Failed to open /dev/input");
        m_scanningDevices = false;
        return;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;

        int         eventNum   = atoi(entry->d_name + 5);
        std::string devicePath = std::string("/dev/input/") + entry->d_name;

        bool alreadyOpen = false;
        for (const auto& dev : m_connectedDevices)
        {
            if (dev.eventNumber == eventNum) { alreadyOpen = true; break; }
        }
        if (alreadyOpen == true) continue;

        int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        char deviceName[256] = {0};
        if (ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName) < 0) { close(fd); continue; }

        unsigned long evBits[NBITS(EV_MAX)]   = {0};
        unsigned long absBits[NBITS(ABS_MAX)] = {0};
        ioctl(fd, EVIOCGBIT(0, sizeof(evBits)), evBits);
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits);
        close(fd);

        InputDeviceInfo info;
        info.path        = devicePath;
        info.name        = std::string(deviceName);
        info.eventNumber = eventNum;
        info.score       = 0;

        std::string nameLower = info.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        bool hasMTPositionX = test_bit(ABS_MT_POSITION_X, absBits);
        bool hasMTPositionY = test_bit(ABS_MT_POSITION_Y, absBits);
        bool hasAbsX        = test_bit(ABS_X, absBits);
        bool hasAbsY        = test_bit(ABS_Y, absBits);
        info.isMTDevice     = (hasMTPositionX == true && hasMTPositionY == true);

        if ((hasMTPositionX == true && hasMTPositionY == true) ||
            (nameLower.find("touch") != std::string::npos && hasAbsX == true && hasAbsY == true))
        {
            info.type   = DeviceType::TOUCHSCREEN;
            info.score += 100;
        }
        else if (nameLower.find("mouse") != std::string::npos)    { info.type = DeviceType::MOUSE;    info.score += 90; }
        else if (nameLower.find("keyboard") != std::string::npos) { info.type = DeviceType::KEYBOARD; info.score += 80; }
        else                                                       { info.type = DeviceType::KEYBOARD; info.score += 10; }

        if (nameLower.find("logitech") != std::string::npos ||
            nameLower.find("microsoft") != std::string::npos ||
            nameLower.find("razer") != std::string::npos) info.score += 100;

        if (nameLower.find("hid") != std::string::npos ||
            nameLower.find("remote") != std::string::npos ||
            nameLower.find("consumer") != std::string::npos) info.score += 60;

        if (nameLower.find("usb") != std::string::npos) info.score += 20;

        if (nameLower.find("power")    != std::string::npos || nameLower.find("pwrkey")   != std::string::npos ||
            nameLower.find("headset")  != std::string::npos || nameLower.find("hdmi")     != std::string::npos ||
            nameLower.find("adc-keys") != std::string::npos || nameLower.find("rockchip") != std::string::npos ||
            nameLower.find(".pwm")     != std::string::npos) info.score -= 100;

        if (info.score >= 0) scannedInfo.push_back(info);
    }
    closedir(dir);

    if (scannedInfo.empty() == true)
    {
        if (m_connectedDevices.empty() == true) LOG_IO_WARNING("No suitable input devices found");
        m_scanningDevices = false;
        return;
    }

    std::sort(scannedInfo.begin(), scannedInfo.end(),
              [](const InputDeviceInfo& a, const InputDeviceInfo& b) { return a.score > b.score; });

    int newDeviceCount = 0;
    for (const auto& devInfo : scannedInfo)
    {
        int fd = open(devInfo.path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        InputDevice dev;
        dev.fd          = fd;
        dev.eventNumber = devInfo.eventNumber;
        dev.type        = devInfo.type;
        dev.name        = devInfo.name;
        dev.isMTDevice  = devInfo.isMTDevice;

        if (dev.type == DeviceType::TOUCHSCREEN) calibrateTouchscreen(dev);

        m_connectedDevices.push_back(dev);
        m_knownDevices[dev.eventNumber] = std::make_pair(dev.name, dev.type);
        newDeviceCount++;
        logDeviceConnected(dev);
    }

    if (newDeviceCount > 0) m_deviceCount = static_cast<int>(m_connectedDevices.size());
    m_scanningDevices = false;
}

void InputReader::checkDeviceHealth()
{
    for (auto it = m_connectedDevices.begin(); it != m_connectedDevices.end();)
    {
        struct input_event ev;
        ssize_t result = read(it->fd, &ev, sizeof(ev));
        if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        {
            logDeviceDisconnected(*it);
            close(it->fd);
            it->fd = -1;
            it     = m_connectedDevices.erase(it);
        }
        else ++it;
    }
    m_deviceCount = static_cast<int>(m_connectedDevices.size());
}

void InputReader::notifyExistingDevices()
{
    if (m_deviceConnectionCallback == nullptr) return;
    for (const auto& dev : m_connectedDevices)
        m_deviceConnectionCallback(dev.name, dev.eventNumber, dev.type, true);
    if (m_connectedDevices.empty() == false)
        LOG_IO_SUCCESSF("Notified callback about %d existing device(s)", static_cast<int>(m_connectedDevices.size()));
}

void InputReader::calibrateTouchscreen(InputDevice& device)
{
    device.touchCalibration.displayWidth  = m_displayWidth;
    device.touchCalibration.displayHeight = m_displayHeight;

    struct input_absinfo absInfo;

    if (ioctl(device.fd, EVIOCGABS(ABS_MT_POSITION_X), &absInfo) >= 0 ||
        ioctl(device.fd, EVIOCGABS(ABS_X), &absInfo) >= 0)
    {
        device.touchCalibration.minX        = absInfo.minimum;
        device.touchCalibration.maxX        = absInfo.maximum;
        device.touchCalibration.needsMapping = true;
    }

    if (ioctl(device.fd, EVIOCGABS(ABS_MT_POSITION_Y), &absInfo) >= 0 ||
        ioctl(device.fd, EVIOCGABS(ABS_Y), &absInfo) >= 0)
    {
        device.touchCalibration.minY        = absInfo.minimum;
        device.touchCalibration.maxY        = absInfo.maximum;
        device.touchCalibration.needsMapping = true;
    }
}

void InputReader::logDeviceConnected(const InputDevice& dev)
{
    char        typeIcon  = 'U';
    const char* typeColor = LOG::Color::WHITE;

    if      (dev.type == DeviceType::KEYBOARD)    { typeIcon = 'K'; typeColor = LOG::Color::CYAN;    }
    else if (dev.type == DeviceType::MOUSE)       { typeIcon = 'M'; typeColor = LOG::Color::YELLOW;  }
    else if (dev.type == DeviceType::TOUCHSCREEN) { typeIcon = 'T'; typeColor = LOG::Color::MAGENTA; }
    else if (dev.type == DeviceType::DPAD)        { typeIcon = 'D'; typeColor = LOG::Color::GREEN;   }

    LOG_IO_SUCCESSF("Device connected: %s[%c]%s event%d - %s", typeColor, typeIcon, LOG::Color::RESET, dev.eventNumber, dev.name.c_str());

    if (m_deviceConnectionCallback != nullptr)
        m_deviceConnectionCallback(dev.name, dev.eventNumber, dev.type, true);

    if (dev.type == DeviceType::TOUCHSCREEN && dev.touchCalibration.needsMapping == true)
        LOG_IO_INFOF("  Touch range: %d-%d x %d-%d -> %dx%d",
                     dev.touchCalibration.minX, dev.touchCalibration.maxX,
                     dev.touchCalibration.minY, dev.touchCalibration.maxY,
                     m_displayWidth, m_displayHeight);
}

void InputReader::logDeviceDisconnected(const InputDevice& dev)
{
    LOG_IO_WARNINGF("Device disconnected: event%d - %s", dev.eventNumber, dev.name.c_str());
    if (m_deviceConnectionCallback != nullptr)
        m_deviceConnectionCallback(dev.name, dev.eventNumber, dev.type, false);
}

KeyCode InputReader::translateLinuxKeyCode(int linuxKeyCode)
{
    switch (linuxKeyCode)
    {
        case KEY_UP:    return KeyCode::DPAD_UP;
        case KEY_DOWN:  return KeyCode::DPAD_DOWN;
        case KEY_LEFT:  return KeyCode::DPAD_LEFT;
        case KEY_RIGHT: return KeyCode::DPAD_RIGHT;

        case KEY_ENTER:     return KeyCode::ENTER;
        case KEY_ESC:       return KeyCode::ESCAPE;
        case KEY_SPACE:     return KeyCode::SPACE;
        case KEY_BACKSPACE: return KeyCode::BACKSPACE;
        case KEY_COMPOSE:   return KeyCode::MENU;
        case KEY_MENU:      return KeyCode::MENU;
        case KEY_HOME:      return KeyCode::HOME;

        case KEY_A: return KeyCode::A; case KEY_B: return KeyCode::B; case KEY_C: return KeyCode::C;
        case KEY_D: return KeyCode::D; case KEY_E: return KeyCode::E; case KEY_F: return KeyCode::F;
        case KEY_G: return KeyCode::G; case KEY_H: return KeyCode::H; case KEY_I: return KeyCode::I;
        case KEY_J: return KeyCode::J; case KEY_K: return KeyCode::K; case KEY_L: return KeyCode::L;
        case KEY_M: return KeyCode::M; case KEY_N: return KeyCode::N; case KEY_O: return KeyCode::O;
        case KEY_P: return KeyCode::P; case KEY_Q: return KeyCode::Q; case KEY_R: return KeyCode::R;
        case KEY_S: return KeyCode::S; case KEY_T: return KeyCode::T; case KEY_U: return KeyCode::U;
        case KEY_V: return KeyCode::V; case KEY_W: return KeyCode::W; case KEY_X: return KeyCode::X;
        case KEY_Y: return KeyCode::Y; case KEY_Z: return KeyCode::Z;

        case KEY_0: return KeyCode::NUM_0; case KEY_1: return KeyCode::NUM_1; case KEY_2: return KeyCode::NUM_2;
        case KEY_3: return KeyCode::NUM_3; case KEY_4: return KeyCode::NUM_4; case KEY_5: return KeyCode::NUM_5;
        case KEY_6: return KeyCode::NUM_6; case KEY_7: return KeyCode::NUM_7; case KEY_8: return KeyCode::NUM_8;
        case KEY_9: return KeyCode::NUM_9;

        case KEY_LEFTBRACE:  return KeyCode::LEFT_BRACKET;
        case KEY_RIGHTBRACE: return KeyCode::RIGHT_BRACKET;
        case KEY_MINUS:      return KeyCode::MINUS;
        case KEY_EQUAL:      return KeyCode::EQUAL;
        case KEY_COMMA:      return KeyCode::COMMA;
        case KEY_DOT:        return KeyCode::PERIOD;

        case KEY_VOLUMEUP:     return KeyCode::VOLUME_UP;
        case KEY_VOLUMEDOWN:   return KeyCode::VOLUME_DOWN;
        case KEY_MUTE:         return KeyCode::VOLUME_MUTE;
        case KEY_CHANNELUP:    return KeyCode::CHANNEL_UP;
        case KEY_CHANNELDOWN:  return KeyCode::CHANNEL_DOWN;
        case KEY_BACK:         return KeyCode::BACK;
        case KEY_HOMEPAGE:     return KeyCode::HOME;
        case KEY_NEXTSONG:     return KeyCode::MEDIA_NEXT;
        case KEY_PREVIOUSSONG: return KeyCode::MEDIA_PREVIOUS;

        default: return KeyCode::UNKNOWN;
    }
}

#endif // USE_DRM

// ============================================================================
// InputState Implementation
// ============================================================================

void InputState::updateFromKeyEvent(const KeyEvent& event)
{
    lastSource = event.source;
}

void InputState::updateFromMotionEvent(const MotionEvent& event)
{
    cursorX    = event.x;
    cursorY    = event.y;
    lastSource = event.source;

    if (event.isMouse() == true)
    {
        if      (event.action == MotionAction::DOWN) { if (event.button == MouseButton::LEFT) mouseLeft = true;  if (event.button == MouseButton::RIGHT) mouseRight = true;  if (event.button == MouseButton::MIDDLE) mouseMiddle = true; }
        else if (event.action == MotionAction::UP)   { if (event.button == MouseButton::LEFT) mouseLeft = false; if (event.button == MouseButton::RIGHT) mouseRight = false; if (event.button == MouseButton::MIDDLE) mouseMiddle = false; }
    }
    else if (event.isTouch() == true)
    {
        touchCount = (event.action == MotionAction::UP || event.action == MotionAction::CANCEL) ? 0 : event.pointerCount;
    }
}

bool InputState::isMouseButtonDown(MouseButton button) const
{
    if (button == MouseButton::LEFT)   return mouseLeft;
    if (button == MouseButton::RIGHT)  return mouseRight;
    if (button == MouseButton::MIDDLE) return mouseMiddle;
    return false;
}

// ============================================================================
// InputHandler Implementation
// ============================================================================

InputHandler::InputHandler() : m_callback(nullptr), m_state() {}
InputHandler::~InputHandler() {}

void InputHandler::handleEvent(const InputEvent& event)
{
    if      (event.type == InputEvent::Type::MOTION) m_state.updateFromMotionEvent(event.motion);
    else if (event.type == InputEvent::Type::KEY)    m_state.updateFromKeyEvent(event.key);

    if (m_callback != nullptr) m_callback(event);
}

// ============================================================================
// DisplayManager Implementation
// ============================================================================

DisplayManager::DisplayManager()
    : m_shouldClose(false)
    , m_eglDisplay(EGL_NO_DISPLAY)
    , m_eglConfig(nullptr)
    , m_eglSharedContext(EGL_NO_CONTEXT)
    #ifdef USE_GSTREAMER
        , m_gstGlDisplay(nullptr)
        , m_gstGlWrappedContext(nullptr)
        , m_gstGlContext(nullptr)
        , m_gstContext(nullptr)
    #endif
    #ifdef USE_X11
        , m_x11Display(nullptr), m_x11RootWindow(0), m_wmDeleteMessage(0)
    #endif
    #ifdef USE_DRM
        , m_drmDeviceFd(-1), m_gbmDevice(nullptr)
    #endif
{}

DisplayManager::~DisplayManager() { shutdown(); }

bool DisplayManager::initialize(const char* title, int width, int height, int windowCount)
{
    LOG_IO_SECTION("Display Initialization");

    #ifdef USE_X11
        if (initializeX11Display(title, width, height, windowCount) == false) return false;
    #endif
    #ifdef USE_DRM
        if (initializeDrmDisplay(width, height) == false) return false;
    #endif

    if (initializeEGL() == false)       return false;
    #ifdef USE_GSTREAMER
        if (initializeGStreamerGL() == false) return false;
    #endif
    return true;
}

void DisplayManager::shutdown()
{
    LOG_IO_INFO("DisplayManager shutting down...");

    if (m_eglDisplay != EGL_NO_DISPLAY)
    {
        LOG_IO_INFO("Releasing current EGL context...");
        eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    #ifdef USE_GSTREAMER
        shutdownGStreamerGL();
    #endif

    #ifdef USE_X11
        shutdownX11Display();
    #endif
    #ifdef USE_DRM
        shutdownDrmDisplay();
    #endif

    shutdownEGL();
    m_shouldClose = true;
    LOG_IO_SUCCESS("DisplayManager shutdown complete");
}

void DisplayManager::beginFrame(int outputIndex)
{
    if (m_displayOn == false && m_useDpms == true) return;

    makeGLContextCurrent(outputIndex);

    #ifdef USE_X11
        if (outputIndex == 0 && m_x11Outputs.size() > 0)
        {
            XEvent xev;
            while (XCheckTypedWindowEvent(m_x11Display, m_x11Outputs[0].nativeWindow, ClientMessage, &xev) == True)
            {
                if (xev.type == ClientMessage && (Atom)xev.xclient.data.l[0] == m_wmDeleteMessage)
                    m_shouldClose = true;
            }
        }
    #endif

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void DisplayManager::endFrame(int outputIndex)
{
    // DPMS mode: signal is cut, nothing to render or swap
    if (m_displayOn == false && m_useDpms == true) return;

    makeGLContextCurrent(outputIndex);

    // Black-frame mode
    if (m_displayOn == false)
    {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    glFinish();

    #ifdef USE_X11
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_x11Outputs.size()))
            eglSwapBuffers(m_eglDisplay, m_x11Outputs[outputIndex].surface);
    #endif
    #ifdef USE_DRM
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_drmOutputs.size()))
            swapDrmBuffers(outputIndex);
    #endif
}

bool DisplayManager::makeGLContextCurrent(int outputIndex)
{
    #ifdef USE_X11
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_x11Outputs.size()))
        {
            if (eglMakeCurrent(m_eglDisplay, m_x11Outputs[outputIndex].surface,
                               m_x11Outputs[outputIndex].surface, m_x11Outputs[outputIndex].context) == EGL_FALSE)
            {
                LOG_IO_ERRORF("Failed to make X11 context current. EGL Error: 0x%x", eglGetError());
                LOG_IO_WARNING("Context may already be current on another thread");
                return false;
            }
            return true;
        }
    #endif
    
    #ifdef USE_DRM
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_drmOutputs.size()))
        {
            if (eglMakeCurrent(m_eglDisplay, m_drmOutputs[outputIndex].surface,
                               m_drmOutputs[outputIndex].surface, m_drmOutputs[outputIndex].context) == EGL_FALSE)
            {
                LOG_IO_ERRORF("Failed to make DRM context current. EGL Error: 0x%x", eglGetError());
                LOG_IO_WARNING("Context may already be current on another thread");
                return false;
            }
            return true;
        }
    #endif
    
    LOG_IO_ERROR("Invalid output index for makeGLContextCurrent");
    return false;
}

bool DisplayManager::releaseGLContext()
{
    if (m_eglDisplay == EGL_NO_DISPLAY) { LOG_IO_WARNING("Cannot release context: No EGL display"); return false; }

    if (eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE)
    {
        LOG_IO_WARNINGF("Failed to release GL context. EGL Error: 0x%x", eglGetError());
        return false;
    }
    return true;
}

int DisplayManager::getOutputCount() const
{
    #ifdef USE_X11
        return static_cast<int>(m_x11Outputs.size());
    #endif
    #ifdef USE_DRM
        return static_cast<int>(m_drmOutputs.size());
    #endif
    return 0;
}

void DisplayManager::getDisplaySize(int& width, int& height, int outputIndex) const
{
    #ifdef USE_X11
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_x11Outputs.size()))
        {
            Window root; int x, y; unsigned int w, h, border, depth;
            XGetGeometry(m_x11Display, m_x11Outputs[outputIndex].nativeWindow, &root, &x, &y, &w, &h, &border, &depth);
            width = static_cast<int>(w); height = static_cast<int>(h);
            return;
        }
    #endif
    #ifdef USE_DRM
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_drmOutputs.size()))
        {
            width  = m_drmOutputs[outputIndex].displayMode.hdisplay;
            height = m_drmOutputs[outputIndex].displayMode.vdisplay;
            return;
        }
    #endif
    width = 1920; height = 1080;
}

DisplayConfig DisplayManager::getDisplayConfig(int outputIndex) const
{
    DisplayConfig config;
    getDisplaySize(config.width, config.height, outputIndex);
    #ifdef USE_DRM
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_drmOutputs.size()))
            config.refreshRate = m_drmOutputs[outputIndex].displayMode.vrefresh;
    #endif
    return config;
}

EGLSurface DisplayManager::getEGLSurface(int outputIndex) const
{
    #ifdef USE_X11
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_x11Outputs.size())) return m_x11Outputs[outputIndex].surface;
    #endif
    #ifdef USE_DRM
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_drmOutputs.size())) return m_drmOutputs[outputIndex].surface;
    #endif
    return EGL_NO_SURFACE;
}

EGLContext DisplayManager::getEGLContext(int outputIndex) const
{
    #ifdef USE_X11
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_x11Outputs.size())) return m_x11Outputs[outputIndex].context;
    #endif
    #ifdef USE_DRM
        if (outputIndex >= 0 && outputIndex < static_cast<int>(m_drmOutputs.size())) return m_drmOutputs[outputIndex].context;
    #endif
    return EGL_NO_CONTEXT;
}

// ----------------------------------------------------------------------------
// DisplayManager - Power Management
// ----------------------------------------------------------------------------

void DisplayManager::setDisplayPowerTimeout(int seconds, bool useDpms)
{
    m_powerTimeoutSecs = seconds;

    #ifdef USE_DRM
        if (useDpms == true) LOG_IO_WARNING("DRM DPMS is dropped — using black-frame mode instead");
        m_useDpms = false;
    #else
        m_useDpms = useDpms;
    #endif

    m_lastActivityTime = TimeUtils::steadyNow();
    LOG_IO_INFOF("Display power timeout set to %d seconds (mode: %s)", seconds, m_useDpms ? "DPMS" : "black-frame");
}

void DisplayManager::notifyUserActivity()
{
    if (m_displayOn == false)
    {
        // DPMS mode: ignore wake events for 3s after turning off — the DPMS signal cut
        // itself causes spurious input events (ADC key, EDID hotplug) that would
        // immediately wake the display back up and leave it frozen.
        if (m_useDpms == true && (TimeUtils::steadyNow() - m_displayOffTime) < 3)
        {
            LOG_IO_INFO("Ignoring wake event — DPMS lockout period");
            return;
        }
        LOG_IO_INFO("User activity detected, turning display on");
        setDisplayPower(true);
    }
    m_lastActivityTime = TimeUtils::steadyNow();
}

void DisplayManager::updateDisplayPower()
{
    if (m_powerTimeoutSecs <= 0 || m_displayOn == false) return;

    time_t elapsed = TimeUtils::steadyNow() - m_lastActivityTime;
    if (elapsed >= static_cast<time_t>(m_powerTimeoutSecs))
    {
        LOG_IO_INFOF("Display idle for %ld seconds, turning off", elapsed);
        setDisplayPower(false);
    }
}

void DisplayManager::setDisplayPower(bool on)
{
    if (m_displayOn == on) return;
    m_displayOn = on;

    if (on == false) m_displayOffTime = TimeUtils::steadyNow();

    if (m_useDpms == false) { LOG_IO_INFOF("Display %s (black-frame mode)", on ? "ON" : "OFF"); return; }

    #ifdef USE_X11
        setDisplayPowerX11(on);
    #endif
    #ifdef USE_DRM
        setDisplayPowerDrm(on);
    #endif
}

// ----------------------------------------------------------------------------
// DisplayManager - EGL
// ----------------------------------------------------------------------------

bool DisplayManager::initializeEGL()
{
    #ifdef USE_X11
        m_eglDisplay = eglGetDisplay((EGLNativeDisplayType)m_x11Display);
    #elif defined(USE_DRM)
        typedef EGLDisplay (*PFN_eglGetPlatformDisplayEXT)(EGLenum, void*, const EGLint*);
        PFN_eglGetPlatformDisplayEXT eglGetPlatformDisplayEXT =
            (PFN_eglGetPlatformDisplayEXT)eglGetProcAddress("eglGetPlatformDisplayEXT");
        m_eglDisplay = (eglGetPlatformDisplayEXT != nullptr)
            ? eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, m_gbmDevice, nullptr)
            : eglGetDisplay((EGLNativeDisplayType)m_gbmDevice);
    #else
        m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    #endif

    if (m_eglDisplay == EGL_NO_DISPLAY) { LOG_IO_ERROR("Failed to get EGL display"); return false; }

    EGLint major = 0, minor = 0;
    if (eglInitialize(m_eglDisplay, &major, &minor) == EGL_FALSE) { LOG_IO_ERROR("Failed to initialize EGL"); return false; }
    LOG_IO_SUCCESSF("EGL %d.%d initialized", major, minor);

    const EGLint configAttribs[] = {
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE
    };

    EGLint count = 0;
    eglGetConfigs(m_eglDisplay, nullptr, 0, &count);
    EGLConfig* configs    = new EGLConfig[count];
    EGLint     numConfigs = 0;
    if (eglChooseConfig(m_eglDisplay, configAttribs, configs, count, &numConfigs) == EGL_FALSE || numConfigs == 0)
    {
        LOG_IO_ERROR("Failed to choose EGL config");
        delete[] configs;
        return false;
    }

    int configIndex = -1;
    #ifdef USE_DRM
        EGLint visualId;
        for (int i = 0; i < numConfigs; ++i)
        {
            if (eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_NATIVE_VISUAL_ID, &visualId) == EGL_FALSE) continue;
            if (visualId == GBM_FORMAT_XRGB8888) { configIndex = i; break; }
        }
    #else
        configIndex = 0;
    #endif

    if (configIndex < 0) { LOG_IO_ERROR("Failed to find matching EGL config"); delete[] configs; return false; }

    m_eglConfig = configs[configIndex];
    delete[] configs;

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    m_eglSharedContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs);
    if (m_eglSharedContext == EGL_NO_CONTEXT) { LOG_IO_ERROR("Failed to create EGL shared context"); return false; }

    #ifdef USE_X11
        for (auto& output : m_x11Outputs)
        {
            output.surface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, (EGLNativeWindowType)output.nativeWindow, nullptr);
            if (output.surface == EGL_NO_SURFACE) { LOG_IO_ERROR("Failed to create EGL surface for X11 window"); return false; }
            output.context = eglCreateContext(m_eglDisplay, m_eglConfig, m_eglSharedContext, contextAttribs);
            if (output.context == EGL_NO_CONTEXT) { LOG_IO_ERROR("Failed to create EGL context for X11 window"); return false; }
        }
    #endif
    #ifdef USE_DRM
        for (auto& output : m_drmOutputs)
        {
            if (output.gbmSurface == nullptr) { LOG_IO_ERROR("GBM surface is null for connector"); return false; }
            output.surface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, (EGLNativeWindowType)output.gbmSurface, nullptr);
            if (output.surface == EGL_NO_SURFACE)
            {
                LOG_IO_ERRORF("Failed to create EGL surface for DRM connector, error: 0x%x", eglGetError());
                return false;
            }
            output.context = eglCreateContext(m_eglDisplay, m_eglConfig, m_eglSharedContext, contextAttribs);
            if (output.context == EGL_NO_CONTEXT) { LOG_IO_ERROR("Failed to create EGL context for DRM connector"); return false; }
        }
    #endif

    makeGLContextCurrent();
    eglSwapInterval(m_eglDisplay, 0);
    LOG_IO_SUCCESS("EGL contexts created");
    return true;
}

void DisplayManager::shutdownEGL()
{
    if (m_eglDisplay == EGL_NO_DISPLAY) return;

    LOG_IO_INFO("Cleaning up EGL resources...");

    if (m_eglSharedContext != EGL_NO_CONTEXT)
    {
        if (eglDestroyContext(m_eglDisplay, m_eglSharedContext) == EGL_FALSE)
            LOG_IO_WARNINGF("Failed to destroy shared context. EGL Error: 0x%x", eglGetError());
        m_eglSharedContext = EGL_NO_CONTEXT;
    }

    if (eglTerminate(m_eglDisplay) == EGL_FALSE)
        LOG_IO_WARNINGF("Failed to terminate EGL display. EGL Error: 0x%x", eglGetError());

    m_eglDisplay = EGL_NO_DISPLAY;
    m_eglConfig  = nullptr;
    LOG_IO_SUCCESS("EGL cleanup complete");
}

// ----------------------------------------------------------------------------
// DisplayManager - GStreamer GL
// ----------------------------------------------------------------------------

#ifdef USE_GSTREAMER

bool DisplayManager::initializeGStreamerGL()
{
    if (gst_is_initialized() == FALSE) { LOG_IO_ERROR("GStreamer must be initialized before GL context"); return false; }

    makeGLContextCurrent();

    #ifdef USE_X11
        m_gstGlDisplay = (GstGLDisplay*)gst_gl_display_x11_new_with_display(m_x11Display);
    #endif
    #ifdef USE_DRM
        m_gstGlDisplay = (GstGLDisplay*)gst_gl_display_egl_new_with_egl_display(m_eglDisplay);
    #endif

    if (m_gstGlDisplay == nullptr) { LOG_IO_ERROR("Failed to create GStreamer GL display"); return false; }

    GstGLPlatform glPlatform = GST_GL_PLATFORM_EGL;
    guintptr      glHandle   = gst_gl_context_get_current_gl_context(glPlatform);
    if (glHandle == 0) { LOG_IO_ERROR("Failed to get current GL context handle"); return false; }

    GstGLAPI glApi          = gst_gl_context_get_current_gl_api(glPlatform, nullptr, nullptr);
    m_gstGlWrappedContext   = gst_gl_context_new_wrapped(m_gstGlDisplay, glHandle, glPlatform, glApi);
    if (m_gstGlWrappedContext == nullptr) { LOG_IO_ERROR("Failed to wrap GL context"); return false; }

    if (gst_gl_context_activate(m_gstGlWrappedContext, TRUE) == FALSE)
        LOG_IO_WARNING("Failed to activate wrapped context");

    GError* error = nullptr;
    if (gst_gl_display_create_context(m_gstGlDisplay, m_gstGlWrappedContext, &m_gstGlContext, &error) == FALSE)
    {
        LOG_IO_WARNING("Failed to create GStreamer GL context, retrying without shared context");
        if (error != nullptr) g_clear_error(&error);
        if (gst_gl_display_create_context(m_gstGlDisplay, nullptr, &m_gstGlContext, &error) == FALSE)
        {
            if (error != nullptr) { LOG_IO_ERRORF("Failed to create GStreamer GL context: %s", error->message); g_clear_error(&error); }
            return false;
        }
    }

    if (gst_gl_display_add_context(m_gstGlDisplay, m_gstGlContext) == FALSE)
        LOG_IO_WARNING("Failed to add GL context to display");

    m_gstContext = gst_context_new(GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
    if (m_gstContext == nullptr) { LOG_IO_ERROR("Failed to create GStreamer context"); return false; }

    gst_context_set_gl_display(m_gstContext, m_gstGlDisplay);
    eglReleaseThread();
    LOG_IO_SUCCESS("GStreamer GL context initialized");
    return true;
}

void DisplayManager::shutdownGStreamerGL()
{
    if (m_gstContext != nullptr)         { gst_context_unref(m_gstContext);           m_gstContext           = nullptr; }
    if (m_gstGlContext != nullptr)       { gst_object_unref(m_gstGlContext);          m_gstGlContext         = nullptr; }
    if (m_gstGlWrappedContext != nullptr){ gst_object_unref(m_gstGlWrappedContext);   m_gstGlWrappedContext  = nullptr; }
    if (m_gstGlDisplay != nullptr)       { gst_object_unref(m_gstGlDisplay);          m_gstGlDisplay         = nullptr; }
    LOG_IO_SUCCESS("GStreamer GL cleanup complete");
}

#endif // USE_GSTREAMER

// ----------------------------------------------------------------------------
// DisplayManager - X11 Platform
// ----------------------------------------------------------------------------

#ifdef USE_X11
#include <X11/extensions/dpms.h>

bool DisplayManager::initializeX11Display(const char* title, int width, int height, int windowCount)
{
    m_x11Display = XOpenDisplay(nullptr);
    if (m_x11Display == nullptr) { LOG_IO_ERROR("Failed to open X11 display"); return false; }

    int screen        = DefaultScreen(m_x11Display);
    m_x11RootWindow   = RootWindow(m_x11Display, screen);
    m_wmDeleteMessage = XInternAtom(m_x11Display, "WM_DELETE_WINDOW", False);

    for (int i = 0; i < windowCount; ++i)
    {
        if (createX11Window(title, width, height, i) == false)
        {
            LOG_IO_ERRORF("Failed to create X11 window %d", i);
            return false;
        }
    }

    LOG_IO_SUCCESS("X11 display initialized");
    return true;
}

void DisplayManager::shutdownX11Display()
{
    if (m_x11Display == nullptr) return;

    LOG_IO_INFO("Cleaning up X11 resources...");

    for (auto& output : m_x11Outputs)
    {
        if (output.context != EGL_NO_CONTEXT)
        {
            if (eglDestroyContext(m_eglDisplay, output.context) == EGL_FALSE)
                LOG_IO_WARNINGF("Failed to destroy X11 output context. EGL Error: 0x%x", eglGetError());
            output.context = EGL_NO_CONTEXT;
        }
        if (output.surface != EGL_NO_SURFACE)
        {
            if (eglDestroySurface(m_eglDisplay, output.surface) == EGL_FALSE)
                LOG_IO_WARNINGF("Failed to destroy X11 output surface. EGL Error: 0x%x", eglGetError());
            output.surface = EGL_NO_SURFACE;
        }
        if (output.nativeWindow != None) { XDestroyWindow(m_x11Display, output.nativeWindow); output.nativeWindow = None; }
    }
    m_x11Outputs.clear();

    XCloseDisplay(m_x11Display);
    m_x11Display      = nullptr;
    m_x11RootWindow   = None;
    m_wmDeleteMessage = None;
    LOG_IO_SUCCESS("X11 cleanup complete");
}

bool DisplayManager::createX11Window(const char* title, int width, int height, int windowIndex)
{
    int screen = DefaultScreen(m_x11Display);

    XSetWindowAttributes windowAttribs;
    windowAttribs.event_mask    = StructureNotifyMask | ExposureMask;
    windowAttribs.background_pixel = BlackPixel(m_x11Display, screen);

    Window window = XCreateWindow(m_x11Display, m_x11RootWindow,
                                  windowIndex * 50, windowIndex * 50, width, height, 0,
                                  CopyFromParent, InputOutput, CopyFromParent,
                                  CWBackPixel | CWEventMask, &windowAttribs);
    if (window == 0) { LOG_IO_ERRORF("Failed to create X11 window %d", windowIndex); return false; }

    std::string windowTitle = (windowIndex > 0) ? (std::string(title) + " [" + std::to_string(windowIndex) + "]") : title;
    XStoreName(m_x11Display, window, windowTitle.c_str());
    XSetWMProtocols(m_x11Display, window, &m_wmDeleteMessage, 1);
    XMapWindow(m_x11Display, window);
    XFlush(m_x11Display);

    X11Output output;
    output.nativeWindow = window;
    m_x11Outputs.push_back(output);

    LOG_IO_SUCCESSF("X11 window #%d created: %dx%d at (%d,%d)", windowIndex, width, height, windowIndex * 50, windowIndex * 50);
    return true;
}

void DisplayManager::setDisplayPowerX11(bool on)
{
    if (m_x11Display == nullptr) return;
    DPMSForceLevel(m_x11Display, on ? DPMSModeOn : DPMSModeOff);
    XFlush(m_x11Display);
    LOG_IO_INFOF("X11 DPMS: display %s", on ? "ON" : "OFF");
}

#endif // USE_X11

// ----------------------------------------------------------------------------
// DisplayManager - DRM Platform
// ----------------------------------------------------------------------------

#ifdef USE_DRM

bool DisplayManager::initializeDrmDisplay(int width, int height)
{
    // Scan all displays from sysfs first
    scanAllDisplays();
    if (m_allDisplays.empty() == true) { LOG_IO_ERROR("No DRM displays found in /sys/class/drm"); return false; }

    // Find available DRM card devices from the scanned displays
    std::set<std::string> cardNames;
    for (const auto& display : m_allDisplays)
    {
        size_t dashPos = display.name.find("-");
        if (dashPos != std::string::npos) cardNames.insert(display.name.substr(0, dashPos));
    }
    if (cardNames.empty() == true) { LOG_IO_ERROR("No DRM cards found"); return false; }

    // Try to open cards until we find one with resources
    drmModeRes* resources = nullptr;
    std::string usedCard;
    for (const auto& cardName : cardNames)
    {
        std::string cardPath = "/dev/dri/" + cardName;
        m_drmDeviceFd = open(cardPath.c_str(), O_RDWR | O_CLOEXEC);
        if (m_drmDeviceFd < 0) { LOG_IO_DEBUGF("Cannot open %s", cardPath.c_str()); continue; }

        resources = drmModeGetResources(m_drmDeviceFd);
        if (resources != nullptr) { usedCard = cardName; LOG_IO_SUCCESSF("Using %s", cardPath.c_str()); break; }

        LOG_IO_DEBUGF("%s has no DRM resources", cardPath.c_str());
        close(m_drmDeviceFd);
        m_drmDeviceFd = -1;
    }
    if (resources == nullptr) { LOG_IO_ERROR("Unable to get DRM resources from any card"); return false; }

    // Create GBM device
    m_gbmDevice = gbm_create_device(m_drmDeviceFd);
    if (m_gbmDevice == nullptr) { LOG_IO_ERROR("Failed to create GBM device"); drmModeFreeResources(resources); return false; }

    // Initialize connected displays
    for (int i = 0; i < resources->count_connectors; ++i)
    {
        drmModeConnector* connector = drmModeGetConnector(m_drmDeviceFd, resources->connectors[i]);
        if (connector == nullptr) continue;

        if (connector->connection != DRM_MODE_CONNECTED || connector->count_modes == 0)
        {
            drmModeFreeConnector(connector);
            continue;
        }

        DrmOutput output;
        output.connectorId   = connector->connector_id;
        output.encoderId     = connector->encoder_id;
        output.displayMode   = findBestDisplayMode(connector, width, height, 60);
        output.connectorName = usedCard + "-" + getConnectorTypeName(connector->connector_type) + "-" + std::to_string(connector->connector_type_id);

        for (int j = 0; j < resources->count_encoders && output.crtcId == 0; ++j)
        {
            drmModeEncoder* encoder = drmModeGetEncoder(m_drmDeviceFd, resources->encoders[j]);
            if (encoder == nullptr) continue;

            bool encoderUsable = (output.encoderId != 0 && encoder->encoder_id == output.encoderId);
            if (encoderUsable == false)
            {
                for (int p = 0; p < connector->count_encoders; ++p)
                {
                    if (connector->encoders[p] == encoder->encoder_id) { encoderUsable = true; break; }
                }
            }

            if (encoderUsable == true)
            {
                for (int k = 0; k < resources->count_crtcs; ++k)
                {
                    if ((encoder->possible_crtcs & (1 << k)) == 0) continue;
                    bool crtcInUse = false;
                    for (const auto& existing : m_drmOutputs)
                    {
                        if (existing.crtcId == resources->crtcs[k]) { crtcInUse = true; break; }
                    }
                    if (crtcInUse == false) { output.crtcId = resources->crtcs[k]; output.encoderId = encoder->encoder_id; break; }
                }
            }
            drmModeFreeEncoder(encoder);
        }

        if (output.crtcId == 0)
        {
            LOG_IO_ERRORF("Failed to find CRTC for %s", output.connectorName.c_str());
            drmModeFreeConnector(connector);
            continue;
        }

        // Create GBM surface
        output.gbmSurface = gbm_surface_create(m_gbmDevice, output.displayMode.hdisplay, output.displayMode.vdisplay,
                                                GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        if (output.gbmSurface == nullptr)
        {
            LOG_IO_ERRORF("Failed to create GBM surface for %s", output.connectorName.c_str());
            drmModeFreeConnector(connector);
            continue;
        }

        output.isActive = true;
        m_drmOutputs.push_back(output);
        LOG_IO_SUCCESSF("Connector %s: %dx%d@%dHz (CRTC %d)",
                        output.connectorName.c_str(),
                        output.displayMode.hdisplay, output.displayMode.vdisplay,
                        output.displayMode.vrefresh, output.crtcId);

        // Update m_allDisplays with active connector info
        for (auto& display : m_allDisplays)
        {
            if (display.name == output.connectorName)
            {
                display.isActive    = true;
                display.width       = output.displayMode.hdisplay;
                display.height      = output.displayMode.vdisplay;
                display.refreshRate = output.displayMode.vrefresh;
                break;
            }
        }

        drmModeFreeConnector(connector);
    }

    drmModeFreeResources(resources);

    if (m_drmOutputs.empty() == true) { LOG_IO_ERROR("No suitable DRM connectors found"); return false; }
    return true;
}

void DisplayManager::shutdownDrmDisplay()
{
    LOG_IO_INFO("Cleaning up DRM resources...");

    for (auto& output : m_drmOutputs)
    {
        if (output.context != EGL_NO_CONTEXT)
        {
            LOG_IO_INFOF("Destroying EGL context for connector %u", output.connectorId);
            if (eglDestroyContext(m_eglDisplay, output.context) == EGL_FALSE)
            {
                EGLint error = eglGetError();
                LOG_IO_WARNINGF("Failed to destroy DRM output context. EGL Error: 0x%x", error);
            }
            output.context = EGL_NO_CONTEXT;
        }
        
        if (output.surface != EGL_NO_SURFACE)
        {
            LOG_IO_INFOF("Destroying EGL surface for connector %u", output.connectorId);
            if (eglDestroySurface(m_eglDisplay, output.surface) == EGL_FALSE)
            {
                EGLint error = eglGetError();
                LOG_IO_WARNINGF("Failed to destroy DRM output surface. EGL Error: 0x%x", error);
            }
            output.surface = EGL_NO_SURFACE;
        }
        
        if (output.gbmSurface != nullptr)
        {
            LOG_IO_INFOF("Destroying GBM surface for connector %u", output.connectorId);
            gbm_surface_destroy(output.gbmSurface);
            output.gbmSurface = nullptr;
        }
        
        if (output.previousBuffer != nullptr)
        {
            drmModeRmFB(m_drmDeviceFd, output.previousFrameBuffer);
            gbm_surface_release_buffer(output.gbmSurface, output.previousBuffer);
            output.previousBuffer = nullptr;
        }
        
        if (output.previousFrameBuffer != 0)
        {
            drmModeRmFB(m_drmDeviceFd, output.previousFrameBuffer);
            output.previousFrameBuffer = 0;
        }
        
        if (output.frameBufferId != 0)
        {
            drmModeRmFB(m_drmDeviceFd, output.frameBufferId);
            output.frameBufferId = 0;
        }
    }


    for (auto& output : m_drmOutputs)
    {
        if (output.context != EGL_NO_CONTEXT)
        {
            if (eglDestroyContext(m_eglDisplay, output.context) == EGL_FALSE)
                LOG_IO_WARNINGF("Failed to destroy DRM output context. EGL Error: 0x%x", eglGetError());
            output.context = EGL_NO_CONTEXT;
        }
        if (output.surface != EGL_NO_SURFACE)
        {
            if (eglDestroySurface(m_eglDisplay, output.surface) == EGL_FALSE)
                LOG_IO_WARNINGF("Failed to destroy DRM output surface. EGL Error: 0x%x", eglGetError());
            output.surface = EGL_NO_SURFACE;
        }
        if (output.gbmSurface != nullptr) { gbm_surface_destroy(output.gbmSurface); output.gbmSurface = nullptr; }
        if (output.previousBuffer != nullptr && output.previousFrameBuffer != 0)
        {
            drmModeRmFB(m_drmDeviceFd, output.previousFrameBuffer);
            gbm_surface_release_buffer(output.gbmSurface, output.previousBuffer);
            output.previousBuffer      = nullptr;
            output.previousFrameBuffer = 0;
        }
        if (output.frameBufferId != 0)
        {
            drmModeRmFB(m_drmDeviceFd, output.frameBufferId);
            output.frameBufferId = 0;
        }
    }

    m_drmOutputs.clear();
    m_allDisplays.clear();

    if (m_gbmDevice != nullptr)   { gbm_device_destroy(m_gbmDevice); m_gbmDevice = nullptr; }
    if (m_drmDeviceFd >= 0)       { close(m_drmDeviceFd); m_drmDeviceFd = -1; }

    LOG_IO_SUCCESS("DRM cleanup complete");
}

void DisplayManager::scanAllDisplays()
{
    m_allDisplays.clear();

    DIR* drmDir = opendir("/sys/class/drm");
    if (drmDir == nullptr) return;

    struct dirent* entry;
    while ((entry = readdir(drmDir)) != nullptr)
    {
        std::string name = entry->d_name;
        if (name.find("card") == std::string::npos || name.find("-") == std::string::npos) continue;

        DisplayInfo info;
        info.name = name;

        std::string statusPath = std::string("/sys/class/drm/") + name + "/status";
        FILE*       statusFile = fopen(statusPath.c_str(), "r");
        if (statusFile != nullptr)
        {
            char status[32] = {0};
            if (fgets(status, sizeof(status), statusFile) != nullptr)
            {
                size_t len = strlen(status);
                if (len > 0 && status[len - 1] == '\n') status[len - 1] = '\0';
                info.status = status;
            }
            fclose(statusFile);
        }
        m_allDisplays.push_back(info);
    }
    closedir(drmDir);
}

void DisplayManager::swapDrmBuffers(int connectorIndex)
{
    if (connectorIndex < 0 || connectorIndex >= static_cast<int>(m_drmOutputs.size())) return;
    
    DrmOutput& output = m_drmOutputs[connectorIndex];
    eglSwapBuffers(m_eglDisplay, output.surface);
    
    gbm_bo* buffer = gbm_surface_lock_front_buffer(output.gbmSurface);
    if (buffer == nullptr) { LOG_IO_ERROR("gbm_surface_lock_front_buffer returned nullptr — skipping frame"); return; }

    uint32_t handles[4] = {gbm_bo_get_handle(buffer).u32, 0, 0, 0};
    uint32_t pitches[4] = {gbm_bo_get_stride(buffer), 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};

    uint32_t newFrameBufferId = 0;
    int addFbResult = drmModeAddFB2(m_drmDeviceFd,
                                    output.displayMode.hdisplay, output.displayMode.vdisplay,
                                    DRM_FORMAT_XRGB8888,
                                    handles, pitches, offsets,
                                    &newFrameBufferId, 0);
    if (addFbResult != 0)
    {
        LOG_IO_ERRORF("drmModeAddFB2 failed (%d) — releasing buffer and skipping frame", addFbResult);
        gbm_surface_release_buffer(output.gbmSurface, buffer);
        return;
    }

    if (drmModeSetCrtc(m_drmDeviceFd, output.crtcId, newFrameBufferId, 0, 0,
                       &output.connectorId, 1, &output.displayMode) != 0)
    {
        LOG_IO_ERROR("drmModeSetCrtc failed — releasing new framebuffer");
        drmModeRmFB(m_drmDeviceFd, newFrameBufferId);
        gbm_surface_release_buffer(output.gbmSurface, buffer);
        return;
    }

    // Only free the previous resources once the new scanout is successfully set.
    if (output.previousBuffer != nullptr)
    {
        drmModeRmFB(m_drmDeviceFd, output.previousFrameBuffer);
        gbm_surface_release_buffer(output.gbmSurface, output.previousBuffer);
    }

    output.previousBuffer      = buffer;
    output.previousFrameBuffer = newFrameBufferId;
    output.frameBufferId       = newFrameBufferId;
}

std::string DisplayManager::getConnectorTypeName(uint32_t connectorType)
{
    switch (connectorType)
    {
        case DRM_MODE_CONNECTOR_VGA:         return "VGA";
        case DRM_MODE_CONNECTOR_DVII:        return "DVI-I";
        case DRM_MODE_CONNECTOR_DVID:        return "DVI-D";
        case DRM_MODE_CONNECTOR_DVIA:        return "DVI-A";
        case DRM_MODE_CONNECTOR_Composite:   return "Composite";
        case DRM_MODE_CONNECTOR_SVIDEO:      return "S-Video";
        case DRM_MODE_CONNECTOR_LVDS:        return "LVDS";
        case DRM_MODE_CONNECTOR_Component:   return "Component";
        case DRM_MODE_CONNECTOR_9PinDIN:     return "9-Pin-DIN";
        case DRM_MODE_CONNECTOR_DisplayPort: return "DP";
        case DRM_MODE_CONNECTOR_HDMIA:       return "HDMI-A";
        case DRM_MODE_CONNECTOR_HDMIB:       return "HDMI-B";
        case DRM_MODE_CONNECTOR_TV:          return "TV";
        case DRM_MODE_CONNECTOR_eDP:         return "eDP";
        case DRM_MODE_CONNECTOR_VIRTUAL:     return "Virtual";
        case DRM_MODE_CONNECTOR_DSI:         return "DSI";
        default:                             return "Unknown";
    }
}

drmModeModeInfo DisplayManager::findBestDisplayMode(drmModeConnector* connector, uint16_t width, uint16_t height, uint32_t refreshRate)
{
    for (int i = 0; i < connector->count_modes; ++i)
    {
        const drmModeModeInfo& mode = connector->modes[i];
        if (mode.hdisplay != width || mode.vdisplay != height) continue;
        if (refreshRate == 0 || mode.vrefresh == refreshRate)  return mode;
    }
    LOG_IO_WARNINGF("No matching mode for %dx%d@%d, using preferred", width, height, refreshRate);
    return connector->modes[0];
}

void DisplayManager::setDisplayPowerDrm(bool on)
{
    // if (m_drmDeviceFd < 0) return;

    // uint32_t dpmsValue = on ? DRM_MODE_DPMS_ON : DRM_MODE_DPMS_OFF;

    // for (auto& output : m_drmOutputs)
    // {
    //     if (output.isActive == false) continue;

    //     drmModeConnector* connector = drmModeGetConnector(m_drmDeviceFd, output.connectorId);
    //     if (connector == nullptr) continue;

    //     for (int i = 0; i < connector->count_props; ++i)
    //     {
    //         drmModePropertyPtr prop = drmModeGetProperty(m_drmDeviceFd, connector->props[i]);
    //         if (prop == nullptr) continue;
    //         if (strcmp(prop->name, "DPMS") == 0)
    //         {
    //             drmModeConnectorSetProperty(m_drmDeviceFd, output.connectorId, prop->prop_id, dpmsValue);
    //             LOG_IO_INFOF("DRM DPMS %s on connector %s", on ? "ON" : "OFF", output.connectorName.c_str());
    //         }
    //         drmModeFreeProperty(prop);
    //     }
    //     drmModeFreeConnector(connector);
    // }

    (void)on;
}

#endif // USE_DRM

// ============================================================================
// Platform Implementation (Singleton Facade)
// ============================================================================

Platform::Platform()
    : m_displayManager(nullptr)
    , m_inputHandler(nullptr)
    , m_currentContextThread()
{}

Platform::~Platform()
{
    { std::lock_guard<std::mutex> lock(m_contextMutex); m_currentContextThread = std::thread::id(); }
    shutdown();
}

Platform& Platform::getInstance()
{
    static Platform instance;
    return instance;
}

bool Platform::initialize(const char* title, int width, int height, int windowCount)
{
    #ifdef USE_GSTREAMER
        if (gst_is_initialized() == FALSE)
        {
            if (setenv("GST_GL_API", "gles2", 1) != 0)       LOG_IO_ERROR("Unable to set GST_GL_API env var");
            if (setenv("GST_GL_PLATFORM", "egl", 1) != 0)    LOG_IO_ERROR("Unable to set GST_GL_PLATFORM env var");
            gst_init(nullptr, nullptr);
        }
    #endif // USE_GSTREAMER
    
    // Initialize display manager
    m_displayManager = std::make_unique<DisplayManager>();
    if (m_displayManager->initialize(title, width, height, windowCount) == false) return false;
    
    // Initialize input reader
    m_inputReader = std::make_unique<InputReader>();

    #ifdef USE_X11
        if (m_displayManager->m_x11Outputs.size() > 0)
            m_inputReader->setX11Window(m_displayManager->m_x11Display, m_displayManager->m_x11Outputs[0].nativeWindow);
    #endif
    #ifdef USE_DRM
        int actualWidth, actualHeight;
        m_displayManager->getDisplaySize(actualWidth, actualHeight, 0);
        m_inputReader->setDisplaySize(actualWidth, actualHeight);
        LOG_IO_INFOF("InputReader configured for display: %dx%d (requested: %dx%d)", actualWidth, actualHeight, width, height);
    #endif

    m_inputReader->initialize();
    
    // Initialize input handler
    m_inputHandler = std::make_unique<InputHandler>();
    
    // Log initialization info
    logInitializationInfo();
    
    return true;
}

void Platform::shutdown()
{
    LOG_IO_INFO("Platform shutting down...");

    if (m_displayManager != nullptr) { m_displayManager->shutdown(); m_displayManager.reset(); }
    m_inputReader.reset();
    m_inputHandler.reset();
    m_eventBuffer.clear();

    LOG_IO_SUCCESS("Platform shutdown complete");
}

void Platform::beginFrame(int outputIndex)
{
    if (m_displayManager != nullptr) m_displayManager->beginFrame(outputIndex);
}

void Platform::endFrame(int outputIndex)
{
    if (m_displayManager != nullptr) m_displayManager->endFrame(outputIndex);
}

bool Platform::makeGLContextCurrent(int outputIndex)
{
    if (m_displayManager == nullptr) { LOG_IO_ERROR("Cannot make context current: DisplayManager not initialized"); return false; }

    std::lock_guard<std::mutex> lock(m_contextMutex);
    std::thread::id thisThread = std::this_thread::get_id();

    if (m_currentContextThread == thisThread) return true;

    if (m_currentContextThread != std::thread::id())
    {
        LOG_IO_WARNINGF("Context is currently owned by thread %zu, attempting to make current on thread %zu",
                        std::hash<std::thread::id>{}(m_currentContextThread), std::hash<std::thread::id>{}(thisThread));
        LOG_IO_WARNING("This may fail - call releaseGLContext() from the owning thread first");
    }

    if (m_displayManager->makeGLContextCurrent(outputIndex) == false)
    {
        LOG_IO_ERRORF("Failed to make context current on thread %zu", std::hash<std::thread::id>{}(thisThread));
        return false;
    }

    m_currentContextThread = thisThread;
    LOG_IO_INFOF("GL context now current on thread %zu", std::hash<std::thread::id>{}(thisThread));
    return true;
}

void Platform::releaseGLContext()
{
    if (m_displayManager == nullptr) { LOG_IO_WARNING("Cannot release context: DisplayManager not initialized"); return; }

    std::lock_guard<std::mutex> lock(m_contextMutex);
    std::thread::id thisThread = std::this_thread::get_id();

    if (m_currentContextThread != thisThread && m_currentContextThread != std::thread::id())
        LOG_IO_WARNINGF("Releasing context from thread %zu, but tracked owner is thread %zu",
                        std::hash<std::thread::id>{}(thisThread), std::hash<std::thread::id>{}(m_currentContextThread));

    m_displayManager->releaseGLContext();
    m_currentContextThread = std::thread::id();
    LOG_IO_INFO("GL context released - available for any thread");
}

void Platform::pollInput()
{
    if (m_inputReader == nullptr || m_inputHandler == nullptr) return;

    m_eventBuffer.clear();
    m_inputReader->pollEvents(m_eventBuffer);

    for (const auto& event : m_eventBuffer)
    {
        // Only count real user gestures (KEY/MOTION) as activity — not device connect/disconnect.
        if ((event.type == InputEvent::Type::KEY || event.type == InputEvent::Type::MOTION) &&
            m_displayManager != nullptr)
            m_displayManager->notifyUserActivity();

        m_inputHandler->handleEvent(event);
    }
}

#ifdef USE_X11
void Platform::setActiveInputWindow(int outputIndex)
{
    if (m_displayManager == nullptr || m_inputReader == nullptr) return;
    if (outputIndex < 0 || outputIndex >= static_cast<int>(m_displayManager->m_x11Outputs.size()))
    {
        LOG_IO_WARNINGF("Platform: Invalid output index %d for setActiveInputWindow", outputIndex);
        return;
    }
    Window window = m_displayManager->m_x11Outputs[outputIndex].nativeWindow;
    m_inputReader->setX11Window(m_displayManager->m_x11Display, window);
    LOG_IO_INFOF("Platform: Active input window set to output %d (Window ID: %lu)", outputIndex, window);
}
#endif

void Platform::setInputCallback(InputEventCallback callback)
{
    if (m_inputHandler != nullptr) m_inputHandler->setCallback(callback);
}

void Platform::setTerminalCommandCallback(TerminalCommandCallback callback)
{
    if (m_inputReader != nullptr) m_inputReader->setTerminalCommandCallback(callback);
}

void Platform::setDeviceConnectionCallback(DeviceConnectionCallback callback)
{
    if (m_inputReader == nullptr) return;

    m_inputReader->setDeviceConnectionCallback(
        [this, callback](const std::string& name, int eventNum, DeviceType type, bool connected)
        {
            // if (connected == true && m_displayManager != nullptr) m_displayManager->notifyUserActivity();
            if (callback != nullptr) callback(name, eventNum, type, connected);
        });

    #ifdef USE_DRM
        m_inputReader->notifyExistingDevices();
    #endif
}

bool          Platform::shouldClose()                     const { return m_displayManager != nullptr ? m_displayManager->shouldClose()                 : true; }
int           Platform::getOutputCount()                  const { return m_displayManager != nullptr ? m_displayManager->getOutputCount()              : 0; }
DisplayConfig Platform::getDisplayConfig(int outputIndex) const { return m_displayManager != nullptr ? m_displayManager->getDisplayConfig(outputIndex) : DisplayConfig(); }
bool          Platform::isDisplayOn()                     const { return m_displayManager != nullptr ? m_displayManager->isDisplayOn()                 : true; }

#ifdef USE_GSTREAMER
    GstGLDisplay* Platform::getGstGLDisplay()             const { return m_displayManager != nullptr ? m_displayManager->getGstGLDisplay()             : nullptr; }
    GstGLContext* Platform::getGstGLContext()             const { return m_displayManager != nullptr ? m_displayManager->getGstGLContext()             : nullptr; }
    GstContext*   Platform::getGstContext()               const { return m_displayManager != nullptr ? m_displayManager->getGstContext()               : nullptr; }
#endif // USE_GSTREAMER

EGLDisplay    Platform::getEGLDisplay()                   const { return m_displayManager != nullptr ? m_displayManager->getEGLDisplay()               : EGL_NO_DISPLAY; }
EGLContext    Platform::getEGLSharedContext()             const { return m_displayManager != nullptr ? m_displayManager->getEGLSharedContext()         : EGL_NO_CONTEXT; }
EGLConfig     Platform::getEGLConfig()                    const { return m_displayManager != nullptr ? m_displayManager->getEGLConfig()                : nullptr; }
EGLSurface    Platform::getEGLSurface(int outputIndex)    const { return m_displayManager != nullptr ? m_displayManager->getEGLSurface(outputIndex)    : EGL_NO_SURFACE; }
EGLContext    Platform::getEGLContext(int outputIndex)    const { return m_displayManager != nullptr ? m_displayManager->getEGLContext(outputIndex)    : EGL_NO_CONTEXT; }

void Platform::getDisplaySize(int& width, int& height, int outputIndex) const
{
    if (m_displayManager != nullptr) m_displayManager->getDisplaySize(width, height, outputIndex);
    else { width = 1920; height = 1080; }
}

void Platform::setDisplayPowerTimeout(int seconds, bool useDpms) { if (m_displayManager != nullptr) m_displayManager->setDisplayPowerTimeout(seconds, useDpms); }
void Platform::updateDisplayPower()                              { if (m_displayManager != nullptr) m_displayManager->updateDisplayPower(); }
void Platform::notifyUserActivity()                              { if (m_displayManager != nullptr) m_displayManager->notifyUserActivity(); }

// ----------------------------------------------------------------------------
// Platform - Initialization Logging
// ----------------------------------------------------------------------------

void Platform::logInitializationInfo()
{
    LOG::BoxBuilder box = LOG::Logger::createBox(70);

    #ifdef USE_X11
        box.setTitle("Platform: X11");
    #endif
    #ifdef USE_DRM
        box.setTitle("Platform: DRM/KMS");
    #endif

    #ifdef USE_X11
        if (m_displayManager != nullptr)
        {
            int width, height;
            m_displayManager->getDisplaySize(width, height);
            box.addLinef("Display: %dx%d (Active)", width, height);
            box.addLinef("Windows: %d", m_displayManager->getOutputCount());
        }
    #endif

    #ifdef USE_DRM
        if (m_displayManager != nullptr && m_displayManager->m_allDisplays.empty() == false)
        {
            box.addLinef("Available Displays:");
            for (const auto& display : m_displayManager->m_allDisplays)
            {
                if (display.isActive == true)
                    box.addLinef("  %s%s: %dx%d@%dHz [ACTIVE]%s", LOG::Color::BOLD_GREEN, display.name.c_str(), display.width, display.height, display.refreshRate, LOG::Color::RESET);
                else if (display.status == "connected")
                    box.addLinef("  %s%s: Connected but not active%s", LOG::Color::YELLOW, display.name.c_str(), LOG::Color::RESET);
                else if (display.status == "disconnected")
                    box.addLinef("  %s%s: Disconnected%s", LOG::Color::DIM, display.name.c_str(), LOG::Color::RESET);
            }
        }
    #endif

    box.addSeparator();

    #ifdef USE_DRM
        if (m_inputReader != nullptr)
        {
            int deviceCount = static_cast<int>(m_inputReader->m_connectedDevices.size());
            box.addLinef("Input Devices: %d connected", deviceCount);

            if (deviceCount > 0)
            {
                std::vector<const InputDevice*> keyboards, mice, touchscreens, others;
                for (const auto& dev : m_inputReader->m_connectedDevices)
                {
                    if      (dev.type == DeviceType::KEYBOARD)    keyboards.push_back(&dev);
                    else if (dev.type == DeviceType::MOUSE)       mice.push_back(&dev);
                    else if (dev.type == DeviceType::TOUCHSCREEN) touchscreens.push_back(&dev);
                    else                                          others.push_back(&dev);
                }

                auto printDevList = [&](const char* label, const char* color, const std::vector<const InputDevice*>& list)
                {
                    if (list.empty() == true) return;
                    box.addSeparator();
                    box.addLinef("%s%s (%d):%s", color, label, static_cast<int>(list.size()), LOG::Color::RESET);
                    for (const auto* dev : list) box.addLinef("  [event%d] %s", dev->eventNumber, dev->name.c_str());
                };

                printDevList("Keyboards",    LOG::Color::CYAN,    keyboards);
                printDevList("Mice",         LOG::Color::YELLOW,  mice);
                printDevList("Touchscreens", LOG::Color::MAGENTA, touchscreens);
                printDevList("Other",        LOG::Color::WHITE,   others);

                for (const auto* dev : touchscreens)
                {
                    if (dev->touchCalibration.needsMapping == true)
                        box.addLinef("    [event%d] Touch range: %d-%d x %d-%d -> %dx%d",
                                     dev->eventNumber,
                                     dev->touchCalibration.minX, dev->touchCalibration.maxX,
                                     dev->touchCalibration.minY, dev->touchCalibration.maxY,
                                     dev->touchCalibration.displayWidth, dev->touchCalibration.displayHeight);
                }
            }
            else
            {
                box.addLinef("  No input devices detected");
            }
        }
    #endif

    #ifdef USE_X11
        box.addLinef("Input: X11 Input Extension ready");
    #endif

    if (m_inputReader != nullptr && m_inputReader->m_terminalInputEnabled == true)
    {
        box.addSeparator();
        box.addLinef("%sTerminal: Command input enabled%s", LOG::Color::BOLD_GREEN, LOG::Color::RESET);
    }

    box.render();
}

} // namespace IO
} // namespace APP
