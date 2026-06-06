#ifndef APP_BASE_H
#define APP_BASE_H

#include "ui_core.h"
#include "ui_elements.h"
#include "constants.h"
#include "logger.h"
#include <vector>
#include <string>
#include <memory>

namespace APP
{

class AppBase
{
public:
    AppBase();
    virtual ~AppBase();

    // ── Lifecycle ─────────────────────────────────────────────────────────
    bool initialize();
    void cleanup();
    void update(float deltaTime);
    void render(int windowWidth, int windowHeight);

    // ── Input ─────────────────────────────────────────────────────────────
    void handleKeyEvent(const IO::KeyEvent& event);
    void handleMotionEvent(const IO::MotionEvent& event);
    void handleTerminalCommand(const std::string& command);

    // ── Canvas ────────────────────────────────────────────────────────────
    void setSharedCanvas(UI::ICanvas* canvas) { m_currentCanvas = canvas; }

    // ── Suspend / Resume ──────────────────────────────────────────────────
    void onSuspend();
    void onResume();
    virtual void onSuspended() {}
    virtual void onResumed()   {}

    // ── State queries ──────────────────────────────────────────────────────
    bool         shouldExit() const { return m_shouldExit; }
    UI::ICanvas* getCanvas()        { return m_currentCanvas; }
    void         requestExit()      { m_shouldExit = true; }

protected:
    // ── Pure-virtual hooks ─────────────────────────────────────────────────
    virtual bool onInitialize()                                = 0;
    virtual void onCleanup()                                   = 0;
    virtual void onUpdate(float deltaTime)                     = 0;
    virtual void onRender()                                    = 0;
    virtual bool onKeyEvent(const IO::KeyEvent& event)         = 0;
    virtual bool onMotionEvent(const IO::MotionEvent& event)   = 0;
    virtual bool onTerminalCommand(const std::string& command) { return false; }

    // ── Optional hooks ─────────────────────────────────────────────────────
    virtual void onCanvasInitialized() {}
    virtual void onResourcesLoaded()   {}
    virtual std::vector<UI::FontConfig> createFontConfigs();

    // ── Cursor ────────────────────────────────────────────────────────────
    void setupCursorView();
    void updateCursorView(float deltaTime);
    void cycleCursorVisibilityMode();
    void cycleCursorStyle();

    // ── Font config ────────────────────────────────────────────────────────
    const std::vector<UI::FontConfig>& getFontConfigs()         const { return m_fontConfigs; }
    int                                getCurrentFontIndex()    const { return m_currentFontIndex; }
    void                               setCurrentFontIndex(int index) { m_currentFontIndex = index; }

    // ── Members ────────────────────────────────────────────────────────────
    UI::ICanvas*     m_currentCanvas;   // NOT owned
    UI::FrameLayout* m_rootLayout;
    UI::CursorView*  m_cursorView;

    bool m_shouldExit;
    int  m_screenWidth;
    int  m_screenHeight;
    bool m_isCleanedUp;

private:
    std::vector<UI::FontConfig> m_fontConfigs;
    int m_currentFontIndex;
};

} // namespace APP

#endif // APP_BASE_H
