#ifndef APP_SHELL_H
#define APP_SHELL_H

#include "app_conf.h"
#include "app_base.h"
#include "ui_core.h"
#include "ui_elements.h"
#include "ui_focus.h"
#include "ui_menu.h"
#include "ui_locale.h"
#include "constants.h"
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <array>
#include <functional>

#ifdef USE_DVR
    #include "vpb_file_manager.h"
    #include "vpb_file_player.h"
#endif

namespace APP
{

// ── Shared enums ──────────────────────────────────────────────────────────────

enum class AdjustmentType { AdjNone, CamROI, DateTime, NetworkIP, NetworkPW,
                            CamViewR2D, CamViewV3D, CalibFeaturePoint, CalibParams };
enum class PopupType      { PopupNone, FileInvalid, FileSingleCopy, FileSingleDelete,
                            FileGroupCopy, SystemFormat };

// ── StatusBar ─────────────────────────────────────────────────────────────────

class StatusBar : public UI::FrameLayout
{
public:
    static constexpr int HEIGHT   = 72;   // inner bar height

    StatusBar();

    // ── Visibility toggle ─────────────────────────────────────────────────
    bool isBarVisible()   const { return m_barVisible; }
    void setBarVisible(bool visible);
    void toggleBarVisible()     { setBarVisible(!m_barVisible); }
    void setOnVisibilityChanged(std::function<void(bool)> cb) { m_onVisibilityChanged = cb; }

    // ── Slot accessors ────────────────────────────────────────────────────
    UI::LinearLayout* getStatusArea()   const { return m_statusArea; }
    UI::ImageView*    getLogoView()     const { return m_logoView; }
    UI::Button*       getMenuButton()   const { return m_menuButton; }
    UI::FrameLayout*  getTogglePanel()  const { return m_togglePanel; }
    UI::Button*       getToggleButton() const { return m_toggleButton; }

    // ── Logo ──────────────────────────────────────────────────────────────
    void setLogoTexture(UI::Texture* tex);

    // ── Menu button icon (text + callback wired by AppShell) ──────────────
    void setupMenuButton(UI::Texture* iconTex);

    // ── Helpers for derived classes to add status slots ───────────────────
    UI::TextView*  addTextSlot(float textSize = 36.0f);
    UI::ImageView* addImageSlot(int width = 48, int height = 48);
    void           addDivider();

private:
    bool               m_barVisible;
    UI::FrameLayout*   m_togglePanel;
    UI::Button*        m_toggleButton;
    UI::FrameLayout*   m_innerBar;
    UI::ImageView*     m_logoView;
    UI::LinearLayout*  m_statusArea;
    UI::Button*        m_menuButton;
    std::function<void(bool)> m_onVisibilityChanged;
};

// ── AppShell ──────────────────────────────────────────────────────────────────
// Intermediate base providing: status bar wiring, menu screen, popup, numpad,
// focus system, config save/revert, datetime adjustment, password management,
// DVR file/playback, and the System menu category.
//
// Derived classes implement:
//   setupMainScreenContent()  — build the app-specific main screen layout
//   buildExtraMenuItems()     — append extra top-level menu items before System
//   onConfigReverted()        — optional hook after config revert

class AppShell : public AppBase
{
public:
    AppShell();
    ~AppShell() override;

    // ── State queries ──────────────────────────────────────────────────────
    bool isMenuVisible()        const { return m_menuVisible; }

    #ifdef USE_DVR
        bool isFileViewActive() const { return m_fileViewEntered; }
    #endif

protected:
    // ── AppBase lifecycle ──────────────────────────────────────────────────
    bool onInitialize()                                 override;
    void onCleanup()                                    override;
    void onUpdate(float deltaTime)                      override;
    void onRender()                                     override;
    bool onKeyEvent(const IO::KeyEvent& event)          override;
    bool onMotionEvent(const IO::MotionEvent& event)    override;
    bool onTerminalCommand(const std::string& command)  override;

    // ── Hooks for derived classes ──────────────────────────────────────────
    virtual void setupMainScreenContent()                                            = 0;
    virtual void buildExtraMenuItems()                                               {}
    virtual void buildExtraSystemMenuItems(std::shared_ptr<UI::MenuItem> /*parent*/) {}
    virtual void buildShellMenuItems();
    virtual void onCancelAdjustment()                                                {}
    virtual void onConfigReverted()                                                  {}
    virtual void onConfigSaved()                                                     {}
    virtual void syncMenuFromConfig();
    virtual void onStatusBarCreated();

    // ── Status bar ─────────────────────────────────────────────────────────
    void       setupStatusBar();
    StatusBar* getStatusBar()       const { return m_statusBar; }
    int        getStatusBarHeight() const;

    // ── Init helpers ───────────────────────────────────────────────────────
    void loadTextures();
    void loadLocale();
    void loadConfigValues();
    void setupMenuScreen();
    void setupMenuPopup();
    void initializeFocusSystem(const std::vector<UI::View*>& extraMainViews = {});
    void initializeMenuSystem();

    // ── Menu tree builders ─────────────────────────────────────────────────
    void buildMenuTree();
    void buildDvrMenuItems(std::shared_ptr<UI::MenuItem> parent);
    void buildFileMenuItems(std::shared_ptr<UI::MenuItem> parent);
    void buildSystemMenuItems(std::shared_ptr<UI::MenuItem> parent);

    // ── Screen management ──────────────────────────────────────────────────
    void showMainScreen();
    void showMenuScreen();
    void closeMenu(bool save);

    // ── Adjustment ─────────────────────────────────────────────────────────
    void         startAdjustment(AdjustmentType type, int steps);
    void         cancelAdjustment();
    void         confirmAdjustment();
    virtual void stepEntered(int step);
    void         startAdjustmentDateTime();
    void         adjustDateTime(int fieldDelta, int valueDelta);

    // ── Hint ───────────────────────────────────────────────────────────────
    void setMenuHint(const std::string& text, UI::Color4 color = UI::Color::Yellow);
    void clearMenuHint();

    // ── Password / Numpad ──────────────────────────────────────────────────
    void showNumpad(const std::string& title,
                    std::function<void(std::array<int,6>)> onConfirm,
                    std::function<void()> onCancel = nullptr);
    void showPwPopup(const std::string& msgKey);
    bool hasStoredPassword() const { return m_passwordIsSet; }
    static bool passwordsMatch(const std::array<int,6>& a, const std::array<int,6>& b);
    static std::array<int,6> stringToPasswordDigits(const std::string& s);

    // ── Popup ──────────────────────────────────────────────────────────────
    void showPopup(PopupType type);
    void hidePopup();
    void executePopupAction();

    // ── Config ─────────────────────────────────────────────────────────────
    void revertConfigChanges();
    void saveConfigChanges();

    // ── Menu display ───────────────────────────────────────────────────────
    void updateMenuDisplay();

    // ── DVR file management ────────────────────────────────────────────────
    #ifdef USE_DVR
        void           enterFileView();
        void           exitFileView();
        void           onFileViewFileSelected(int groupIdx, int fileIdxInGroup);
        void           onFileViewFileUnload();
        void           updateSelectedFileIndex();
        void           rebuildGroupsAfterDirectoryChange();
        void           startPausePlayback();
        void           stopPlayback();
        void           playbackSeekBackward();
        void           playbackSeekForward();
        void           onPlaybackStateChanged(VPB::PlaybackState oldState, VPB::PlaybackState newState);
        UI::ImageView* getFileMenuPreviewImage() const;
        void           updateFilePlayerTexture();
    #endif

    // ── UI helpers ─────────────────────────────────────────────────────────
    enum class TopBarFocus { NONE, SAVE, BACK };
    void setMenuTopBarFocus(TopBarFocus f);
    void applyMenuBackButtonFocus(bool focused);
    void applyMenuSaveButtonFocus(bool focused);
    bool handleMenuKeyEvent(const IO::KeyEvent& event);
    bool handleMenuMotionEvent(const IO::MotionEvent& event);

    // ── DateTime helpers ───────────────────────────────────────────────────
    static void        applySystemTime(const SystemTime& st);
    static void        syncSystemTimeFromOS(SystemTime& st);
    static std::string pad2(int v);
    static std::string pad4(int v);

    // ── Control button repeat ──────────────────────────────────────────────
    void updateCtrlBtnRepeats(float deltaTime);

    // ─────────────────────────────────────────────────────────────────────
    // Members visible to derived classes
    // ─────────────────────────────────────────────────────────────────────

    // ── Screen layouts ─────────────────────────────────────────────────────
    UI::FrameLayout*  m_mainScreen;
    UI::LinearLayout* m_menuScreen;

    // ── Status bar ─────────────────────────────────────────────────────────
    StatusBar*        m_statusBar;

    // ── Common status bar slots (BLE icon + CPU + datetime) ───────────────
    UI::ImageView*    m_barBleSlot;
    UI::TextView*     m_barCpuSlot;
    UI::TextView*     m_barMemSlot;
    float             m_cpuUpdateTimer    = 0.0f;
    unsigned long     m_cpuPrevProcTicks  = 0;
    long              m_cpuClkTck         = 0;
    UI::TextView*     m_barDateTimeSlot;

    #ifdef USE_DVR
        UI::ImageView* m_barRecSlot;
        UI::TextView*  m_barStorageUsageSlot;
        float          m_storageUpdateTimer;
    #endif

    #ifdef USE_CAN
        UI::TextView*  m_barCanSpeedSlot;
        UI::TextView*  m_barCanRpmSteerSlot;
        UI::ImageView* m_barCanGearSlot;
        UI::ImageView* m_barCanSignalLeftSlot;
        UI::ImageView* m_barCanSignalRightSlot;
    #endif

    // ── Focus ──────────────────────────────────────────────────────────────
    UI::FocusManager                  m_focusManager;
    std::shared_ptr<UI::FocusContext> m_mainScreenContext;

    // ── Menu screen ────────────────────────────────────────────────────────
    UI::ImageView*    m_menuLogoView;
    UI::LinearLayout* m_menuControlsBar;
    UI::TextView*     m_hintTextView;
    UI::Button*       m_ctrlBtnUp;
    UI::Button*       m_ctrlBtnDown;
    UI::Button*       m_ctrlBtnLeft;
    UI::Button*       m_ctrlBtnRight;
    UI::Button*       m_ctrlBtnOk;
    UI::ImageView*    m_menuTitleIconView;
    UI::TextView*     m_menuTitleTextView;
    UI::FrameLayout*  m_menuContentLayout;
    UI::Button*       m_menuBackButton;
    UI::Button*       m_menuSaveButton;
    TopBarFocus       m_topBarFocus;

    std::shared_ptr<UI::MenuItem>      m_menuRoot;
    std::unique_ptr<UI::MenuNavigator> m_menuNavigator;
    UI::FocusManager                   m_menuFocusManager;

    UI::PopupView*   m_popupView;
    UI::NumpadPopup* m_numpadPopup;

    // ── State ──────────────────────────────────────────────────────────────
    bool           m_menuVisible;
    bool           m_fileViewEntered;
    AdjustmentType m_adjustmentType;
    int            m_adjustmentSteps;
    int            m_adjustmentStepIndex;

    int               m_timeFieldIndex;
    UI::MenuItem*     m_timeRowItem;
    UI::MenuItem*     m_timeMenuParent;
    std::atomic<bool> m_timePillsNeedUpdate;

    AppConfiguration m_configBackup;

    bool                                   m_passwordIsSet;
    bool                                   m_numpadPendingShow;
    std::string                            m_numpadPendingTitle;
    std::string                            m_pendingLangChange;
    std::string                            m_pendingPwMsg;
    std::function<void(std::array<int,6>)> m_numpadPendingConfirm;
    std::function<void()>                  m_numpadPendingCancel;

    PopupType m_popupType;
    bool      m_popupVisible;

    #ifdef USE_DVR
        VPB::FileManager m_fileManager;
        VPB::FilePlayer  m_filePlayer;
        int32_t          m_fileViewSelectedGroupId;
        int32_t          m_fileViewSelectedFileId;
        int32_t          m_fileViewGroupMaxSize;
        std::string      m_fileViewSelectedFileName;
    #endif

    struct CtrlBtnRepeat
    {
        UI::Button*  btn          = nullptr;
        IO::KeyCode  keyCode      = IO::KeyCode::UNKNOWN;
        float        holdTime     = 0.0f;
        float        repeatAccum  = 0.0f;
        bool         repeatActive = false;
    };
    std::array<CtrlBtnRepeat, 4> m_ctrlBtnRepeats;
};

} // namespace APP

#endif // APP_SHELL_H