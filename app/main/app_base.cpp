#include "app_base.h"
#include "io_platform.h"
#include <algorithm>

namespace APP
{

// ============================================================================
// AppBase
// ============================================================================

AppBase::AppBase()
    : m_currentCanvas(nullptr)
    , m_rootLayout(nullptr)
    , m_cursorView(nullptr)
    , m_shouldExit(false)
    , m_screenWidth(1920)
    , m_screenHeight(1080)
    , m_isCleanedUp(false)
    , m_currentFontIndex(0)
{}

AppBase::~AppBase()
{
    if (m_isCleanedUp == false)
    {
        LOG_APP_WARNING("AppBase: Destructor called without prior cleanup()");
        if (m_rootLayout != nullptr) { delete m_rootLayout; m_rootLayout = nullptr; }
        m_fontConfigs.clear();
        m_isCleanedUp = true;
    }
}

bool AppBase::initialize()
{
    LOG_APP_SECTION("AppBase: Initializing");

    if (m_currentCanvas == nullptr)
    {
        LOG_APP_ERROR("AppBase: No canvas set — call setSharedCanvas() before initialize()");
        return false;
    }

    onCanvasInitialized();

    LOG_APP_INFO("AppBase: Initializing FontManager...");
    if (UI::FontManager::getInstance().initialize() == false)
    {
        LOG_APP_ERROR("AppBase: Failed to initialize FontManager");
        return false;
    }
    LOG_APP_SUCCESS("AppBase: FontManager initialized");

    std::vector<UI::FontConfig> fontConfigs = createFontConfigs();
    if (fontConfigs.empty() == false)
    {
        m_fontConfigs      = fontConfigs;
        m_currentFontIndex = 0;
        if (m_currentCanvas != nullptr)
        {
            m_currentCanvas->reloadFonts(m_fontConfigs);
            LOG_APP_SUCCESSF("AppBase: Loaded %zu font(s)", m_fontConfigs.size());
        }
    }

    onResourcesLoaded();

    if (onInitialize() == false)
    {
        LOG_APP_ERROR("AppBase: Derived class initialization failed");
        return false;
    }

    LOG_APP_SUCCESS("AppBase: Initialized successfully");
    return true;
}

void AppBase::cleanup()
{
    if (m_isCleanedUp == true) return;
    LOG_APP_INFO("AppBase: Cleaning up...");
    m_isCleanedUp = true;
    onCleanup();
    if (m_rootLayout != nullptr) { delete m_rootLayout; m_rootLayout = nullptr; }
    m_fontConfigs.clear();
    m_currentFontIndex = 0;
    LOG_APP_SUCCESS("AppBase: Cleanup complete");
}

void AppBase::onSuspend() { LOG_APP_INFO("AppBase: Suspending..."); onSuspended(); }

void AppBase::onResume()
{
    LOG_APP_INFO("AppBase: Resuming...");
    if (m_currentCanvas != nullptr && m_fontConfigs.empty() == false)
        m_currentCanvas->reloadFonts(m_fontConfigs);
    onResumed();
}

void AppBase::update(float deltaTime)
{
    if (m_cursorView != nullptr) updateCursorView(deltaTime);
    onUpdate(deltaTime);
}

void AppBase::render(int windowWidth, int windowHeight)
{
    if (m_screenWidth != windowWidth || m_screenHeight != windowHeight)
    {
        m_screenWidth  = windowWidth;
        m_screenHeight = windowHeight;
        if (m_currentCanvas != nullptr) m_currentCanvas->setScreenSize(windowWidth, windowHeight);
    }

    if (m_currentCanvas != nullptr) m_currentCanvas->begin();
    onRender();

    if (m_rootLayout != nullptr && m_currentCanvas != nullptr)
    {
        UI::MeasureSpec widthSpec  = UI::MeasureSpec::makeExactly(static_cast<float>(windowWidth));
        UI::MeasureSpec heightSpec = UI::MeasureSpec::makeExactly(static_cast<float>(windowHeight));
        m_rootLayout->onMeasure(widthSpec, heightSpec);
        m_rootLayout->onLayout(UI::RectF::fromXYWH(0, 0, windowWidth, windowHeight));
        m_rootLayout->onDraw(*m_currentCanvas);
    }

    if (m_currentCanvas != nullptr) m_currentCanvas->end();
}

void AppBase::handleKeyEvent(const IO::KeyEvent& event)
{
    if (event.action == IO::KeyAction::DOWN && event.keyCode == IO::KeyCode::Q) { requestExit(); return; }
    if (onKeyEvent(event) == true) return;
    if (m_rootLayout != nullptr) m_rootLayout->onKeyEvent(event);
}

void AppBase::handleMotionEvent(const IO::MotionEvent& event)
{
    if (m_cursorView != nullptr)      m_cursorView->updateFromMotionEvent(event);
    if (onMotionEvent(event) == true) return;
    if (m_rootLayout != nullptr)      m_rootLayout->onMotionEvent(event);
}

void AppBase::handleTerminalCommand(const std::string& command)
{
    auto synthKey = [this](IO::KeyCode kc)
    {
        IO::KeyEvent ev; ev.keyCode = kc; ev.action = IO::KeyAction::DOWN; ev.source = IO::Source::KEYBOARD;
        handleKeyEvent(ev);
    };

    if (command == "w") { synthKey(IO::KeyCode::DPAD_UP);     return; }
    if (command == "x") { synthKey(IO::KeyCode::DPAD_DOWN);   return; }
    if (command == "a") { synthKey(IO::KeyCode::DPAD_LEFT);   return; }
    if (command == "d") { synthKey(IO::KeyCode::DPAD_RIGHT);  return; }
    if (command == "s") { synthKey(IO::KeyCode::DPAD_CENTER); return; }

    if (command == "q" || command == "quit" || command == "exit")
        { LOG_APP_INFO("AppBase: Exit command received"); requestExit(); return; }

    if (command == "cv" || command == "cursor_visibility") { cycleCursorVisibilityMode(); return; }
    if (command == "cs" || command == "cursor_style")      { cycleCursorStyle();          return; }

    if (onTerminalCommand(command) == true) return;
    LOG_APP_WARNINGF("AppBase: Unrecognized command '%s'", command.c_str());
}

// ── Cursor ──────────────────────────────────────────────────────────────────

void AppBase::setupCursorView()
{
    if (m_rootLayout == nullptr) { LOG_APP_ERROR("AppBase: Cannot setup cursor — root layout is null"); return; }
    m_cursorView = new UI::CursorView();
    m_cursorView->getLayoutParams().width  = UI::MATCH_PARENT;
    m_cursorView->getLayoutParams().height = UI::MATCH_PARENT;
    m_cursorView->setVisibilityMode(UI::CursorView::VisibilityMode::SHOW_ON_MOVE);
    m_cursorView->setCursorStyle(UI::CursorView::CursorStyle::DOT_WITH_RING);
    m_cursorView->setCursorSize(20.0f);
    m_cursorView->setFadeOutDuration(2.0f);
    m_cursorView->setMouseColor(UI::Color4::fromRGBA(100, 180, 255, 200));
    m_cursorView->setTouchColor(UI::Color4::fromRGBA(255, 100, 150, 220));
    m_cursorView->setPressedColor(UI::Color4::fromRGBA(255, 200,  80, 240));
    m_rootLayout->addView(m_cursorView);
}

void AppBase::updateCursorView(float deltaTime)
{
    if (m_cursorView != nullptr) m_cursorView->update(deltaTime);
}

void AppBase::cycleCursorVisibilityMode()
{
    if (m_cursorView == nullptr) { LOG_APP_WARNING("AppBase: Cursor view not initialized"); return; }
    using VM = UI::CursorView::VisibilityMode;
    switch (m_cursorView->getVisibilityMode())
    {
        case VM::ALWAYS_VISIBLE: m_cursorView->setVisibilityMode(VM::SHOW_ON_TOUCH);  break;
        case VM::SHOW_ON_TOUCH:  m_cursorView->setVisibilityMode(VM::SHOW_ON_MOVE);   break;
        case VM::SHOW_ON_MOVE:   m_cursorView->setVisibilityMode(VM::NEVER_VISIBLE);  break;
        case VM::NEVER_VISIBLE:  m_cursorView->setVisibilityMode(VM::ALWAYS_VISIBLE); break;
        default:                 m_cursorView->setVisibilityMode(VM::ALWAYS_VISIBLE); break;
    }
}

void AppBase::cycleCursorStyle()
{
    if (m_cursorView == nullptr) { LOG_APP_WARNING("AppBase: Cursor view not initialized"); return; }
    using CS = UI::CursorView::CursorStyle;
    switch (m_cursorView->getCursorStyle())
    {
        case CS::CIRCLE:        m_cursorView->setCursorStyle(CS::RING);          break;
        case CS::RING:          m_cursorView->setCursorStyle(CS::CROSSHAIR);     break;
        case CS::CROSSHAIR:     m_cursorView->setCursorStyle(CS::DOT_WITH_RING); break;
        case CS::DOT_WITH_RING: m_cursorView->setCursorStyle(CS::RIPPLE);        break;
        case CS::RIPPLE:        m_cursorView->setCursorStyle(CS::CIRCLE);        break;
        default:                m_cursorView->setCursorStyle(CS::CIRCLE);        break;
    }
}

std::vector<UI::FontConfig> AppBase::createFontConfigs()
{
    return { UI::FontConfig("kr", "NotoSansKR-Bold", std::string(_FONTS_PATH_) + "/NotoSansKR-Bold.ttf",
                            60.0f, UI::CharacterSet::KOREAN, 2048, 2048) };
}

} // namespace APP
