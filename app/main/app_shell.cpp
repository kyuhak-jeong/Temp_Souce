#include "app_shell.h"
#include "app_vars.h"
#include "logger.h"
#include "io_platform.h"
#include "ui_locale.h"
#include <ctime>
#include <cstdio>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <algorithm>

#ifdef USE_DVR
    extern void onChannelCountChanged(int newChannelCount);
#endif

namespace APP
{

// ============================================================================
// StatusBar
// ============================================================================

StatusBar::StatusBar()
    : UI::FrameLayout()
    , m_barVisible(true)
    , m_togglePanel(nullptr)
    , m_toggleButton(nullptr)
    , m_innerBar(nullptr)
    , m_logoView(nullptr)
    , m_statusArea(nullptr)
    , m_menuButton(nullptr)
{
    getLayoutParams().width   = UI::MATCH_PARENT;
    getLayoutParams().height  = HEIGHT;
    getLayoutParams().gravity = UI::Gravity::BOTTOM;
    setBackgroundColor(UI::Color::Transparent);

    // ── Inner bar — added first (lower z), fills the container ────────────
    m_innerBar = new UI::FrameLayout();
    m_innerBar->getLayoutParams().width   = UI::MATCH_PARENT;
    m_innerBar->getLayoutParams().height  = UI::MATCH_PARENT;
    m_innerBar->setBackgroundColor(UI::Color::DarkBackground.withAlpha(0.92f));

    // Left panel: logo
    m_logoView = new UI::ImageView();
    m_logoView->getLayoutParams().width   = UI::WRAP_CONTENT;
    m_logoView->getLayoutParams().height  = UI::WRAP_CONTENT;
    m_logoView->getLayoutParams().gravity = UI::Gravity::LEFT | UI::Gravity::CENTER_VERTICAL;
    m_logoView->getLayoutParams().setMargin(10.0f);
    m_innerBar->addView(m_logoView);

    // Right panel: status slots + menu button
    UI::LinearLayout* rightPanel = new UI::LinearLayout();
    rightPanel->setOrientation(UI::Orientation::HORIZONTAL);
    rightPanel->getLayoutParams().width   = UI::WRAP_CONTENT;
    rightPanel->getLayoutParams().height  = UI::MATCH_PARENT;
    rightPanel->getLayoutParams().gravity = UI::Gravity::RIGHT | UI::Gravity::CENTER_VERTICAL;
    rightPanel->setGravity(UI::Gravity::CENTER_VERTICAL);
    rightPanel->getLayoutParams().setMargin(0.0f, 0.0f, static_cast<float>(HEIGHT) + 16.0f, 0.0f);
    m_innerBar->addView(rightPanel);

    m_statusArea = new UI::LinearLayout();
    m_statusArea->setOrientation(UI::Orientation::HORIZONTAL);
    m_statusArea->getLayoutParams().width   = UI::WRAP_CONTENT;
    m_statusArea->getLayoutParams().height  = UI::MATCH_PARENT;
    m_statusArea->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
    m_statusArea->setGravity(UI::Gravity::CENTER_VERTICAL);
    m_statusArea->getLayoutParams().setMargin(0.0f, 0.0f, 16.0f, 0.0f);
    rightPanel->addView(m_statusArea);

    m_menuButton = new UI::Button();
    m_menuButton->getLayoutParams().width   = 210;
    m_menuButton->getLayoutParams().height  = UI::WRAP_CONTENT;
    m_menuButton->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
    m_menuButton->getLayoutParams().setMargin(0.0f, 0.0f, 16.0f, 0.0f);
    m_menuButton->setNormalColor(UI::Color::CardBackground);
    m_menuButton->setHoverColor(UI::Color::CardBackgroundHover);
    m_menuButton->setFocusedColor(UI::Color::CardBackground);
    m_menuButton->setPressedColor(UI::Color::CardBackgroundHover);
    m_menuButton->setBorderWidth(4.0f);
    m_menuButton->setCornerRadius(20.0f);
    m_menuButton->setTextSize(48.0f);
    m_menuButton->setTextColor(UI::Color::White);
    m_menuButton->setContentOrientation(UI::Orientation::HORIZONTAL);
    m_menuButton->setContentSpacing(10.0f);
    m_menuButton->setContentGravity(UI::Gravity::CENTER | UI::Gravity::CENTER_VERTICAL);
    m_menuButton->setFocusable(true);
    rightPanel->addView(m_menuButton);

    addView(m_innerBar);

    // ── Toggle button — inside a transparent HEIGHT-tall container ────────────
    m_togglePanel = new UI::FrameLayout();
    m_togglePanel->getLayoutParams().width   = HEIGHT;
    m_togglePanel->getLayoutParams().height  = HEIGHT;
    m_togglePanel->getLayoutParams().gravity = UI::Gravity::BOTTOM | UI::Gravity::RIGHT;
    m_togglePanel->getLayoutParams().setMargin(0.0f, 0.0f, 16.0f, 0.0f);
    m_togglePanel->setBackgroundColor(UI::Color::Transparent);

    m_toggleButton = new UI::Button();
    m_toggleButton->getLayoutParams().width   = HEIGHT;
    m_toggleButton->getLayoutParams().height  = UI::WRAP_CONTENT;
    m_toggleButton->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
    m_toggleButton->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
    m_toggleButton->setNormalColor(UI::Color::CardBackground);
    m_toggleButton->setHoverColor(UI::Color::CardBackgroundHover);
    m_toggleButton->setPressedColor(UI::Color::CardBackgroundHover);
    m_toggleButton->setFocusable(true);
    m_toggleButton->setBorderWidth(4.0f);
    m_toggleButton->setCornerRadius(20.0f);
    m_toggleButton->setText("▼");
    m_toggleButton->setTextSize(48.0f);
    m_toggleButton->setTextColor(UI::Color::White);
    m_toggleButton->setOnClickListener([this](UI::View*) { toggleBarVisible(); });
    m_togglePanel->addView(m_toggleButton);
}

void StatusBar::setBarVisible(bool visible)
{
    if (m_barVisible == visible) return;
    m_barVisible = visible;
    if (m_toggleButton != nullptr) m_toggleButton->setText(m_barVisible ? "▼" : "▲");
    if (m_innerBar     != nullptr) m_innerBar->setVisibility(m_barVisible ? UI::Visibility::VISIBLE
                                                                           : UI::Visibility::GONE);
    getLayoutParams().height = m_barVisible ? HEIGHT : 0;
    if (m_onVisibilityChanged) m_onVisibilityChanged(m_barVisible);
}

void StatusBar::setLogoTexture(UI::Texture* tex)
{
    if (m_logoView != nullptr) m_logoView->setTexture(tex);
}

void StatusBar::setupMenuButton(UI::Texture* iconTex)
{
    if (m_menuButton == nullptr || iconTex == nullptr) return;
    m_menuButton->setTexture(iconTex);
    m_menuButton->setImageSize(static_cast<float>(iconTex->width), static_cast<float>(iconTex->height));
}

UI::TextView* StatusBar::addTextSlot(float textSize)
{
    if (m_statusArea == nullptr) return nullptr;
    UI::TextView* tv = new UI::TextView();
    tv->getLayoutParams().width   = UI::WRAP_CONTENT;
    tv->getLayoutParams().height  = UI::WRAP_CONTENT;
    tv->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
    tv->setTextSize(textSize);
    tv->setTextColor(UI::Color::TextPrimary);
    tv->setBackgroundColor(UI::Color::Transparent);
    m_statusArea->addView(tv);
    return tv;
}

UI::ImageView* StatusBar::addImageSlot(int width, int height)
{
    if (m_statusArea == nullptr) return nullptr;
    UI::ImageView* iv = new UI::ImageView();
    iv->getLayoutParams().width   = width;
    iv->getLayoutParams().height  = height;
    iv->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
    m_statusArea->addView(iv);
    return iv;
}

void StatusBar::addDivider()
{
    if (m_statusArea == nullptr) return;
    UI::View* div = new UI::View();
    div->getLayoutParams().width   = 2;
    div->getLayoutParams().height  = 36;
    div->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
    div->getLayoutParams().setMargin(8.0f, 0.0f, 8.0f, 0.0f);
    div->setBackgroundColor(UI::Color4::fromRGBA(255, 255, 255, 30));
    m_statusArea->addView(div);
}

// ============================================================================
// Static helpers
// ============================================================================

std::string AppShell::pad2(int v) { char b[8];  snprintf(b, sizeof(b), "%02d", v); return b; }
std::string AppShell::pad4(int v) { char b[16]; snprintf(b, sizeof(b), "%04d", v); return b; }

void AppShell::syncSystemTimeFromOS(SystemTime& st)
{
    time_t now    = time(nullptr);
    struct tm* lt = localtime(&now);
    if (lt == nullptr) return;
    st.year   = lt->tm_year + 1900;
    st.month  = lt->tm_mon  + 1;
    st.day    = lt->tm_mday;
    st.hour   = lt->tm_hour;
    st.minute = lt->tm_min;
    st.second = lt->tm_sec;
}

void AppShell::applySystemTime(const SystemTime& st)
{
    struct tm t = {};
    t.tm_year  = st.year  - 1900;
    t.tm_mon   = st.month - 1;
    t.tm_mday  = st.day;
    t.tm_hour  = st.hour;
    t.tm_min   = st.minute;
    t.tm_sec   = st.second;
    t.tm_isdst = -1;
    time_t epoch = mktime(&t);
    if (epoch == (time_t)-1) return;
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

AppShell::AppShell()
    : AppBase()
    , m_mainScreen(nullptr)
    , m_menuScreen(nullptr)
    , m_statusBar(nullptr)
    , m_barBleSlot(nullptr)
    , m_barDateTimeSlot(nullptr)
    #ifdef USE_DVR
        , m_barRecSlot(nullptr)
        , m_barStorageUsageSlot(nullptr)
        , m_storageUpdateTimer(0.0f)
    #endif
    #ifdef USE_CAN
        , m_barCanSpeedSlot(nullptr)
        , m_barCanRpmSteerSlot(nullptr)
        , m_barCanGearSlot(nullptr)
        , m_barCanSignalLeftSlot(nullptr)
        , m_barCanSignalRightSlot(nullptr)
    #endif
    , m_menuLogoView(nullptr)
    , m_menuControlsBar(nullptr)
    , m_hintTextView(nullptr)
    , m_ctrlBtnUp(nullptr)
    , m_ctrlBtnDown(nullptr)
    , m_ctrlBtnLeft(nullptr)
    , m_ctrlBtnRight(nullptr)
    , m_ctrlBtnOk(nullptr)
    , m_menuTitleIconView(nullptr)
    , m_menuTitleTextView(nullptr)
    , m_menuContentLayout(nullptr)
    , m_menuBackButton(nullptr)
    , m_menuSaveButton(nullptr)
    , m_topBarFocus(TopBarFocus::NONE)
    , m_popupView(nullptr)
    , m_numpadPopup(nullptr)
    , m_menuVisible(false)
    , m_fileViewEntered(false)
    , m_adjustmentType(AdjustmentType::AdjNone)
    , m_adjustmentSteps(0)
    , m_adjustmentStepIndex(0)
    , m_timeFieldIndex(0)
    , m_timeRowItem(nullptr)
    , m_timeMenuParent(nullptr)
    , m_timePillsNeedUpdate(false)
    , m_configBackup()
    , m_passwordIsSet(false)
    , m_numpadPendingShow(false)
    , m_numpadPendingTitle("")
    , m_pendingLangChange("")
    , m_pendingPwMsg("")
    , m_numpadPendingConfirm(nullptr)
    , m_numpadPendingCancel(nullptr)
    , m_popupType(PopupType::PopupNone)
    , m_popupVisible(false)
    #ifdef USE_DVR
        , m_fileViewSelectedGroupId(0)
        , m_fileViewSelectedFileId(-1)
        , m_fileViewGroupMaxSize(10)
        , m_fileViewSelectedFileName("")
    #endif
{}

AppShell::~AppShell() {}

// ============================================================================
// AppBase lifecycle overrides
// ============================================================================

bool AppShell::onInitialize()
{
    LOG_APP_SECTION("AppShell: Initializing");

    loadConfigValues();
    loadTextures();
    loadLocale();

    m_rootLayout = new UI::FrameLayout();
    m_rootLayout->getLayoutParams().width  = UI::MATCH_PARENT;
    m_rootLayout->getLayoutParams().height = UI::MATCH_PARENT;

    // ── Main screen ────────────────────────────────────────────────────────
    m_mainScreen = new UI::FrameLayout();
    m_mainScreen->getLayoutParams().width   = UI::MATCH_PARENT;
    m_mainScreen->getLayoutParams().height  = UI::MATCH_PARENT;
    m_mainScreen->getLayoutParams().gravity = UI::Gravity::TOP;
    m_mainScreen->getLayoutParams().setPadding(0.0f, 0.0f, 0.0f, static_cast<float>(StatusBar::HEIGHT));
    m_mainScreen->setBackgroundColor(UI::Color::DarkBackground);
    m_rootLayout->addView(m_mainScreen);

    setupStatusBar(); // must come before setupMainScreenContent so getStatusBarHeight() is valid when rebuildLayout runs
    setupMainScreenContent();
    setupMenuScreen();
    setupMenuPopup();
    setupCursorView();
    initializeMenuSystem();
    showMainScreen();

    #ifdef USE_DVR
        m_fileManager.setExtensionFilter({"mp4", "avi", "mkv"});
        m_filePlayer.initialize();
        m_filePlayer.setStateChangeCallback([this](VPB::PlaybackState o, VPB::PlaybackState n)
            { onPlaybackStateChanged(o, n); });
    #endif

    LOG_APP_SUCCESS("AppShell: Initialized successfully");
    return true;
}

void AppShell::onCleanup()
{
    LOG_APP_INFO("AppShell: Cleaning up...");
    if (m_menuNavigator != nullptr) { m_menuNavigator->cleanup(); m_menuNavigator.reset(); }
    #ifdef USE_DVR
        m_filePlayer.shutdown();
    #endif
    UI::TextureManager::getInstance().clearAll();
    UI::LocalizationManager::getInstance().clearAll();
    m_menuRoot.reset();
}

void AppShell::onStatusBarCreated()
{
    StatusBar* bar = getStatusBar();
    if (bar == nullptr) return;

    // Wire logo
    UI::Texture* logoTex = UI::TextureManager::getInstance().getTextureByName("04_logo_company");
    bar->setLogoTexture(logoTex);

    // Wire menu button text, icon, and click handler
    UI::Button* menuBtn = bar->getMenuButton();
    if (menuBtn != nullptr)
    {
        SET_LOCALIZED_TEXT(menuBtn, "Menu");
        UI::Texture* menuIconTex = UI::TextureManager::getInstance().getTextureByName("03_icn_ctrl_menu");
        bar->setupMenuButton(menuIconTex);
        menuBtn->setOnClickListener([this](UI::View*) { showMenuScreen(); });
    }

    // ── Status bar slot order (left → right): ─────────────────────────────
    //   [CAN: speed | rpm+steer | left-ind gear right-ind] | [REC + storage%] | [BLE] | [datetime]
    #ifdef USE_CAN
    {
        UI::LinearLayout* canGroup = new UI::LinearLayout();
        canGroup->setOrientation(UI::Orientation::HORIZONTAL);
        canGroup->getLayoutParams().width   = UI::WRAP_CONTENT;
        canGroup->getLayoutParams().height  = UI::MATCH_PARENT;
        canGroup->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        canGroup->getLayoutParams().setMargin(0.0f, 0.0f, 2.0f, 0.0f);
        canGroup->setGravity(UI::Gravity::CENTER_VERTICAL);
        canGroup->setSpacing(10.0f);
        bar->getStatusArea()->addView(canGroup);

        // ── Block 1: Speed ─────────────────────────────────────────────────
        m_barCanSpeedSlot = new UI::TextView();
        m_barCanSpeedSlot->getLayoutParams().width   = UI::WRAP_CONTENT;
        m_barCanSpeedSlot->getLayoutParams().height  = UI::WRAP_CONTENT;
        m_barCanSpeedSlot->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        m_barCanSpeedSlot->setTextSize(60.0f);
        m_barCanSpeedSlot->setTextColor(UI::Color4::fromRGBA(80, 220, 120, 255));
        m_barCanSpeedSlot->setBackgroundColor(UI::Color::Transparent);
        m_barCanSpeedSlot->setText("-- km/h");
        canGroup->addView(m_barCanSpeedSlot);

        // ── Block 2: RPM + Steering (2-line, smaller, no labels) ──────────
        UI::LinearLayout* rpmSteerBlock = new UI::LinearLayout();
        rpmSteerBlock->setOrientation(UI::Orientation::VERTICAL);
        rpmSteerBlock->getLayoutParams().width   = UI::WRAP_CONTENT;
        rpmSteerBlock->getLayoutParams().height  = UI::WRAP_CONTENT;
        rpmSteerBlock->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        rpmSteerBlock->setGravity(UI::Gravity::CENTER_HORIZONTAL);
        rpmSteerBlock->getLayoutParams().setMargin(0.0f, 0.0f, 16.0f, 0.0f);
        canGroup->addView(rpmSteerBlock);

        m_barCanRpmSteerSlot = new UI::TextView();
        m_barCanRpmSteerSlot->getLayoutParams().width   = UI::WRAP_CONTENT;
        m_barCanRpmSteerSlot->getLayoutParams().height  = UI::WRAP_CONTENT;
        m_barCanRpmSteerSlot->getLayoutParams().gravity = UI::Gravity::CENTER_HORIZONTAL;
        m_barCanRpmSteerSlot->setTextSize(30.0f);
        m_barCanRpmSteerSlot->setTextColor(UI::Color::TextSecondary);
        m_barCanRpmSteerSlot->setBackgroundColor(UI::Color::Transparent);
        m_barCanRpmSteerSlot->setText("---- rpm\n---deg");
        rpmSteerBlock->addView(m_barCanRpmSteerSlot);

        // ── Block 3: [left-indicator] [gear icon] [right-indicator] ───────
        UI::LinearLayout* gearBlock = new UI::LinearLayout();
        gearBlock->setOrientation(UI::Orientation::HORIZONTAL);
        gearBlock->getLayoutParams().width   = UI::WRAP_CONTENT;
        gearBlock->getLayoutParams().height  = UI::MATCH_PARENT;
        gearBlock->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        gearBlock->setGravity(UI::Gravity::CENTER_VERTICAL);
        gearBlock->getLayoutParams().setMargin(0.0f, 0.0f, 4.0f, 0.0f);
        canGroup->addView(gearBlock);

        m_barCanSignalLeftSlot = new UI::ImageView();
        m_barCanSignalLeftSlot->getLayoutParams().width   = 36;
        m_barCanSignalLeftSlot->getLayoutParams().height  = 58;
        m_barCanSignalLeftSlot->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        m_barCanSignalLeftSlot->getLayoutParams().setMargin(0.0f, 0.0f, 6.0f, 0.0f);
        gearBlock->addView(m_barCanSignalLeftSlot);

        m_barCanGearSlot = new UI::ImageView();
        m_barCanGearSlot->getLayoutParams().width   = UI::WRAP_CONTENT;
        m_barCanGearSlot->getLayoutParams().height  = 58;
        m_barCanGearSlot->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        gearBlock->addView(m_barCanGearSlot);

        m_barCanSignalRightSlot = new UI::ImageView();
        m_barCanSignalRightSlot->getLayoutParams().width   = 36;
        m_barCanSignalRightSlot->getLayoutParams().height  = 58;
        m_barCanSignalRightSlot->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        m_barCanSignalRightSlot->getLayoutParams().setMargin(6.0f, 0.0f, 0.0f, 0.0f);
        gearBlock->addView(m_barCanSignalRightSlot);

        bar->addDivider();
    }
    #endif

    #ifdef USE_DVR
    {
        UI::LinearLayout* dvrGroup = new UI::LinearLayout();
        dvrGroup->setOrientation(UI::Orientation::HORIZONTAL);
        dvrGroup->getLayoutParams().width   = UI::WRAP_CONTENT;
        dvrGroup->getLayoutParams().height  = UI::MATCH_PARENT;
        dvrGroup->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        dvrGroup->setGravity(UI::Gravity::CENTER_VERTICAL);
        dvrGroup->getLayoutParams().setMargin(0.0f, 0.0f, 8.0f, 0.0f);
        bar->getStatusArea()->addView(dvrGroup);

        m_barRecSlot = new UI::ImageView();
        m_barRecSlot->getLayoutParams().width   = 48;
        m_barRecSlot->getLayoutParams().height  = 48;
        m_barRecSlot->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        m_barRecSlot->getLayoutParams().setMargin(0.0f, 0.0f, 10.0f, 0.0f);
        dvrGroup->addView(m_barRecSlot);

        m_barStorageUsageSlot = new UI::TextView();
        m_barStorageUsageSlot->getLayoutParams().width   = UI::WRAP_CONTENT;
        m_barStorageUsageSlot->getLayoutParams().height  = UI::WRAP_CONTENT;
        m_barStorageUsageSlot->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        m_barStorageUsageSlot->setTextSize(48.0f);
        m_barStorageUsageSlot->setTextColor(UI::Color::TextPrimary);
        m_barStorageUsageSlot->setBackgroundColor(UI::Color::Transparent);
        m_barStorageUsageSlot->setText("--%");
        dvrGroup->addView(m_barStorageUsageSlot);

        bar->addDivider();
    }
    #endif

    m_barBleSlot = bar->addImageSlot(48, 48);

    bar->addDivider();

    {
        UI::LinearLayout* sysBlock = new UI::LinearLayout();
        sysBlock->setOrientation(UI::Orientation::VERTICAL);
        sysBlock->getLayoutParams().width   = UI::WRAP_CONTENT;
        sysBlock->getLayoutParams().height  = UI::MATCH_PARENT;
        sysBlock->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        sysBlock->setGravity(UI::Gravity::CENTER_HORIZONTAL);
        bar->getStatusArea()->addView(sysBlock);

        m_barCpuSlot = new UI::TextView();
        m_barCpuSlot->getLayoutParams().width   = UI::WRAP_CONTENT;
        m_barCpuSlot->getLayoutParams().height  = UI::WRAP_CONTENT;
        m_barCpuSlot->getLayoutParams().gravity = UI::Gravity::CENTER_HORIZONTAL;
        m_barCpuSlot->setTextSize(30.0f);
        m_barCpuSlot->setTextColor(UI::Color::TextSecondary);
        m_barCpuSlot->setBackgroundColor(UI::Color::Transparent);
        m_barCpuSlot->setText("--%");
        sysBlock->addView(m_barCpuSlot);

        m_barMemSlot = new UI::TextView();
        m_barMemSlot->getLayoutParams().width   = UI::WRAP_CONTENT;
        m_barMemSlot->getLayoutParams().height  = UI::WRAP_CONTENT;
        m_barMemSlot->getLayoutParams().gravity = UI::Gravity::CENTER_HORIZONTAL;
        m_barMemSlot->setTextSize(30.0f);
        m_barMemSlot->setTextColor(UI::Color::TextSecondary);
        m_barMemSlot->setBackgroundColor(UI::Color::Transparent);
        m_barMemSlot->setText("---M");
        sysBlock->addView(m_barMemSlot);
    }

    bar->addDivider();

    m_barDateTimeSlot = bar->addTextSlot(48.0f);
    m_barDateTimeSlot->setTextColor(UI::Color::TextPrimary);
    m_barDateTimeSlot->setTextGravity(UI::Gravity::CENTER_VERTICAL);
    {
        const auto& st = APP::appConf.system_time;
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d",
                 st.year, st.month, st.day, st.hour, st.minute, st.second);
        m_barDateTimeSlot->setText(buf);
    }
}

void AppShell::onUpdate(float deltaTime)
{
    // ── Common status bar slot updates ─────────────────────────────────────
    if (m_barBleSlot != nullptr)
    {
        const char* bleTexName = APP::ble_connect
            ? "03_icn_remocon_connected" : "03_icn_remocon_disconnected";
        UI::Texture* bleTex = UI::TextureManager::getInstance().getTextureByName(bleTexName);
        if (bleTex != nullptr) m_barBleSlot->setTexture(bleTex);
    }
    #ifdef USE_DVR
        if (m_barRecSlot != nullptr)
        {
            bool isRecording = (APP::dvrConf.recordingChannel > 0);
            const char* recTexName = isRecording ? "03_icn_rec_active" : "03_icn_rec_inactive";
            UI::Texture* recTex = UI::TextureManager::getInstance().getTextureByName(recTexName);
            if (recTex != nullptr) m_barRecSlot->setTexture(recTex);
        }
        if (m_barStorageUsageSlot != nullptr)
        {
            if (APP::storage_connected == true && APP::storage_total_mb > 0)
            {
                char usageBuf[16];
                float pct = static_cast<float>(APP::storage_used_mb * 100.0f / APP::storage_total_mb);
                snprintf(usageBuf, sizeof(usageBuf), "%.1f%%", pct);
                m_barStorageUsageSlot->setText(usageBuf);
            }
            else
            {
                m_barStorageUsageSlot->setText("--%");
            }
        }
        // ── Storage stats poll (every 5 seconds) ──────────────────────────
        m_storageUpdateTimer += deltaTime;
        if (m_storageUpdateTimer >= 5.0f)
        {
            m_storageUpdateTimer = 0.0f;
            if (APP::storage_connected == true)
            {
                struct statvfs vfsStat;
                if (statvfs(APP::storage_path.c_str(), &vfsStat) == 0)
                {
                    long blockSize        = static_cast<long>(vfsStat.f_frsize);
                    APP::storage_total_mb = (blockSize * static_cast<long>(vfsStat.f_blocks)) / static_cast<long>(MB_TO_BYTE);
                    long free_mb          = (blockSize * static_cast<long>(vfsStat.f_bfree))  / static_cast<long>(MB_TO_BYTE);
                    APP::storage_used_mb  = APP::storage_total_mb - free_mb;
                }
            }
            else
            {
                APP::storage_used_mb  = 0;
                APP::storage_total_mb = 0;
            }
        }
    #endif

    // ── CPU usage — SafeView process only (every 2 seconds) ──────────────
    if (m_cpuClkTck == 0)
        m_cpuClkTck = sysconf(_SC_CLK_TCK);

    m_cpuUpdateTimer += deltaTime;
    if (m_cpuUpdateTimer >= 2.0f)
    {
        float elapsed    = m_cpuUpdateTimer;
        m_cpuUpdateTimer = 0.0f;

        // /proc/self/stat: pid (comm) state ppid ... utime(14) stime(15)
        // comm may contain spaces, so find last ')' and parse from there.
        FILE* f = fopen("/proc/self/stat", "r");
        if (f != nullptr)
        {
            char line[512] = {};
            unsigned long utime = 0, stime = 0;
            if (fgets(line, sizeof(line), f) != nullptr)
            {
                char* p = strrchr(line, ')');
                if (p != nullptr)
                {
                    char state;
                    long ppid, pgrp, sess, tty, tpgid;
                    unsigned long flags, minflt, cminflt, majflt, cmajflt;
                    sscanf(p + 2,
                           "%c %ld %ld %ld %ld %ld"
                           " %lu %lu %lu %lu %lu"
                           " %lu %lu",
                           &state, &ppid, &pgrp, &sess, &tty, &tpgid,
                           &flags, &minflt, &cminflt, &majflt, &cmajflt,
                           &utime, &stime);
                }
            }
            fclose(f);

            unsigned long procTicks = utime + stime;
            unsigned long dTicks    = procTicks - m_cpuPrevProcTicks;
            m_cpuPrevProcTicks      = procTicks;

            if (m_cpuClkTck > 0 && elapsed > 0.0f && m_barCpuSlot != nullptr)
            {
                // 100% = one full core (same scale as top per-process %)
                int pct = static_cast<int>(100.0f * dTicks / (elapsed * m_cpuClkTck));
                char buf[16];
                snprintf(buf, sizeof(buf), "%d%%", pct);
                m_barCpuSlot->setText(buf);
            }
        }

        // Memory used (MemTotal - MemAvailable), matches top's "used"
        if (m_barMemSlot != nullptr)
        {
            FILE* fm = fopen("/proc/meminfo", "r");
            if (fm != nullptr)
            {
                long totalKb = 0, availKb = 0;
                char key[64];
                long val;
                while (fscanf(fm, "%63s %ld kB\n", key, &val) == 2)
                {
                    if      (strcmp(key, "MemTotal:")     == 0) totalKb = val;
                    else if (strcmp(key, "MemAvailable:") == 0) availKb = val;
                    if (totalKb > 0 && availKb > 0) break;
                }
                fclose(fm);
                if (totalKb > 0)
                {
                    char buf[20];
                    snprintf(buf, sizeof(buf), "%ldM", (totalKb - availKb) / 1024);
                    m_barMemSlot->setText(buf);
                }
            }
        }
    }

    #ifdef USE_CAN
    {
        const OBDData obd = APP::g_obd.read();

        if (m_barCanSpeedSlot != nullptr)
        {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d km/h", obd.speedKmh);
            m_barCanSpeedSlot->setText(buf);
        }
        if (m_barCanRpmSteerSlot != nullptr)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%4d rpm\n%4d deg", obd.rpm, obd.steeringAngle);
            m_barCanRpmSteerSlot->setText(buf);
        }
        if (m_barCanGearSlot != nullptr)
        {
            const char* gearTex = "03_icn_gear_n";
            switch (obd.gearPos)
            {
                case GEAR::GEAR_DRIVING: gearTex = "03_icn_gear_d"; break;
                case GEAR::GEAR_REVERSE: gearTex = "03_icn_gear_r"; break;
                case GEAR::GEAR_NEUTRAL: gearTex = "03_icn_gear_n"; break;
                case GEAR::GEAR_PARKING: gearTex = "03_icn_gear_p"; break;
                default:                 gearTex = "03_icn_gear_n"; break;
            }
            UI::Texture* tex = UI::TextureManager::getInstance().getTextureByName(gearTex);
            if (tex != nullptr) m_barCanGearSlot->setTexture(tex);
        }
        if (m_barCanSignalLeftSlot != nullptr)
        {
            bool leftOn = (obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_LEFT ||
                           obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_EMERGENCY);
            const char* texName = leftOn ? "03_icn_turn_left_on" : "03_icn_turn_left_off";
            UI::Texture* tex = UI::TextureManager::getInstance().getTextureByName(texName);
            if (tex != nullptr) m_barCanSignalLeftSlot->setTexture(tex);
        }
        if (m_barCanSignalRightSlot != nullptr)
        {
            bool rightOn = (obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_RIGHT ||
                            obd.turnSignal == TURN_SIGNAL::TURN_SIGNAL_EMERGENCY);
            const char* texName = rightOn ? "03_icn_turn_right_on" : "03_icn_turn_right_off";
            UI::Texture* tex = UI::TextureManager::getInstance().getTextureByName(texName);
            if (tex != nullptr) m_barCanSignalRightSlot->setTexture(tex);
        }
    }
    #endif
    if (m_barDateTimeSlot != nullptr)
    {
        time_t now    = time(nullptr);
        struct tm* lt = localtime(&now);
        if (lt != nullptr)
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %02d:%02d:%02d",
                     lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday,
                     lt->tm_hour, lt->tm_min, lt->tm_sec);
            m_barDateTimeSlot->setText(buf);
        }
    }

    // ── Deferred numpad show ───────────────────────────────────────────────
    if (m_numpadPendingShow == true)
    {
        m_numpadPendingShow = false;
        if (m_numpadPopup != nullptr)
        {
            if (m_numpadPendingConfirm != nullptr)
                m_numpadPopup->setOnConfirm([this, cb = std::move(m_numpadPendingConfirm)](const std::string& v)
                    { cb(stringToPasswordDigits(v)); });
            if (m_numpadPendingCancel != nullptr)
                m_numpadPopup->setOnCancel(std::move(m_numpadPendingCancel));
            m_numpadPopup->setCancelText(TR("Cancel"));
            m_numpadPopup->setConfirmText(TR("Confirm"));
            m_numpadPopup->show(m_numpadPendingTitle, "", false);
        }
    }

    // ── Deferred password popup ────────────────────────────────────────────
    if (m_pendingPwMsg.empty() == false)
    {
        showPwPopup(std::move(m_pendingPwMsg));
        m_pendingPwMsg.clear();
    }

    // ── Deferred language change ───────────────────────────────────────────
    if (m_pendingLangChange.empty() == false)
    {
        std::string lang = std::move(m_pendingLangChange);
        m_pendingLangChange.clear();
        UI::LocalizationManager::getInstance().setLanguage(lang);
        if (m_menuNavigator != nullptr)
        {
            UI::MenuItem* current = m_menuNavigator->getCurrentItem();
            if (current != nullptr)
            {
                int savedIdx = m_menuNavigator->getSubMenuSelectedIndex();
                m_menuNavigator->showSubMenu(current);
                m_menuNavigator->setSubMenuSelectedIndex(savedIdx);
            }
        }
        updateMenuDisplay();
    }

    updateCtrlBtnRepeats(deltaTime);

    if (m_menuVisible == true && m_menuNavigator != nullptr)
        m_menuNavigator->update(deltaTime);

    if (m_popupVisible == true && m_popupView != nullptr)
        m_popupView->update(deltaTime);

    const bool timeMenuActive = m_menuVisible
                             && m_menuNavigator  != nullptr
                             && m_timeMenuParent != nullptr
                             && m_menuNavigator->getCurrentItem() == m_timeMenuParent;

    if (m_timePillsNeedUpdate.exchange(false) == true && timeMenuActive == true)
        m_menuNavigator->refreshSubMenuContent();

    // ── Sync OS clock every second ─────────────────────────────────────────
    {
        static float clockAccum = 0.0f;
        clockAccum += deltaTime;
        if (clockAccum >= 1.0f)
        {
            clockAccum = 0.0f;
            time_t now    = time(nullptr);
            struct tm* lt = localtime(&now);
            if (lt != nullptr)
            {
                if (m_adjustmentType != AdjustmentType::DateTime)
                    syncSystemTimeFromOS(APP::appConf.system_time);

                if (timeMenuActive == true && m_timeRowItem != nullptr
                    && m_adjustmentType != AdjustmentType::DateTime)
                {
                    const auto& ch = m_timeRowItem->getChildren();
                    const auto& st = APP::appConf.system_time;
                    const struct { int idx; std::string name; } updates[] = {
                        {1, pad4(st.year)}, {2, pad2(st.month)},  {3, pad2(st.day)},
                        {4, pad2(st.hour)}, {5, pad2(st.minute)}, {6, pad2(st.second)},
                    };
                    for (const auto& u : updates)
                        if (u.idx < static_cast<int>(ch.size()) && ch[u.idx] != nullptr)
                            ch[u.idx]->setName(u.name);
                    m_menuNavigator->refreshSubMenuContent();
                }
            }
        }
    }

    #ifdef USE_DVR
        if (m_fileViewEntered == true)
        {
            m_filePlayer.update(deltaTime);
            updateFilePlayerTexture();
        }
    #endif
}

void AppShell::onRender() {}

bool AppShell::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE)
        return false;

    if (event.keyCode == IO::KeyCode::M || event.keyCode == IO::KeyCode::MENU)
    {
        if (m_menuVisible == true) closeMenu(false);
        else                       showMenuScreen();
        return true;
    }

    if (m_popupVisible == true)
    {
        if (m_popupView != nullptr) m_popupView->handleKeyEvent(event);
        return true;
    }
    if (m_numpadPopup != nullptr && m_numpadPopup->isShowing() == true)
        { m_numpadPopup->handleKeyEvent(event); return true; }

    if (m_adjustmentType == AdjustmentType::DateTime)
    {
        if (event.keyCode == IO::KeyCode::ESCAPE)
        {
            cancelAdjustment();
            return true;
        }
        if (event.keyCode == IO::KeyCode::DPAD_LEFT)  { adjustDateTime(-1,  0); return true; }
        if (event.keyCode == IO::KeyCode::DPAD_RIGHT) { adjustDateTime( 1,  0); return true; }
        if (event.keyCode == IO::KeyCode::DPAD_UP)    { adjustDateTime( 0,  1); return true; }
        if (event.keyCode == IO::KeyCode::DPAD_DOWN)  { adjustDateTime( 0, -1); return true; }
        if (event.keyCode == IO::KeyCode::ENTER      ||
            event.keyCode == IO::KeyCode::SPACE       ||
            event.keyCode == IO::KeyCode::DPAD_CENTER)
        {
            if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
            confirmAdjustment();
            if (APP::appConf.system_time.auto_time == false)
                applySystemTime(APP::appConf.system_time);
            return true;
        }
        return true;
    }

    if (m_menuVisible == true) return handleMenuKeyEvent(event);

    if (m_focusManager.handleKeyEvent(event) == true) return true;

    if (event.keyCode == IO::KeyCode::DPAD_CENTER ||
        event.keyCode == IO::KeyCode::ENTER       ||
        event.keyCode == IO::KeyCode::SPACE)
    {
        UI::View* focused    = m_focusManager.getFocusedView();
        UI::Button* menuBtn  = (m_statusBar != nullptr) ? m_statusBar->getMenuButton()   : nullptr;
        UI::Button* toggleBtn= (m_statusBar != nullptr) ? m_statusBar->getToggleButton() : nullptr;
        if (focused != nullptr && focused == menuBtn)   { showMenuScreen();                return true; }
        if (focused != nullptr && focused == toggleBtn) { m_statusBar->toggleBarVisible(); return true; }
    }
    return false;
}

bool AppShell::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_menuVisible == true) return handleMenuMotionEvent(event);

    if (m_popupVisible == true)
    {
        if (m_popupView != nullptr) m_popupView->handleMotionEvent(event);
        return true;
    }
    if (m_numpadPopup != nullptr && m_numpadPopup->isShowing() == true)
        { m_numpadPopup->handleMotionEvent(event); return true; }

    m_focusManager.handleMotionEvent(event);
    return false;
}

bool AppShell::onTerminalCommand(const std::string& command)
{
    if (command == "m" || command == "M" || command == "menu")
    {
        if (m_menuVisible == true) closeMenu(false);
        else                       showMenuScreen();
        return true;
    }
    if (command == "tb" || command == "togglebar")
        { if (m_statusBar != nullptr) m_statusBar->toggleBarVisible(); return true; }
    if (command == "save")   { saveConfigChanges();   return true; }
    if (command == "revert") { revertConfigChanges(); return true; }
    return false;
}

// ============================================================================
// Control Button Repeat
// ============================================================================

void AppShell::updateCtrlBtnRepeats(float deltaTime)
{
    constexpr float INITIAL_DELAY   = 0.4f;
    constexpr float REPEAT_INTERVAL = 0.08f;

    for (auto& r : m_ctrlBtnRepeats)
    {
        if (r.btn == nullptr) continue;
        if (r.btn->isPressed() == true)
        {
            r.holdTime += deltaTime;
            if (r.repeatActive == false)
            {
                if (r.holdTime >= INITIAL_DELAY) { r.repeatActive = true; r.repeatAccum = 0.0f; }
            }
            else
            {
                r.repeatAccum += deltaTime;
                while (r.repeatAccum >= REPEAT_INTERVAL)
                {
                    r.repeatAccum -= REPEAT_INTERVAL;
                    IO::KeyEvent synth;
                    synth.keyCode  = r.keyCode;
                    synth.action   = IO::KeyAction::MULTIPLE;
                    synth.source   = IO::Source::TOUCHSCREEN;
                    synth.holdTime = r.holdTime;
                    onKeyEvent(synth);
                }
            }
        }
        else { r.holdTime = 0.0f; r.repeatAccum = 0.0f; r.repeatActive = false; }
    }
}

// ============================================================================
// Resource Loading
// ============================================================================

void AppShell::loadTextures()
{
    LOG_APP_INFO("AppShell: Loading textures");
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();
    std::string uiPath = std::string(_TEXTURES_PATH_) + "/ui";
    if (texMgr.loadTextureCollection(uiPath, "ui_list.txt", "aicam_ui") == true)
        LOG_APP_SUCCESSF("AppShell: Textures loaded from: %s", uiPath.c_str());
    else
        LOG_APP_WARNINGF("AppShell: Failed to load textures from: %s", uiPath.c_str());
}

void AppShell::loadLocale()
{
    const std::string lang = (APP::appConf.system_language == Language_English) ? "en" : "ko";
    UI::LocalizationManager::getInstance().setLanguage(lang);
}

void AppShell::loadConfigValues()
{
    m_configBackup  = APP::appConf;
    m_passwordIsSet = false;
    for (int d : APP::appConf.system_calib_pw)
        if (d != 0) { m_passwordIsSet = true; break; }
    if (APP::appConf.system_time.auto_time == false)
        applySystemTime(APP::appConf.system_time);

    // Apply display power timeout loaded from config
    IO::Platform::getInstance().setDisplayPowerTimeout(APP::appConf.system_display_timeout);
}

// ============================================================================
// Focus System
// ============================================================================

void AppShell::initializeFocusSystem(const std::vector<UI::View*>& extraMainViews)
{
    m_mainScreenContext = m_focusManager.createContext(UI::FocusContextType::NORMAL);
    m_mainScreenContext->setActive(true);
    m_mainScreenContext->setBlocking(false);
    m_mainScreenContext->setZOrder(0);
    m_mainScreenContext->setNavigationStrategy(UI::NavigationStrategy::GEOMETRIC);

    // Register the menu button and toggle button — bar is always created before setupMainScreenContent
    UI::Button* menuBtn    = (m_statusBar != nullptr) ? m_statusBar->getMenuButton()   : nullptr;
    UI::Button* toggleBtn  = (m_statusBar != nullptr) ? m_statusBar->getToggleButton() : nullptr;
    if (menuBtn != nullptr)
    {
        menuBtn->setFocusManager(&m_focusManager);
        m_focusManager.registerView(menuBtn, m_mainScreenContext);
    }
    if (toggleBtn != nullptr)
    {
        toggleBtn->setFocusManager(&m_focusManager);
        m_focusManager.registerView(toggleBtn, m_mainScreenContext);
        m_focusManager.setFocus(toggleBtn);
    }

    for (UI::View* v : extraMainViews)
    {
        if (v == nullptr) continue;
        v->setFocusManager(&m_focusManager);
        m_focusManager.registerView(v, m_mainScreenContext);
    }
}

// ============================================================================
// Menu Screen Setup
// ============================================================================

// ── Status bar ──────────────────────────────────────────────────────────────

void AppShell::setupStatusBar()
{
    m_statusBar = new StatusBar();
    m_statusBar->setOnVisibilityChanged([this](bool /*visible*/)
    {
        if (m_mainScreen != nullptr) m_mainScreen->getLayoutParams().setPadding(0.0f, 0.0f, 0.0f, static_cast<float>(getStatusBarHeight()));
    });
    m_rootLayout->addView(m_statusBar);
    m_rootLayout->addView(m_statusBar->getTogglePanel());
    onStatusBarCreated();
    LOG_APP_SUCCESS("AppShell: Status bar created");
}

int AppShell::getStatusBarHeight() const
{
    if (m_statusBar == nullptr)               return 0;
    if (m_statusBar->isBarVisible() == false) return 0;
    return StatusBar::HEIGHT;
}

void AppShell::setupMenuScreen()
{
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();

    m_menuScreen = new UI::LinearLayout();
    m_menuScreen->setOrientation(UI::Orientation::VERTICAL);
    m_menuScreen->getLayoutParams().width  = UI::MATCH_PARENT;
    m_menuScreen->getLayoutParams().height = UI::MATCH_PARENT;
    m_menuScreen->setVisibility(UI::Visibility::GONE);
    m_menuScreen->setBackgroundColor(UI::Color::Black); // opaque, block transparent
    m_rootLayout->addView(m_menuScreen);

    // ── Top bar ────────────────────────────────────────────────────────────
    UI::FrameLayout* topBar = new UI::FrameLayout();
    topBar->getLayoutParams().width  = UI::MATCH_PARENT;
    topBar->getLayoutParams().height = 140;
    topBar->setBackgroundColor(UI::Color::DarkBackground.withAlpha(0.95f));

    m_menuLogoView = new UI::ImageView();
    m_menuLogoView->getLayoutParams().width   = UI::WRAP_CONTENT;
    m_menuLogoView->getLayoutParams().height  = UI::WRAP_CONTENT;
    m_menuLogoView->getLayoutParams().gravity = UI::Gravity::LEFT | UI::Gravity::CENTER_VERTICAL;
    m_menuLogoView->getLayoutParams().setMargin(40.0f, 0.0f, 0.0f, 0.0f);
    UI::Texture* logoTex = texMgr.getTextureByName("04_logo_company");
    if (logoTex != nullptr) m_menuLogoView->setTexture(logoTex);
    topBar->addView(m_menuLogoView);

    UI::LinearLayout* titleArea = new UI::LinearLayout();
    titleArea->setOrientation(UI::Orientation::HORIZONTAL);
    titleArea->setGravity(UI::Gravity::CENTER);
    titleArea->setSpacing(20.0f);
    titleArea->getLayoutParams().width   = UI::WRAP_CONTENT;
    titleArea->getLayoutParams().height  = UI::WRAP_CONTENT;
    titleArea->getLayoutParams().gravity = UI::Gravity::CENTER;

    m_menuTitleIconView = new UI::ImageView();
    m_menuTitleIconView->getLayoutParams().width  = 60;
    m_menuTitleIconView->getLayoutParams().height = 60;
    titleArea->addView(m_menuTitleIconView);

    m_menuTitleTextView = new UI::TextView();
    SET_LOCALIZED_TEXT(m_menuTitleTextView, "");
    m_menuTitleTextView->setTextSize(60.0f);
    m_menuTitleTextView->setTextColor(UI::Color::White);
    m_menuTitleTextView->getLayoutParams().width  = UI::WRAP_CONTENT;
    m_menuTitleTextView->getLayoutParams().height = UI::WRAP_CONTENT;
    titleArea->addView(m_menuTitleTextView);
    topBar->addView(titleArea);

    auto makeTopBarBtn = [&](const char* locKey, UI::Gravity grav, float marginR) -> UI::Button*
    {
        UI::Button* btn = new UI::Button();
        SET_LOCALIZED_TEXT(btn, locKey);
        btn->setTextSize(60.0f);
        btn->setTextColor(UI::Color::TextPrimary);
        btn->setNormalColor(UI::Color::CardBackground);
        btn->setHoverColor(UI::Color::CardBackgroundHover);
        btn->setFocusedColor(UI::Color::CardBackground);
        btn->setPressedColor(UI::Color::CardBackgroundHover);
        btn->setBorderWidth(5.0f);
        btn->setCornerRadius(24.0f);
        btn->setFocusable(false);
        btn->getLayoutParams().width   = 150;
        btn->getLayoutParams().height  = 60;
        btn->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL | grav;
        btn->getLayoutParams().setMargin(0.0f, 0.0f, marginR, 0.0f);
        return btn;
    };

    m_menuSaveButton = makeTopBarBtn("Save", UI::Gravity::RIGHT, 200.0f);
    m_menuSaveButton->setOnClickListener([this](UI::View*) { closeMenu(true); });
    topBar->addView(m_menuSaveButton);

    m_menuBackButton = makeTopBarBtn("Exit", UI::Gravity::RIGHT, 20.0f);
    m_menuBackButton->setOnClickListener([this](UI::View*)
        { if (m_menuNavigator != nullptr) m_menuNavigator->navigateBack(); });
    topBar->addView(m_menuBackButton);

    m_menuScreen->addView(topBar);

    // ── Content area ───────────────────────────────────────────────────────
    m_menuContentLayout = new UI::FrameLayout();
    m_menuContentLayout->getLayoutParams().width  = UI::MATCH_PARENT;
    m_menuContentLayout->getLayoutParams().height = UI::MATCH_PARENT;
    m_menuContentLayout->getLayoutParams().weight = 1.0f;
    m_menuScreen->addView(m_menuContentLayout);

    // ── Bottom control hint bar ────────────────────────────────────────────
    constexpr float CTRL_BTN_W      = 60.0f;
    constexpr float CTRL_BTN_H      = 60.0f;
    constexpr float CTRL_BTN_ICO    = 48.0f;
    constexpr float CTRL_BTN_RADIUS = 10.0f;
    constexpr float CTRL_LABEL_SIZE = 48.0f;
    constexpr float CTRL_GROUP_GAP  = 24.0f;

    auto makeCtrlBtn = [&](const char* texName, IO::KeyCode kc) -> UI::Button*
    {
        UI::Button* btn = new UI::Button();
        UI::Texture* tex = texMgr.getTextureByName(texName);
        if (tex != nullptr) { btn->setTexture(tex); btn->setImageSize(CTRL_BTN_ICO, CTRL_BTN_ICO); }
        btn->setContentGravity(UI::Gravity::CENTER);
        btn->setNormalColor(UI::Color::Transparent);
        btn->setHoverColor(UI::Color::Transparent);
        btn->setPressedColor(UI::Color::CardBackgroundActive);
        btn->setCornerRadius(CTRL_BTN_RADIUS);
        btn->setFocusable(false);
        btn->getLayoutParams().width   = static_cast<int>(CTRL_BTN_W);
        btn->getLayoutParams().height  = static_cast<int>(CTRL_BTN_H);
        btn->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        btn->setOnClickListener([this, kc](UI::View*)
        {
            IO::KeyEvent synth;
            synth.keyCode  = kc;
            synth.action   = IO::KeyAction::DOWN;
            synth.source   = IO::Source::TOUCHSCREEN;
            synth.holdTime = 0.0f;
            onKeyEvent(synth);
        });
        return btn;
    };

    auto makeCtrlLabel = [&](const char* key) -> UI::TextView*
    {
        UI::TextView* lbl = new UI::TextView();
        SET_LOCALIZED_TEXT(lbl, key);
        lbl->setTextColor(UI::Color::White);
        lbl->setTextSize(CTRL_LABEL_SIZE);
        lbl->setTextGravity(UI::Gravity::CENTER_VERTICAL);
        lbl->getLayoutParams().width   = UI::WRAP_CONTENT;
        lbl->getLayoutParams().height  = UI::WRAP_CONTENT;
        lbl->getLayoutParams().gravity = UI::Gravity::CENTER_VERTICAL;
        lbl->getLayoutParams().setMargin(10.0f, 0.0f, CTRL_GROUP_GAP, 0.0f);
        return lbl;
    };

    m_ctrlBtnUp    = makeCtrlBtn("04_icn_ctrl_up",    IO::KeyCode::DPAD_UP);
    m_ctrlBtnDown  = makeCtrlBtn("04_icn_ctrl_down",  IO::KeyCode::DPAD_DOWN);
    m_ctrlBtnLeft  = makeCtrlBtn("04_icn_ctrl_left",  IO::KeyCode::DPAD_LEFT);
    m_ctrlBtnRight = makeCtrlBtn("04_icn_ctrl_right", IO::KeyCode::DPAD_RIGHT);
    m_ctrlBtnOk    = makeCtrlBtn("04_icn_ctrl_ok",    IO::KeyCode::DPAD_CENTER);

    m_ctrlBtnRepeats[0] = { m_ctrlBtnUp,    IO::KeyCode::DPAD_UP    };
    m_ctrlBtnRepeats[1] = { m_ctrlBtnDown,  IO::KeyCode::DPAD_DOWN  };
    m_ctrlBtnRepeats[2] = { m_ctrlBtnLeft,  IO::KeyCode::DPAD_LEFT  };
    m_ctrlBtnRepeats[3] = { m_ctrlBtnRight, IO::KeyCode::DPAD_RIGHT };

    m_ctrlBtnUp->getLayoutParams().setMargin(CTRL_GROUP_GAP, 0.0f, 2.0f, 0.0f);
    m_ctrlBtnDown->getLayoutParams().setMargin(2.0f, 0.0f, 2.0f, 0.0f);
    m_ctrlBtnLeft->getLayoutParams().setMargin(2.0f, 0.0f, 2.0f, 0.0f);
    m_ctrlBtnRight->getLayoutParams().setMargin(2.0f, 0.0f, 0.0f, 0.0f);

    m_menuControlsBar = new UI::LinearLayout();
    m_menuControlsBar->setOrientation(UI::Orientation::HORIZONTAL);
    m_menuControlsBar->setGravity(UI::Gravity::CENTER);
    m_menuControlsBar->setSpacing(0.0f);
    m_menuControlsBar->getLayoutParams().width   = UI::WRAP_CONTENT;
    m_menuControlsBar->getLayoutParams().height  = UI::WRAP_CONTENT;
    m_menuControlsBar->getLayoutParams().gravity = UI::Gravity::CENTER;

    m_menuControlsBar->addView(m_ctrlBtnUp);
    m_menuControlsBar->addView(m_ctrlBtnDown);
    m_menuControlsBar->addView(m_ctrlBtnLeft);
    m_menuControlsBar->addView(m_ctrlBtnRight);
    m_menuControlsBar->addView(makeCtrlLabel("Move"));
    m_ctrlBtnOk->getLayoutParams().setMargin(0.0f, 0.0f, 0.0f, 0.0f);
    m_menuControlsBar->addView(m_ctrlBtnOk);
    m_menuControlsBar->addView(makeCtrlLabel("Select"));

    UI::FrameLayout* botBar = new UI::FrameLayout();
    botBar->getLayoutParams().width  = UI::MATCH_PARENT;
    botBar->getLayoutParams().height = 130;
    botBar->setBackgroundColor(UI::Color::DarkBackground.withAlpha(0.95f));
    botBar->addView(m_menuControlsBar);

    m_hintTextView = new UI::TextView();
    m_hintTextView->setTextSize(42.0f);
    m_hintTextView->setTextColor(UI::Color::Yellow);
    m_hintTextView->setTextGravity(UI::Gravity::LEFT | UI::Gravity::CENTER_VERTICAL);
    m_hintTextView->getLayoutParams().width   = UI::WRAP_CONTENT;
    m_hintTextView->getLayoutParams().height  = UI::MATCH_PARENT;
    m_hintTextView->getLayoutParams().gravity = UI::Gravity::LEFT | UI::Gravity::CENTER_VERTICAL;
    m_hintTextView->getLayoutParams().paddingLeft = 20.0f;
    m_hintTextView->setVisibility(UI::Visibility::GONE);
    botBar->addView(m_hintTextView);

    m_menuScreen->addView(botBar);
}

void AppShell::setupMenuPopup()
{
    m_popupView = new UI::PopupView();
    m_popupView->getLayoutParams().width  = UI::MATCH_PARENT;
    m_popupView->getLayoutParams().height = UI::MATCH_PARENT;
    m_popupView->setFocusManager(&m_menuFocusManager);
    m_popupView->setOnConfirm([this]() { executePopupAction(); hidePopup(); });
    m_popupView->setOnCancel([this]()  { hidePopup(); });
    m_rootLayout->addView(m_popupView);

    m_numpadPopup = new UI::NumpadPopup();
    m_numpadPopup->getLayoutParams().width  = UI::MATCH_PARENT;
    m_numpadPopup->getLayoutParams().height = UI::MATCH_PARENT;
    m_numpadPopup->setFocusManager(&m_menuFocusManager);
    m_numpadPopup->setMaxLength(6);
    m_numpadPopup->setAllowDecimal(false);
    m_numpadPopup->setAllowNegative(false);
    m_rootLayout->addView(m_numpadPopup);
}

// ============================================================================
// Menu System Initialization
// ============================================================================

void AppShell::initializeMenuSystem()
{
    buildMenuTree();

    m_menuNavigator = std::make_unique<UI::MenuNavigator>();
    m_menuNavigator->setFocusManager(&m_menuFocusManager);

    #ifdef USE_DVR
        m_menuNavigator->addFileMenuId("file");
    #endif

    if (m_menuNavigator->initialize(m_menuRoot.get()) == false)
    {
        LOG_APP_ERROR("AppShell: Failed to initialize menu navigator");
        return;
    }

    m_menuNavigator->setOnBackToApp([this]()    { closeMenu(false); });
    m_menuNavigator->setOnSave([this]()         { closeMenu(true); });
    m_menuNavigator->setOnFileMenuExit([this]() { exitFileView(); });

    #ifdef USE_DVR
        UI::FileMenuView* fmv = m_menuNavigator->getFileMenuView();
        if (fmv != nullptr)
        {
            fmv->setFullscreenRootLayout(m_rootLayout);
            fmv->setOnFileSelected([this](int g, int f) { onFileViewFileSelected(g, f); });
            fmv->setOnFileUnload([this]()               { onFileViewFileUnload(); });

            fmv->setOnFileCopy([this](int groupIdx, int fileIdxInGroup)
            {
                m_fileViewSelectedGroupId = groupIdx;
                m_fileViewSelectedFileId  = fileIdxInGroup;
                VPB::GroupInfo* group = m_fileManager.getGroupAt(groupIdx);
                if (group != nullptr)
                {
                    VPB::FileInfo* file = m_fileManager.getFileAt(group->startIndex + fileIdxInGroup);
                    if (file != nullptr) m_fileViewSelectedFileName = file->name;
                }
                showPopup(PopupType::FileSingleCopy);
            });

            fmv->setOnFileDelete([this](int groupIdx, int fileIdxInGroup)
            {
                m_fileViewSelectedGroupId = groupIdx;
                m_fileViewSelectedFileId  = fileIdxInGroup;
                VPB::GroupInfo* group = m_fileManager.getGroupAt(groupIdx);
                if (group != nullptr)
                {
                    VPB::FileInfo* file = m_fileManager.getFileAt(group->startIndex + fileIdxInGroup);
                    if (file != nullptr) m_fileViewSelectedFileName = file->name;
                }
                showPopup(PopupType::FileSingleDelete);
            });

            fmv->setOnGroupCopy([this](int groupIdx)
                { m_fileViewSelectedGroupId = groupIdx; showPopup(PopupType::FileGroupCopy); });

            fmv->setOnVolumeChanged([this](float volume) { m_filePlayer.setVolume(volume); });
            // Sync initial volume from player config
            fmv->setVolume(m_filePlayer.getConfig().volume);
        }
    #endif

    UI::FrameLayout* navRoot = m_menuNavigator->getRootMenuView();
    if (navRoot != nullptr)
    {
        navRoot->getLayoutParams().width  = UI::MATCH_PARENT;
        navRoot->getLayoutParams().height = UI::MATCH_PARENT;
        m_menuContentLayout->addView(navRoot);
    }
}

// ============================================================================
// Menu Tree
// ============================================================================

void AppShell::buildMenuTree()
{
    m_menuRoot = std::make_shared<UI::MenuItem>("root", TR("Menu"), UI::MenuItemType::CATEGORY);
    SET_LOCALIZED_MENU_ITEM(m_menuRoot, "Menu");

    // Extra items from derived class
    buildExtraMenuItems();

    // Shell-standard items (DVR, File, System)
    buildShellMenuItems();
}

void AppShell::buildShellMenuItems()
{
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();

    // DVR settings
    #ifdef USE_DVR
    {
        auto dvrMenu = std::make_shared<UI::MenuItem>("dvr", TR("DVR"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(dvrMenu, "DVR");
        UI::Texture* icn = texMgr.getTextureByName("04_icn_dvr");
        if (icn != nullptr) dvrMenu->setIconTexture(icn);
        dvrMenu->setShowPreview(false);
        buildDvrMenuItems(dvrMenu);
        m_menuRoot->addChild(dvrMenu);
    }
    #endif

    // File playback
    #ifdef USE_DVR
    {
        auto fileMenu = std::make_shared<UI::MenuItem>("file", TR("File"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(fileMenu, "File");
        UI::Texture* icn = texMgr.getTextureByName("04_icn_file");
        if (icn != nullptr) fileMenu->setIconTexture(icn);
        fileMenu->setShowPreview(true);
        fileMenu->setOnActivate([this](UI::MenuItem*) { enterFileView(); });
        buildFileMenuItems(fileMenu);
        m_menuRoot->addChild(fileMenu);
    }
    #endif

    // System
    {
        auto sysMenu = std::make_shared<UI::MenuItem>("system", TR("System"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(sysMenu, "System");
        UI::Texture* icn = texMgr.getTextureByName("04_icn_system");
        if (icn != nullptr) sysMenu->setIconTexture(icn);
        sysMenu->setShowPreview(false);
        buildSystemMenuItems(sysMenu);
        m_menuRoot->addChild(sysMenu);
    }
}

void AppShell::buildDvrMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    #ifdef USE_DVR
        // Record Time
        {
            auto row = std::make_shared<UI::MenuItem>("dvr_rec_time", TR("Record_Time"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Record_Time");
            row->setShowPreview(false);
            const int sel = (APP::appConf.menu_dvr.recording_time == 300 ? 2
                           : APP::appConf.menu_dvr.recording_time == 180 ? 1 : 0);
            row->setSelectedChildIndex(sel);
            row->setOnValueChanged([](UI::MenuItem* item)
            {
                const int times[3] = { 60, 180, 300 };
                int idx = item->getSelectedChildIndex();
                if (idx >= 0 && idx < 3) APP::appConf.menu_dvr.recording_time = times[idx];
            });
            struct { const char* id; const char* lbl; } opts[3] = {
                {"dvr_rt_1m","1min"}, {"dvr_rt_3m","3min"}, {"dvr_rt_5m","5min"}
            };
            for (auto& o : opts)
            {
                auto opt = std::make_shared<UI::MenuItem>(o.id, o.lbl, UI::MenuItemType::ACTION);
                row->addChild(opt);
            }
            parent->addChild(row);
        }

        // Channel
        {
            auto row = std::make_shared<UI::MenuItem>("dvr_channel", TR("Channel"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Channel");
            row->setShowPreview(false);
            const int sel = (APP::appConf.menu_dvr.recording_channel == 8 ? 3
                           : APP::appConf.menu_dvr.recording_channel == 6 ? 2
                           : APP::appConf.menu_dvr.recording_channel == 4 ? 1 : 0);
            row->setSelectedChildIndex(sel);
            row->setOnValueChanged([](UI::MenuItem* item)
            {
                const int chs[4] = { 0, 4, 6, 8 };
                int idx = item->getSelectedChildIndex();
                if (idx >= 0 && idx < 4) APP::appConf.menu_dvr.recording_channel = chs[idx];
            });
            struct { const char* id; const char* lbl; } opts[4] = {
                {"dvr_ch_na","NA"}, {"dvr_ch_4","4CH"}, {"dvr_ch_6","6CH"}, {"dvr_ch_8","8CH"}
            };
            for (auto& o : opts)
            {
                auto opt = std::make_shared<UI::MenuItem>(o.id, o.lbl, UI::MenuItemType::ACTION);
                row->addChild(opt);
            }
            parent->addChild(row);
        }

        // Framerate / AVM — only when SVM cameras are present
        #ifdef USE_SVM
        {
            auto row = std::make_shared<UI::MenuItem>("dvr_fps_avm", TR("Framerate_AVM"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Framerate_AVM");
            row->setShowPreview(false);
            const int sel = (APP::appConf.menu_dvr.recording_fps_avm == 30 ? 2
                           : APP::appConf.menu_dvr.recording_fps_avm == 20 ? 1 : 0);
            row->setSelectedChildIndex(sel);
            row->setOnValueChanged([](UI::MenuItem* item)
            {
                const int fps[3] = { 10, 20, 30 };
                int idx = item->getSelectedChildIndex();
                if (idx >= 0 && idx < 3) APP::appConf.menu_dvr.recording_fps_avm = fps[idx];
            });
            struct { const char* id; const char* lbl; } opts[3] = {
                {"dvr_fps_avm_10","10FPS"}, {"dvr_fps_avm_20","20FPS"}, {"dvr_fps_avm_30","30FPS"}
            };
            for (auto& o : opts)
            {
                auto opt = std::make_shared<UI::MenuItem>(o.id, o.lbl, UI::MenuItemType::ACTION);
                row->addChild(opt);
            }
            parent->addChild(row);
        }
        #endif // USE_SVM

        // Framerate / Internal
        {
            auto row = std::make_shared<UI::MenuItem>("dvr_fps_int", TR("Framerate_Internal"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Framerate_Internal");
            row->setShowPreview(false);
            const int sel = (APP::appConf.menu_dvr.recording_fps_internal == 30 ? 2
                           : APP::appConf.menu_dvr.recording_fps_internal == 20 ? 1 : 0);
            row->setSelectedChildIndex(sel);
            row->setOnValueChanged([](UI::MenuItem* item)
            {
                const int fps[3] = { 10, 20, 30 };
                int idx = item->getSelectedChildIndex();
                if (idx >= 0 && idx < 3) APP::appConf.menu_dvr.recording_fps_internal = fps[idx];
            });
            struct { const char* id; const char* lbl; } opts[3] = {
                {"dvr_fps_int_10","10FPS"}, {"dvr_fps_int_20","20FPS"}, {"dvr_fps_int_30","30FPS"}
            };
            for (auto& o : opts)
            {
                auto opt = std::make_shared<UI::MenuItem>(o.id, o.lbl, UI::MenuItemType::ACTION);
                row->addChild(opt);
            }
            parent->addChild(row);
        }

        // Record Audio
        {
            auto row = std::make_shared<UI::MenuItem>("dvr_audio", TR("Record_Audio"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Record_Audio");
            row->setShowPreview(false);
            const int sel = APP::appConf.menu_dvr.audio_enabled ? 1 : 0;
            row->setSelectedChildIndex(sel);
            row->setOnValueChanged([](UI::MenuItem* item)
            {
                APP::appConf.menu_dvr.audio_enabled = (item->getSelectedChildIndex() == 1);
            });
            struct { const char* id; const char* lbl; } opts[2] = {
                {"dvr_audio_off", "OFF"}, {"dvr_audio_on", "ON"}
            };
            for (auto& o : opts)
            {
                auto opt = std::make_shared<UI::MenuItem>(o.id, TR(o.lbl), UI::MenuItemType::ACTION);
                SET_LOCALIZED_MENU_ITEM(opt, o.lbl);
                row->addChild(opt);
            }
            parent->addChild(row);
        }
    #endif
}

void AppShell::buildFileMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    #ifdef USE_DVR
        // Group list
        {
            auto grpList = std::make_shared<UI::MenuItem>("grp_list", TR("Group_List"), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(grpList, "Group_List");
            grpList->setEnabled(false);
            parent->addChild(grpList);
        }

        // Group range navigation
        {
            auto row    = std::make_shared<UI::MenuItem>("group_range", TR("Group_Range"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Group_Range");
            row->setShowPreview(false);
            auto arrowL = std::make_shared<UI::MenuItem>("grp_left",  "<",       UI::MenuItemType::ACTION);
            auto label  = std::make_shared<UI::MenuItem>("grp_label", "101~200", UI::MenuItemType::ACTION);
            auto arrowR = std::make_shared<UI::MenuItem>("grp_right", ">",       UI::MenuItemType::ACTION);
            arrowL->setEnabled(false);
            label ->setEnabled(false);
            arrowR->setEnabled(false);
            arrowL->setOnActivate([this](UI::MenuItem*)
            {
                int32_t maxId = std::max(0, m_fileManager.getGroupCount() - 1);
                m_fileViewSelectedGroupId = (m_fileViewSelectedGroupId > 0)
                    ? m_fileViewSelectedGroupId - 1 : maxId;
                rebuildGroupsAfterDirectoryChange();
            });
            arrowR->setOnActivate([this](UI::MenuItem*)
            {
                int32_t maxId = std::max(0, m_fileManager.getGroupCount() - 1);
                m_fileViewSelectedGroupId = (m_fileViewSelectedGroupId < maxId)
                    ? m_fileViewSelectedGroupId + 1 : 0;
                rebuildGroupsAfterDirectoryChange();
            });
            row->addChild(arrowL); row->addChild(label); row->addChild(arrowR);
            parent->addChild(row);
        }

        // Group copy
        {
            auto row = std::make_shared<UI::MenuItem>("group_copy", TR("Group_Copy"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Group_Copy");
            row->setShowPreview(false);
            auto btn = std::make_shared<UI::MenuItem>("group_copy_btn", TR("Group_Copy"), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(btn, "Group_Copy");
            btn->setOnActivate([this](UI::MenuItem*) { showPopup(PopupType::FileGroupCopy); });
            row->addChild(btn);
            parent->addChild(row);
        }

        // File list
        {
            auto fileList = std::make_shared<UI::MenuItem>("file_list", TR("File_List"), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(fileList, "File_List");
            parent->addChild(fileList);
        }

        // Channel
        {
            auto row = std::make_shared<UI::MenuItem>("channel", TR("Channel"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Channel");
            row->setShowPreview(false);
            struct ChInfo { const char* id; const char* lbl; int vid; };
            static const ChInfo chs[] = {
                {"ch1","1",0}, {"ch2","2",1}, {"ch3","3",2}, {"ch4","4",3},
                {"ch14","1-4",8},
                {"ch5","5",4}, {"ch6","6",5}, {"ch7","7",6}, {"ch8","8",7},
                {"ch58","5-8",9},
            };
            for (const auto& c : chs)
            {
                auto ch = std::make_shared<UI::MenuItem>(c.id, c.lbl, UI::MenuItemType::ACTION);
                int vid = c.vid;
                ch->setOnActivate([this, vid](UI::MenuItem*) { m_filePlayer.setSelectedView(vid); });
                row->addChild(ch);
            }
            parent->addChild(row);
        }

        // Playback controls
        {
            auto row = std::make_shared<UI::MenuItem>("control", TR("Control"), UI::MenuItemType::CATEGORY);
            SET_LOCALIZED_MENU_ITEM(row, "Control");
            row->setShowPreview(false);
            auto addCtrl = [&](const char* id, const char* lbl, std::function<void()> fn)
            {
                auto btn = std::make_shared<UI::MenuItem>(id, lbl, UI::MenuItemType::ACTION);
                btn->setOnActivate([fn](UI::MenuItem*) { fn(); });
                row->addChild(btn);
            };
            addCtrl("ctrl_bwd",  "", [this] { playbackSeekBackward(); });
            addCtrl("ctrl_play", "", [this] { startPausePlayback();   });
            addCtrl("ctrl_stop", "", [this] { stopPlayback();          });
            addCtrl("ctrl_fwd",  "", [this] { playbackSeekForward();   });
            parent->addChild(row);
        }
    #endif
}

void AppShell::buildSystemMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    // Language
    {
        auto row = std::make_shared<UI::MenuItem>("language", TR("Language"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "Language");
        row->setShowPreview(false);
        auto eng = std::make_shared<UI::MenuItem>("lang_en", "English",    UI::MenuItemType::ACTION);
        auto kor = std::make_shared<UI::MenuItem>("lang_kr", TR("Korean"), UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(kor, "Korean");
        row->setSelectedChildIndex(APP::appConf.system_language == Language_Korean ? 1 : 0);
        row->setOnValueChanged([this, row](UI::MenuItem*)
        {
            int selIdx  = row->getSelectedChildIndex();
            int newLang = (selIdx == 0) ? Language_English : Language_Korean;
            if (APP::appConf.system_language != newLang)
            {
                APP::appConf.system_language = newLang;
                m_pendingLangChange = (selIdx == 0) ? "en" : "ko";
            }
        });
        row->addChild(eng); row->addChild(kor);
        parent->addChild(row);
    }

    // System time — Children: [0]=auto, [1]=year, [2]=month, [3]=day, [4]=hour, [5]=min, [6]=sec
    {
        auto row = std::make_shared<UI::MenuItem>("system_time", TR("System_Time"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "System_Time");
        row->setShowPreview(false);
        const auto& st = APP::appConf.system_time;

        auto fAuto = std::make_shared<UI::MenuItem>(
            "time_auto", st.auto_time ? TR("Auto_Time") : TR("Manual_Time"), UI::MenuItemType::SLIDER);
        fAuto->setRange(0.0f, 1.0f, 1.0f);
        fAuto->setValueFloat(st.auto_time ? 1.0f : 0.0f);
        fAuto->setOnValueChanged([this, row](UI::MenuItem* item)
        {
            const bool nowAuto = (item->getValueFloat() >= 0.5f);
            APP::appConf.system_time.auto_time = nowAuto;
            const char* key = nowAuto ? "Auto_Time" : "Manual_Time";
            item->setName(TR(key));
            UI::LocalizationManager::getInstance().registerMenuItem(item, key);
            const auto& ch = row->getChildren();
            for (int fi = 1; fi < static_cast<int>(ch.size()); ++fi)
                if (ch[fi] != nullptr) ch[fi]->setEnabled(!nowAuto);
            if (nowAuto == true) syncSystemTimeFromOS(APP::appConf.system_time);
            else                 applySystemTime(APP::appConf.system_time);
        });
        UI::LocalizationManager::getInstance().registerMenuItem(
            fAuto.get(), st.auto_time ? "Auto_Time" : "Manual_Time");
        fAuto->setOnActivate([this, row](UI::MenuItem*)
        {
            m_timeRowItem    = row.get();
            m_timeFieldIndex = 0;
            row->setSelectedChildIndex(0);
            startAdjustmentDateTime();
        });
        row->addChild(fAuto);

        struct TimeFieldDef
        {
            const char* id;
            float minV, maxV;
            int   fieldIdx;
            std::function<int()>            getValue;
            std::function<void(int)>        setValue;
            std::function<std::string(int)> format;
        };
        const std::vector<TimeFieldDef> fields = {
            { "time_year",  2000, 2099, 1,
              []{ return APP::appConf.system_time.year;   }, [](int v){ APP::appConf.system_time.year   = v; }, [](int v){ return pad4(v); } },
            { "time_month",    1,   12, 2,
              []{ return APP::appConf.system_time.month;  }, [](int v){ APP::appConf.system_time.month  = v; }, [](int v){ return pad2(v); } },
            { "time_day",      1,   31, 3,
              []{ return APP::appConf.system_time.day;    }, [](int v){ APP::appConf.system_time.day    = v; }, [](int v){ return pad2(v); } },
            { "time_hour",     0,   23, 4,
              []{ return APP::appConf.system_time.hour;   }, [](int v){ APP::appConf.system_time.hour   = v; }, [](int v){ return pad2(v); } },
            { "time_min",      0,   59, 5,
              []{ return APP::appConf.system_time.minute; }, [](int v){ APP::appConf.system_time.minute = v; }, [](int v){ return pad2(v); } },
            { "time_sec",      0,   59, 6,
              []{ return APP::appConf.system_time.second; }, [](int v){ APP::appConf.system_time.second = v; }, [](int v){ return pad2(v); } },
        };
        for (const auto& f : fields)
        {
            auto fld = std::make_shared<UI::MenuItem>(f.id, f.format(f.getValue()), UI::MenuItemType::SLIDER);
            fld->setRange(f.minV, f.maxV, 1.0f);
            fld->setValueFloat(static_cast<float>(f.getValue()));
            fld->setOnValueChanged([f](UI::MenuItem* item)
            {
                int v = static_cast<int>(item->getValueFloat());
                f.setValue(v);
                item->setName(f.format(v));
            });
            int fieldIdx = f.fieldIdx;
            fld->setOnActivate([this, row, fieldIdx](UI::MenuItem*)
            {
                m_timeRowItem    = row.get();
                m_timeFieldIndex = fieldIdx;
                row->setSelectedChildIndex(fieldIdx);
                startAdjustmentDateTime();
            });
            row->addChild(fld);
        }
        row->setSelectedChildIndex(0);
        m_timeRowItem    = row.get();
        m_timeMenuParent = parent.get();
        if (APP::appConf.system_time.auto_time == true)
        {
            const auto& ch = row->getChildren();
            for (int fi = 1; fi < static_cast<int>(ch.size()); ++fi)
                if (ch[fi] != nullptr) ch[fi]->setEnabled(false);
        }
        parent->addChild(row);
    }

    // Storage format (DVR only)
    #ifdef USE_DVR
    {
        auto row = std::make_shared<UI::MenuItem>("storage_format", TR("Storage_Format"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "Storage_Format");
        row->setShowPreview(false);
        auto btn = std::make_shared<UI::MenuItem>("format_btn", TR("Format"), UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(btn, "Format");
        btn->setOnActivate([this](UI::MenuItem*)
        {
            auto doFormat = [this]() { showPopup(PopupType::SystemFormat); };
            if (hasStoredPassword() == false) { doFormat(); return; }
            showNumpad(TR("Format_EnterPassword"), [this, doFormat](std::array<int,6> entered)
            {
                if (passwordsMatch(entered, APP::appConf.system_calib_pw) == true) doFormat();
                else showPwPopup("Format_WrongPassword");
            });
        });
        row->addChild(btn);
        parent->addChild(row);
    }
    #endif

    // System Password
    {
        auto row = std::make_shared<UI::MenuItem>("system_password", TR("System_Password"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "System_Password");
        row->setShowPreview(false);

        auto setPwBtn = std::make_shared<UI::MenuItem>("set_pw", TR("Set_Password"), UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(setPwBtn, "Set_Password");
        setPwBtn->setOnActivate([this](UI::MenuItem*)
        {
            auto enterNew = [this]()
            {
                showNumpad(TR("Password_EnterNew"), [this](std::array<int,6> newPw)
                {
                    showNumpad(TR("Password_ConfirmNew"), [this, newPw](std::array<int,6> confirm)
                    {
                        if (passwordsMatch(newPw, confirm) == false) { showPwPopup("Password_Mismatch"); return; }
                        APP::appConf.system_calib_pw = newPw;
                        m_passwordIsSet = true;
                        saveConfigChanges();
                        showPwPopup("Password_Success");
                    });
                });
            };
            if (hasStoredPassword() == true)
                showNumpad(TR("Password_EnterOld"), [this, enterNew](std::array<int,6> old)
                {
                    if (passwordsMatch(old, APP::appConf.system_calib_pw) == false)
                        { showPwPopup("Password_Incorrect"); return; }
                    enterNew();
                });
            else
                enterNew();
        });

        auto clearPwBtn = std::make_shared<UI::MenuItem>("clear_pw", TR("Clear_Password"), UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(clearPwBtn, "Clear_Password");
        clearPwBtn->setOnActivate([this](UI::MenuItem*)
        {
            if (hasStoredPassword() == false) { showPwPopup("Password_NoneSet"); return; }
            showNumpad(TR("Password_EnterOld"), [this](std::array<int,6> old)
            {
                if (passwordsMatch(old, APP::appConf.system_calib_pw) == false)
                    { showPwPopup("Password_Incorrect"); return; }
                if (m_popupView == nullptr) return;
                UI::Texture* icon = UI::TextureManager::getInstance().getTextureByName("09_icn_popup_system");
                m_popupVisible = true;
                m_popupType    = PopupType::PopupNone;
                m_popupView->show(TR("Password_ClearConfirm"), "", icon,
                                  UI::PopupButtonMode::OK_CANCEL, TR("OK"), TR("Cancel"));
                m_popupView->setOnConfirm([this]()
                {
                    APP::appConf.system_calib_pw.fill(0);
                    m_passwordIsSet = false;
                    saveConfigChanges();
                    hidePopup();
                    m_pendingPwMsg = "Password_Cleared";
                });
                m_popupView->setOnCancel([this]() { hidePopup(); });
            });
        });

        row->addChild(setPwBtn);
        row->addChild(clearPwBtn);
        parent->addChild(row);
    }

    // Display Timeout
    {
        auto row = std::make_shared<UI::MenuItem>("display_timeout", TR("Display_Timeout"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "Display_Timeout");
        row->setShowPreview(false);

        struct TimeoutOption { const char* id; const char* locKey; int secs; };
        static const TimeoutOption opts[4] = {
            { "dt_always_on", "Display_Timeout_AlwaysOn",  0 },
            { "dt_10s",       "Display_Timeout_10s",      10 },
            { "dt_20s",       "Display_Timeout_20s",      20 },
            { "dt_30s",       "Display_Timeout_30s",      30 },
        };

        // Resolve selected index from current config value
        // Index 0 = Always On (0s), 1 = 10s, 2 = 20s, 3 = 30s
        int selIdx = 0; // default: Always On
        if (APP::appConf.system_display_timeout == 10)      selIdx = 1;
        else if (APP::appConf.system_display_timeout == 20) selIdx = 2;
        else if (APP::appConf.system_display_timeout == 30) selIdx = 3;
        row->setSelectedChildIndex(selIdx);

        row->setOnValueChanged([](UI::MenuItem* item)
        {
            static const int timeoutValues[4] = { 0, 10, 20, 30 };
            const int idx = item->getSelectedChildIndex();
            if (idx >= 0 && idx < 4)
                APP::appConf.system_display_timeout = timeoutValues[idx];
        });

        for (const auto& opt : opts)
        {
            auto child = std::make_shared<UI::MenuItem>(opt.id, TR(opt.locKey), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(child, opt.locKey);
            row->addChild(child);
        }
        parent->addChild(row);
    }

    // Extra system items from derived class (e.g. Vehicle / Calibration for SVM)
    buildExtraSystemMenuItems(parent);
}

// ============================================================================
// Screen Management
// ============================================================================

void AppShell::showMainScreen()
{
    m_mainScreen->setVisibility(UI::Visibility::VISIBLE);
    m_menuScreen->setVisibility(UI::Visibility::GONE);
    if (m_statusBar != nullptr) m_statusBar->setVisibility(UI::Visibility::VISIBLE);
}

void AppShell::showMenuScreen()
{
    m_configBackup = APP::appConf;
    syncMenuFromConfig();
    m_menuVisible = true;
    if (m_statusBar != nullptr) m_statusBar->setVisibility(UI::Visibility::GONE);
    m_menuScreen->setVisibility(UI::Visibility::VISIBLE);
    if (m_menuNavigator != nullptr)
    {
        m_menuNavigator->navigateToRoot();
        setMenuTopBarFocus(TopBarFocus::NONE);
        updateMenuDisplay();
    }
}

void AppShell::closeMenu(bool save)
{
    cancelAdjustment();

    if (save == false)
    {
        APP::appConf = m_configBackup;
        syncMenuFromConfig();
        onConfigReverted();
    }
    else
    {
        saveConfigChanges();
    }

    #ifdef USE_DVR
        if (m_fileViewEntered == true)
        {
            UI::FileMenuView* fmv = (m_menuNavigator != nullptr)
                ? m_menuNavigator->getFileMenuView() : nullptr;
            if (fmv != nullptr && fmv->isPreviewFullscreen() == true)
                fmv->exitPreviewFullscreen();
            exitFileView();
        }
    #endif

    m_menuVisible = false;
    m_menuScreen->setVisibility(UI::Visibility::GONE);
    showMainScreen();
}

// ============================================================================
// Adjustment System
// ============================================================================

void AppShell::startAdjustment(AdjustmentType type, int steps)
{
    m_adjustmentType      = type;
    m_adjustmentSteps     = steps;
    m_adjustmentStepIndex = 0;
}

void AppShell::cancelAdjustment()
{
    if (m_adjustmentType == AdjustmentType::AdjNone) return;
    onCancelAdjustment();
    m_adjustmentType      = AdjustmentType::AdjNone;
    m_adjustmentSteps     = 0;
    m_adjustmentStepIndex = 0;
    if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
    clearMenuHint();
}

void AppShell::confirmAdjustment()
{
    m_adjustmentStepIndex++;
    if (m_adjustmentStepIndex >= m_adjustmentSteps)
    {
        m_adjustmentType = AdjustmentType::AdjNone;
        LOG_APP_INFO("AppShell: Adjustment complete");
    }
    else
        stepEntered(m_adjustmentStepIndex);
}

void AppShell::stepEntered(int step) { LOG_APP_INFOF("AppShell: Adjustment step entered: %d", step); }

void AppShell::setMenuHint(const std::string& text, UI::Color4 color)
{
    if (m_hintTextView == nullptr) return;
    m_hintTextView->setText(text);
    m_hintTextView->setTextColor(color);
    m_hintTextView->setVisibility(UI::Visibility::VISIBLE);
}

void AppShell::clearMenuHint()
{
    if (m_hintTextView == nullptr) return;
    m_hintTextView->setVisibility(UI::Visibility::GONE);
}

void AppShell::startAdjustmentDateTime()
{
    startAdjustment(AdjustmentType::DateTime, 1);
    if (m_menuNavigator != nullptr)
    {
        m_menuNavigator->setSubMenuAdjustmentMode(true);
        m_menuNavigator->setSubMenuOnAdjustmentConfirm([this]()
        {
            if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
            confirmAdjustment();
            if (APP::appConf.system_time.auto_time == false)
                applySystemTime(APP::appConf.system_time);
        });
    }
}

void AppShell::adjustDateTime(int fieldDelta, int valueDelta)
{
    if (m_timeRowItem == nullptr) return;
    const bool isAutoMode = APP::appConf.system_time.auto_time;

    if (fieldDelta != 0)
    {
        constexpr int FIELD_COUNT = 7;
        m_timeFieldIndex = isAutoMode
            ? 0
            : (m_timeFieldIndex + fieldDelta + FIELD_COUNT) % FIELD_COUNT;
        m_timeRowItem->setSelectedChildIndex(m_timeFieldIndex);
        m_timePillsNeedUpdate.store(true);
        return;
    }

    if (valueDelta == 0) return;

    const auto& children = m_timeRowItem->getChildren();
    if (m_timeFieldIndex < 0 || m_timeFieldIndex >= static_cast<int>(children.size())) return;

    UI::MenuItem* field = children[m_timeFieldIndex].get();
    if (field == nullptr) return;

    const bool isAutoField = (m_timeFieldIndex == 0);
    if (isAutoField == false && isAutoMode == true) return;

    float cur  = field->getValueFloat();
    float minV = field->getMinValue();
    float maxV = field->getMaxValue();
    float next = cur + static_cast<float>(valueDelta) * field->getStep();
    if      (next > maxV) next = minV;
    else if (next < minV) next = maxV;
    field->setValueFloat(next);

    if (isAutoField == true)
    {
        const bool isAutoNow = APP::appConf.system_time.auto_time;
        for (int fi = 1; fi < static_cast<int>(children.size()); ++fi)
            if (children[fi] != nullptr) children[fi]->setEnabled(!isAutoNow);

        if (isAutoNow == true)
        {
            const auto& st = APP::appConf.system_time;
            const struct { int idx; std::string name; } nameUpdates[] = {
                {1, pad4(st.year)}, {2, pad2(st.month)},  {3, pad2(st.day)},
                {4, pad2(st.hour)}, {5, pad2(st.minute)}, {6, pad2(st.second)},
            };
            for (const auto& u : nameUpdates)
                if (u.idx < static_cast<int>(children.size()) && children[u.idx] != nullptr)
                    children[u.idx]->setName(u.name);
            m_timeFieldIndex = 0;
            m_timeRowItem->setSelectedChildIndex(0);
        }
    }
    m_timePillsNeedUpdate.store(true);
}

// ============================================================================
// Password / Numpad
// ============================================================================

void AppShell::showNumpad(const std::string& title,
                          std::function<void(std::array<int,6>)> onConfirm,
                          std::function<void()> onCancel)
{
    m_numpadPendingShow    = true;
    m_numpadPendingTitle   = title;
    m_numpadPendingConfirm = std::move(onConfirm);
    m_numpadPendingCancel  = (onCancel != nullptr) ? std::move(onCancel) : [this]()
    {
        if (m_numpadPopup != nullptr)
        {
            m_numpadPopup->setOnConfirm(nullptr);
            m_numpadPopup->setOnCancel(nullptr);
        }
    };
}

void AppShell::showPwPopup(const std::string& msgKey)
{
    if (m_popupView == nullptr) return;
    m_popupView->setOnConfirm([this]() { executePopupAction(); hidePopup(); });
    m_popupView->setOnCancel([this]()  { hidePopup(); });
    UI::Texture* icon = UI::TextureManager::getInstance().getTextureByName("09_icn_popup_system");
    m_popupVisible = true;
    m_popupType    = PopupType::PopupNone;
    m_popupView->show(TR(msgKey), "", icon, UI::PopupButtonMode::OK_ONLY, TR("OK"), TR("Cancel"));
}

bool AppShell::passwordsMatch(const std::array<int,6>& a, const std::array<int,6>& b) { return a == b; }

std::array<int,6> AppShell::stringToPasswordDigits(const std::string& s)
{
    std::array<int,6> r = {};
    int out = 0;
    for (char c : s)
        if (c >= '0' && c <= '9' && out < 6) r[out++] = c - '0';
    return r;
}

// ============================================================================
// Popup Management
// ============================================================================

void AppShell::showPopup(PopupType type)
{
    m_popupType    = type;
    m_popupVisible = true;

    UI::TextureManager& texMgr = UI::TextureManager::getInstance();
    std::string iconTexName, topText, botText;
    UI::PopupButtonMode btnMode = UI::PopupButtonMode::OK_CANCEL;

    switch (type)
    {
        #ifdef USE_DVR
            case PopupType::FileInvalid:
                iconTexName = "09_icn_popup_format";
                topText     = "FileView_corrupted";
                btnMode     = UI::PopupButtonMode::OK_ONLY;
                break;
            case PopupType::FileSingleCopy:
                iconTexName = "08_icn_popup_copy";
                topText     = "FileView_copySingle";
                botText     = "( " + m_fileViewSelectedFileName + " )";
                break;
            case PopupType::FileSingleDelete:
                iconTexName = "08_icn_popup_delete";
                topText     = "FileView_deleteSingle";
                botText     = "( " + m_fileViewSelectedFileName + " )";
                break;
            case PopupType::FileGroupCopy:
            {
                iconTexName = "08_icn_popup_copy";
                topText     = "FileView_copyGroup";
                char groupRange[64];
                snprintf(groupRange, sizeof(groupRange), "( %d ~ %d )",
                         m_fileViewSelectedGroupId * m_fileViewGroupMaxSize + 1,
                         (m_fileViewSelectedGroupId + 1) * m_fileViewGroupMaxSize);
                botText = groupRange;
                break;
            }
        #endif
        case PopupType::SystemFormat:
            iconTexName = "09_icn_popup_format";
            topText     = "System_format";
            botText     = "Format_confirm";
            break;
        default:
            break;
    }

    UI::Texture* iconTex = iconTexName.empty() ? nullptr : texMgr.getTextureByName(iconTexName);
    if (m_popupView != nullptr)
        m_popupView->show(TR(topText), TR(botText), iconTex, btnMode, TR("OK"), TR("Cancel"));
}

void AppShell::hidePopup()
{
    m_popupVisible = false;
    m_popupType    = PopupType::PopupNone;
    if (m_popupView != nullptr) m_popupView->hide();
}

void AppShell::executePopupAction()
{
    switch (m_popupType)
    {
        case PopupType::SystemFormat:
        {
            #ifdef USE_DVR
                const bool wasRecording    = (APP::dvrConf.recordingChannel > 0);
                const int  savedChannel    = APP::dvrConf.recordingChannel;
                if (wasRecording == true)
                {
                    LOG_APP_INFO("AppShell: Stopping DVR before format...");
                    onChannelCountChanged(0);
                }
            #endif

            const std::string cmd = "find \"" + APP::dvrConf.storageDevPath + "/VideoFiles\""
                                    " -type f \\( -name \"*.txt\" -o -name \"*.mp4\" -o -name \"*.avi\" -o -name \"*.mkv\" \\)"
                                    " -exec rm -f {} \\;";
            if (system(cmd.c_str()) != 0)
                LOG_APP_ERROR("AppShell: Format failed");
            else
                LOG_APP_SUCCESS("AppShell: Format done");

            #ifdef USE_DVR
                if (wasRecording == true)
                {
                    LOG_APP_INFO("AppShell: Resuming DVR after format...");
                    onChannelCountChanged(savedChannel);
                }
                if (m_fileViewEntered == true) rebuildGroupsAfterDirectoryChange();
            #endif
            break;
        }

        #ifdef USE_DVR
        case PopupType::FileSingleDelete:
        {
            VPB::GroupInfo* group = m_fileManager.getGroupAt(m_fileViewSelectedGroupId);
            if (group == nullptr) break;
            const int32_t globalIdx = group->startIndex + m_fileViewSelectedFileId;
            VPB::FileInfo* file     = m_fileManager.getFileAt(globalIdx);
            if (file == nullptr) break;

            const std::string path = file->path;
            if (m_fileManager.deleteFile(globalIdx) == false)
                LOG_APP_ERRORF("AppShell: File delete failed: %s", path.c_str());
            else
            {
                LOG_APP_SUCCESSF("AppShell: File deleted: %s", path.c_str());
                rebuildGroupsAfterDirectoryChange();
            }
            break;
        }

        case PopupType::FileSingleCopy:
        {
            VPB::GroupInfo* group = m_fileManager.getGroupAt(m_fileViewSelectedGroupId);
            if (group == nullptr) break;
            VPB::FileInfo* file = m_fileManager.getFileAt(group->startIndex + m_fileViewSelectedFileId);
            if (file == nullptr) break;

            const std::string path = file->path;
            if (m_fileManager.copyFile(group->startIndex + m_fileViewSelectedFileId, "/mnt/sdcard") == false)
                LOG_APP_ERRORF("AppShell: File copy failed: %s", path.c_str());
            else
                LOG_APP_SUCCESSF("AppShell: File copied: %s", path.c_str());
            break;
        }

        case PopupType::FileGroupCopy:
        {
            VPB::GroupInfo* group = m_fileManager.getGroupAt(m_fileViewSelectedGroupId);
            if (group == nullptr) break;

            const int32_t count = group->count;
            if (m_fileManager.copyGroup(m_fileViewSelectedGroupId, "/mnt/sdcard") == false)
                LOG_APP_ERRORF("AppShell: Group copy failed (group %d)", m_fileViewSelectedGroupId);
            else
                LOG_APP_SUCCESSF("AppShell: Group copied: %d files", count);
            break;
        }
        #endif // USE_DVR

        default:
            break;
    }
}

// ============================================================================
// Configuration Management
// ============================================================================

void AppShell::syncMenuFromConfig()
{
    if (m_menuRoot == nullptr) return;
    const AppConfiguration& c = APP::appConf;

    #ifdef USE_DVR
    {
        auto setIdx = [&](const char* id, int idx)
        {
            UI::MenuItem* item = m_menuRoot->find(id);
            if (item != nullptr) item->setSelectedChildIndex(idx);
        };

        setIdx("dvr_rec_time",
            c.menu_dvr.recording_time == 300 ? 2 :
            c.menu_dvr.recording_time == 180 ? 1 : 0);

        setIdx("dvr_channel",
            c.menu_dvr.recording_channel == 8 ? 3 :
            c.menu_dvr.recording_channel == 6 ? 2 :
            c.menu_dvr.recording_channel == 4 ? 1 : 0);

        #ifdef USE_SVM
            setIdx("dvr_fps_avm",
                c.menu_dvr.recording_fps_avm == 30 ? 2 :
                c.menu_dvr.recording_fps_avm == 20 ? 1 : 0);
        #endif

        setIdx("dvr_fps_int",
            c.menu_dvr.recording_fps_internal == 30 ? 2 :
            c.menu_dvr.recording_fps_internal == 20 ? 1 : 0);

        setIdx("dvr_audio",
            c.menu_dvr.audio_enabled ? 1 : 0);
    }
    #endif

    // Language
    {
        UI::MenuItem* item = m_menuRoot->find("language");
        if (item != nullptr)
            item->setSelectedChildIndex(c.system_language == Language_Korean ? 1 : 0);
    }

    // Display Timeout
    {
        UI::MenuItem* item = m_menuRoot->find("display_timeout");
        if (item != nullptr)
        {
            int selIdx = 0; // Always On
            if (c.system_display_timeout == 10)      selIdx = 1;
            else if (c.system_display_timeout == 20) selIdx = 2;
            else if (c.system_display_timeout == 30) selIdx = 3;
            item->setSelectedChildIndex(selIdx);
        }
    }

    // System time fields
    {
        auto setVal = [&](const char* id, float v)
        {
            UI::MenuItem* item = m_menuRoot->find(id);
            if (item != nullptr) item->setValueFloat(v);
        };
        setVal("time_auto",  c.system_time.auto_time ? 1.0f : 0.0f);
        setVal("time_year",  static_cast<float>(c.system_time.year));
        setVal("time_month", static_cast<float>(c.system_time.month));
        setVal("time_day",   static_cast<float>(c.system_time.day));
        setVal("time_hour",  static_cast<float>(c.system_time.hour));
        setVal("time_min",   static_cast<float>(c.system_time.minute));
        setVal("time_sec",   static_cast<float>(c.system_time.second));

        // Keep display names of time fields in sync
        struct { const char* id; std::function<std::string()> fmt; } names[] = {
            { "time_year",  [&]{ return pad4(c.system_time.year);   } },
            { "time_month", [&]{ return pad2(c.system_time.month);  } },
            { "time_day",   [&]{ return pad2(c.system_time.day);    } },
            { "time_hour",  [&]{ return pad2(c.system_time.hour);   } },
            { "time_min",   [&]{ return pad2(c.system_time.minute); } },
            { "time_sec",   [&]{ return pad2(c.system_time.second); } },
        };
        for (auto& n : names)
        {
            UI::MenuItem* item = m_menuRoot->find(n.id);
            if (item != nullptr) item->setName(n.fmt());
        }
    }
}

void AppShell::revertConfigChanges()
{
    APP::appConf = m_configBackup;
    syncMenuFromConfig();
    onConfigReverted();
    LOG_APP_INFO("AppShell: Configuration reverted");
}

void AppShell::saveConfigChanges()
{
    #ifdef USE_DVR
        if (APP::appConf.menu_dvr != m_configBackup.menu_dvr)
        {
            APP::dvrConf.isDvrSettingsChanged = true;
            LOG_APP_INFO("AppShell: DVR settings changed");
        }
    #endif

    m_configBackup = APP::appConf;
    onConfigSaved();

    // Apply display power timeout from saved config
    IO::Platform::getInstance().setDisplayPowerTimeout(APP::appConf.system_display_timeout);

    if (APP::appConf.writeConfig("app_conf.json") == true)
        LOG_APP_SUCCESS("AppShell: Configuration saved");
    else
        LOG_APP_ERROR("AppShell: Failed to save configuration");
}

// ============================================================================
// Menu Display
// ============================================================================

void AppShell::updateMenuDisplay()
{
    if (m_menuNavigator == nullptr || m_menuNavigator->getCurrentItem() == nullptr) return;
    UI::MenuItem* current = m_menuNavigator->getCurrentItem();

    if (m_menuTitleTextView != nullptr) m_menuTitleTextView->setText(TR(current->getName()));
    if (m_menuTitleIconView != nullptr) m_menuTitleIconView->setTexture(current->getIconTexture());

    if (m_menuBackButton != nullptr)
    {
        bool atRoot     = (m_menuNavigator == nullptr || m_menuNavigator->isAtRoot());
        const char* key = atRoot ? "Exit" : "Back";
        m_menuBackButton->setText(TR(key));
    }

    if (m_menuSaveButton != nullptr)
        m_menuSaveButton->setVisibility(m_fileViewEntered == true ? UI::Visibility::GONE : UI::Visibility::VISIBLE);
}

// ============================================================================
// Menu UI helpers
// ============================================================================

void AppShell::applyMenuBackButtonFocus(bool focused)
{
    if (m_menuBackButton != nullptr)
        m_menuBackButton->setBorderColor(focused ? UI::Color::BorderFocus : UI::Color::Transparent, 5.0f);
}

void AppShell::applyMenuSaveButtonFocus(bool focused)
{
    if (m_menuSaveButton != nullptr)
        m_menuSaveButton->setBorderColor(focused ? UI::Color::BorderFocus : UI::Color::Transparent, 5.0f);
}

void AppShell::setMenuTopBarFocus(TopBarFocus newFocus)
{
    TopBarFocus old = m_topBarFocus;
    m_topBarFocus   = newFocus;

    applyMenuBackButtonFocus(newFocus == TopBarFocus::BACK);
    applyMenuSaveButtonFocus(newFocus == TopBarFocus::SAVE);

    if (m_menuNavigator != nullptr)
    {
        bool contentHadFocus = (old      == TopBarFocus::NONE);
        bool contentHasFocus = (newFocus == TopBarFocus::NONE);
        if (contentHadFocus != contentHasFocus)
            m_menuNavigator->setContentFocused(contentHasFocus);
    }
}

// ============================================================================
// Menu Event Handling
// ============================================================================

bool AppShell::handleMenuKeyEvent(const IO::KeyEvent& event)
{
    if (m_menuVisible == false) return false;

    if (m_popupVisible == true)
    {
        if (m_popupView != nullptr) m_popupView->handleKeyEvent(event);
        return true;
    }
    if (m_numpadPopup != nullptr && m_numpadPopup->isShowing() == true)
        { m_numpadPopup->handleKeyEvent(event); return true; }

    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE) return false;

    if (m_menuNavigator != nullptr && m_menuNavigator->isFileMenuActive() == true)
    {
        UI::FileMenuView* fmv = m_menuNavigator->getFileMenuView();
        if (fmv != nullptr && fmv->isPreviewFullscreen() == true)
            fmv->resetFsHudTimer();
    }

    if (event.keyCode == IO::KeyCode::ESCAPE || event.keyCode == IO::KeyCode::BACK)
    {
        if (m_menuNavigator != nullptr && m_menuNavigator->isFileMenuActive() == true)
        {
            UI::FileMenuView* fmv = m_menuNavigator->getFileMenuView();
            if (fmv != nullptr && fmv->isPreviewFullscreen() == true)
                { fmv->exitPreviewFullscreen(); return true; }
        }
        cancelAdjustment();
        if (m_menuNavigator != nullptr) m_menuNavigator->navigateBack();
        setMenuTopBarFocus(TopBarFocus::NONE);
        updateMenuDisplay();
        return true;
    }

    if (m_topBarFocus != TopBarFocus::NONE)
    {
        switch (event.keyCode)
        {
            case IO::KeyCode::DPAD_LEFT:
                if (m_topBarFocus == TopBarFocus::BACK &&
                    m_menuSaveButton != nullptr &&
                    m_menuSaveButton->getVisibility() == UI::Visibility::VISIBLE)
                    setMenuTopBarFocus(TopBarFocus::SAVE);
                return true;

            case IO::KeyCode::DPAD_RIGHT:
                if (m_topBarFocus == TopBarFocus::SAVE) setMenuTopBarFocus(TopBarFocus::BACK);
                return true;

            case IO::KeyCode::DPAD_DOWN:
                setMenuTopBarFocus(TopBarFocus::NONE);
                return true;

            case IO::KeyCode::DPAD_UP:
                return true;

            case IO::KeyCode::DPAD_CENTER:
            case IO::KeyCode::ENTER:
            case IO::KeyCode::SPACE:
                if (m_topBarFocus == TopBarFocus::BACK)
                {
                    cancelAdjustment();
                    if (m_menuNavigator != nullptr) m_menuNavigator->navigateBack();
                    setMenuTopBarFocus(TopBarFocus::NONE);
                    updateMenuDisplay();
                }
                else if (m_topBarFocus == TopBarFocus::SAVE)
                    closeMenu(true);
                return true;

            default:
                return false;
        }
    }

    if (event.keyCode == IO::KeyCode::DPAD_UP)
    {
        bool handled = (m_menuNavigator != nullptr) ? m_menuNavigator->handleKeyEvent(event) : false;
        if (handled == false) setMenuTopBarFocus(TopBarFocus::BACK);
        else                  updateMenuDisplay();
        return true;
    }

    if (m_menuNavigator != nullptr)
    {
        bool handled = m_menuNavigator->handleKeyEvent(event);
        updateMenuDisplay();
        return handled;
    }
    return false;
}

bool AppShell::handleMenuMotionEvent(const IO::MotionEvent& event)
{
    if (m_menuVisible == false) return false;

    if (m_menuNavigator != nullptr && m_menuNavigator->isFileMenuActive() == true)
    {
        UI::FileMenuView* fmv = m_menuNavigator->getFileMenuView();
        if (fmv != nullptr && fmv->isPreviewFullscreen() == true)
            { fmv->onMotionEvent(event); return true; }
    }

    if (m_popupVisible == true)
    {
        if (m_popupView != nullptr) m_popupView->handleMotionEvent(event);
        return true;
    }
    if (m_numpadPopup != nullptr && m_numpadPopup->isShowing() == true)
        { m_numpadPopup->handleMotionEvent(event); return true; }

    auto handleTopBarBtn = [&](UI::Button* btn, TopBarFocus focusVal,
                                std::function<void()> onConfirm) -> bool
    {
        if (btn == nullptr) return false;
        const UI::RectF& bounds = btn->getBounds();
        bool inside = bounds.contains(event.x, event.y);
        if (event.canHover() == true)
        {
            if (inside == true  && m_topBarFocus != focusVal) { setMenuTopBarFocus(focusVal);          btn->setHovered(true);  }
            if (inside == false && m_topBarFocus == focusVal) { setMenuTopBarFocus(TopBarFocus::NONE); btn->setHovered(false); }
        }
        if (inside == true && event.action == IO::MotionAction::DOWN)
        {
            btn->setPressed(true);
            if (event.canHover() == false) setMenuTopBarFocus(focusVal);
            return true;
        }
        if (event.action == IO::MotionAction::UP && btn->isPressed() == true)
        {
            btn->setPressed(false);
            if (inside == true)                  onConfirm();
            else if (event.canHover() == false)  setMenuTopBarFocus(TopBarFocus::NONE);
            return true;
        }
        return false;
    };

    if (handleTopBarBtn(m_menuBackButton, TopBarFocus::BACK, [this]()
    {
        cancelAdjustment();
        if (m_menuNavigator != nullptr) m_menuNavigator->navigateBack();
        setMenuTopBarFocus(TopBarFocus::NONE);
        updateMenuDisplay();
    }) == true) return true;

    if (m_menuSaveButton != nullptr && m_menuSaveButton->getVisibility() == UI::Visibility::VISIBLE)
    {
        if (handleTopBarBtn(m_menuSaveButton, TopBarFocus::SAVE,
                             [this]() { closeMenu(true); }) == true) return true;
    }

    if (m_menuNavigator != nullptr)
    {
        bool handled = m_menuNavigator->handleMotionEvent(event);
        updateMenuDisplay();
        return handled;
    }
    return false;
}

// ============================================================================
// DVR File Management
// ============================================================================

#ifdef USE_DVR

    UI::ImageView* AppShell::getFileMenuPreviewImage() const
    {
        if (m_menuNavigator == nullptr) return nullptr;
        UI::FileMenuView* fmv = m_menuNavigator->getFileMenuView();
        return (fmv != nullptr) ? fmv->getPreviewImage() : nullptr;
    }

    void AppShell::updateFilePlayerTexture()
    {
        if (m_fileViewEntered == false) return;
        UI::ImageView* previewImage = getFileMenuPreviewImage();
        if (previewImage == nullptr) return;

        UI::Texture* videoTex = m_filePlayer.getVideoTexture();
        if (videoTex != nullptr && videoTex->isValid() == true)
        {
            previewImage->setTexture(videoTex, false);
        }
        else if (m_filePlayer.getState() == VPB::PlaybackState::STOPPED)
        {
            UI::Texture* bgTex = UI::TextureManager::getInstance().getTextureByName("08_bg_fileview");
            if (bgTex != nullptr) previewImage->setTexture(bgTex);
        }

        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;
        if (fmv != nullptr)
        {
            int64_t pos = m_filePlayer.getCurrentPosition();
            int64_t dur = m_filePlayer.getDuration();
            fmv->setPlaybackTime(TimeUtils::formatMilliseconds(pos) + " / " + TimeUtils::formatMilliseconds(dur));
            const VPB::SubtitleData* sub = m_filePlayer.getCurrentSubtitle();
            if (sub != nullptr && sub->rawText.empty() == false)
            {
                VPB::SubtitleStyle style;
                auto [line1, line2] = VPB::SubtitleFormatter::formatTwoLines(*sub, style);
                fmv->setSubtitleLines(line1, line2);

                // fmv->setSubtitleLines("", sub->rawText);
            }
            else fmv->setSubtitleLines("", "");
        }
    }

    void AppShell::enterFileView()
    {
        m_fileViewEntered = true;
        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;
        if (fmv != nullptr && fmv->getLogoImage() != nullptr)
        {
            UI::Texture* logoTex = UI::TextureManager::getInstance().getTextureByName("08_bg_logo_product");
            if (logoTex != nullptr) fmv->getLogoImage()->setTexture(logoTex);
        }

        const std::string rootDir = APP::dvrConf.storageDevPath + "/VideoFiles";
        m_fileManager.setRootDirectory(rootDir);
        m_fileManager.setCurrentDirectory(rootDir);

        m_fileManager.setExtensionFilter({"mp4", "avi", "mkv"});
        m_fileViewSelectedGroupId = 0;
        m_fileViewSelectedFileId  = 0;
        rebuildGroupsAfterDirectoryChange();
    }

    void AppShell::exitFileView()
    {
        m_fileViewEntered = false;
        if (m_filePlayer.isFileLoaded() == true) { m_filePlayer.stop(); m_filePlayer.unloadFile(); }
        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;
        if (fmv != nullptr) fmv->setFileLoaded(false);
        updateMenuDisplay();
    }

    void AppShell::onFileViewFileSelected(int groupIdx, int fileIdxInGroup)
    {
        VPB::GroupInfo* group = m_fileManager.getGroupAt(groupIdx);
        if (group == nullptr) return;
        VPB::FileInfo* file = m_fileManager.getFileAt(group->startIndex + fileIdxInGroup);
        if (file == nullptr) return;

        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;

        if (file->isDirectory == true)
        {
            bool ok = file->isParentDir() ? m_fileManager.navigateToParent()
                                           : m_fileManager.navigateToChild(file->name);
            if (ok == true)
            {
                m_fileViewSelectedGroupId = 0;
                m_fileViewSelectedFileId  = 0;
                rebuildGroupsAfterDirectoryChange();
            }
        }
        else
        {
            m_fileViewSelectedGroupId = groupIdx;
            m_fileViewSelectedFileId  = fileIdxInGroup;
            if (m_filePlayer.isFileLoaded() == true) { m_filePlayer.stop(); m_filePlayer.unloadFile(); }
            if (m_filePlayer.loadFile(file->path) == true)
            {
                const VPB::FileMetadata& meta = m_filePlayer.getMetadata();
                if (meta.isValid == false || meta.videoTrackCount <= 0 || meta.duration <= 0)
                {
                    LOG_APP_WARNINGF("AppShell: File metadata invalid: %s", file->path.c_str());
                    m_filePlayer.stop();
                    m_filePlayer.unloadFile();
                    if (fmv != nullptr) fmv->setFileLoaded(false);
                    showPopup(PopupType::FileInvalid);
                }
                else
                {
                    int numChannelButtons = (fmv != nullptr) ? static_cast<int>(fmv->getChannelButtonCount()) : 0;
                    if (numChannelButtons <= 0) numChannelButtons = 10;
                    const int tracks = meta.videoTrackCount;
                    std::vector<bool> channelAvail(numChannelButtons, false);
                    for (int i = 0; i < numChannelButtons; ++i)
                    {
                        if      (i < 4)  channelAvail[i] = (i < tracks);
                        else if (i == 4) channelAvail[i] = (tracks >= 4);
                        else if (i < 9)  channelAvail[i] = ((i - 1) < tracks);
                        else if (i == 9) channelAvail[i] = (tracks >= 8);
                    }
                    if (fmv != nullptr) fmv->onFileOpened(channelAvail);
                }
            }
            else
            {
                LOG_APP_ERRORF("AppShell: Failed to open file: %s", file->path.c_str());
                if (fmv != nullptr) fmv->setFileLoaded(false);
                showPopup(PopupType::FileInvalid);
            }
        }
    }

    void AppShell::onFileViewFileUnload()
    {
        if (m_filePlayer.isFileLoaded() == true) { m_filePlayer.stop(); m_filePlayer.unloadFile(); }
        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;
        if (fmv != nullptr) fmv->setFileLoaded(false);
    }

    void AppShell::updateSelectedFileIndex() {}

    void AppShell::rebuildGroupsAfterDirectoryChange()
    {
        m_fileManager.scanDirectory();
        m_fileManager.clearGroups();

        int32_t fileCount  = m_fileManager.getFileCount();
        int32_t groupCount = (fileCount + m_fileViewGroupMaxSize - 1) / m_fileViewGroupMaxSize;

        for (int32_t i = 0; i < groupCount; ++i)
        {
            int32_t startIdx = i * m_fileViewGroupMaxSize;
            int32_t count    = std::min(m_fileViewGroupMaxSize, fileCount - startIdx);
            m_fileManager.createGroup(
                std::to_string(startIdx + 1) + " ~ " + std::to_string(startIdx + count),
                startIdx, count);
        }

        if (m_fileViewSelectedGroupId >= groupCount)
            m_fileViewSelectedGroupId = std::max(0, groupCount - 1);

        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;
        if (fmv != nullptr)
            fmv->populateFileList(&m_fileManager, m_fileViewSelectedGroupId, groupCount, fileCount);
    }

    void AppShell::startPausePlayback()
    {
        if (m_filePlayer.isFileLoaded() == false) return;
        if (m_filePlayer.getState() == VPB::PlaybackState::PLAYING) m_filePlayer.pause();
        else                                                          m_filePlayer.play();
    }

    void AppShell::stopPlayback() { m_filePlayer.stop(); }

    void AppShell::playbackSeekBackward()
    {
        if (m_filePlayer.isFileLoaded() == false) return;
        int64_t newPos = std::max(int64_t(0), m_filePlayer.getCurrentPosition() - 5000);
        m_filePlayer.seek(newPos);
    }

    void AppShell::playbackSeekForward()
    {
        if (m_filePlayer.isFileLoaded() == false) return;
        int64_t newPos = std::min(m_filePlayer.getDuration(), m_filePlayer.getCurrentPosition() + 5000);
        m_filePlayer.seek(newPos);
    }

    void AppShell::onPlaybackStateChanged(VPB::PlaybackState oldState, VPB::PlaybackState newState)
    {
        // LOG_APP_INFOF("AppShell: Playback state %d → %d", static_cast<int>(oldState), static_cast<int>(newState));
        UI::FileMenuView* fmv = (m_menuNavigator != nullptr) ? m_menuNavigator->getFileMenuView() : nullptr;
        if (fmv != nullptr) fmv->setPlaybackState(newState);
        if (newState == VPB::PlaybackState::STOPPED)
        {
            UI::ImageView* previewImage = getFileMenuPreviewImage();
            if (previewImage != nullptr)
            {
                UI::Texture* bgTex = UI::TextureManager::getInstance().getTextureByName("08_bg_fileview");
                if (bgTex != nullptr) previewImage->setTexture(bgTex);
            }
            if (fmv != nullptr) fmv->setSubtitleLines("", "");
        }
    }

#endif // USE_DVR

} // namespace APP
