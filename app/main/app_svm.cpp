#include "app_svm.h"
#include "app_vars.h"
#include "logger.h"
#include "ui_locale.h"
#include <ctime>
#include <cstdio>
#include <cmath>

extern const char* Goto_CalibUi;
extern void Write_Webserver(const char* write_buffer);

namespace APP
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

AppSvm::AppSvm()
    : AppShell()
    , m_svmImageView(nullptr)
    , m_layout1FbRect(0.0f, 0.0f, 0.0f, 0.0f)
{}

AppSvm::~AppSvm() {}

// ============================================================================
// Lifecycle
// ============================================================================

bool AppSvm::onInitialize()
{
    if (AppShell::onInitialize() == false) return false;
    if (m_svmFb.create(WND_WIDTH, WND_HEIGHT) == false)
    {
        LOG_APP_ERROR("AppSvm: Failed to create SVM framebuffer");
        return false;
    }
    LOG_APP_SUCCESSF("AppSvm: SVM framebuffer created [%d, %d]", WND_WIDTH, WND_HEIGHT);
    return true;
}

void AppSvm::onCleanup()
{
    m_svmFb.release();
    AppShell::onCleanup();
}

void AppSvm::onUpdate(float deltaTime) { AppShell::onUpdate(deltaTime); }

void AppSvm::onRender()
{
    AppShell::onRender();
    if (m_menuVisible == true && m_menuNavigator != nullptr)
    {
        APP::appConf.menu_svm.visible = true;
        UI::MenuItem*     current     = m_menuNavigator->getCurrentItem();
        const std::string id          = (current != nullptr) ? current->getId() : "";
        if (id == "view" || id == "activation") updateSvmPreview();
    }
    else
    {
        APP::appConf.menu_svm.visible = false;
    }
}

// ============================================================================
// Key event
// ============================================================================

bool AppSvm::onKeyEvent(const IO::KeyEvent& event)
{
    if (m_adjustmentType == AdjustmentType::CamViewV3D)
    {
        if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE)
            return false;

        if (event.keyCode == IO::KeyCode::ESCAPE)
        {
            if (m_svmAnimator != nullptr)
                { m_svmAnimator->resetToDefaultPose(); m_svmAnimator->setInteractive(false); }
            cancelAdjustment();
            clearMenuHint();
            if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
            return true;
        }

        if (event.keyCode == IO::KeyCode::DPAD_CENTER ||
            event.keyCode == IO::KeyCode::ENTER       ||
            event.keyCode == IO::KeyCode::SPACE)
        {
            if (event.action == IO::KeyAction::DOWN)
            {
                if (m_svmAnimator != nullptr)
                {
                    glm::vec3 pos, ori;
                    m_svmAnimator->getCurrentPose(pos, ori);
                    auto& v = APP::appConf.menu_svm.vcam[m_v3dAdjCamIdx];
                    v = { pos.x, pos.y, pos.z, ori.x, ori.y, ori.z };
                    m_svmAnimator->setInteractive(false);
                }
                confirmAdjustment();
                clearMenuHint();
                if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
            }
            return true;
        }

        const bool left  = (event.keyCode == IO::KeyCode::DPAD_LEFT);
        const bool right = (event.keyCode == IO::KeyCode::DPAD_RIGHT);
        const bool up    = (event.keyCode == IO::KeyCode::DPAD_UP);
        const bool down  = (event.keyCode == IO::KeyCode::DPAD_DOWN);

        if (left || right || up || down)
        {
            if (event.holdTime >= kLongPressThreshold)
            {
                if (left)  accumulateOrbit(-kOrbitStep, 0.0f);
                if (right) accumulateOrbit( kOrbitStep, 0.0f);
                if (m_svmAnimator != nullptr)
                {
                    if (up)   m_svmAnimator->nudgeRadius(-kZoomStep);
                    if (down) m_svmAnimator->nudgeRadius( kZoomStep);
                }
            }
            else
            {
                if (left)  accumulateOrbit(-kOrbitStep,  0.0f);
                if (right) accumulateOrbit( kOrbitStep,  0.0f);
                if (up)    accumulateOrbit( 0.0f, -kOrbitStep);
                if (down)  accumulateOrbit( 0.0f,  kOrbitStep);
            }
            if (m_svmAnimator != nullptr)
                { m_svmAnimator->pushCameraInput(m_cameraInput); m_cameraInput = CameraInput{}; }
            return true;
        }
        return false;
    }

    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE)
        return AppShell::onKeyEvent(event);

    if (m_adjustmentType == AdjustmentType::CamViewR2D)
    {
        if (event.keyCode == IO::KeyCode::ESCAPE)
            { cancelAdjustment(); return true; }
        if (event.keyCode == IO::KeyCode::DPAD_CENTER ||
            event.keyCode == IO::KeyCode::ENTER       ||
            event.keyCode == IO::KeyCode::SPACE)
        {
            confirmAdjustment();
            if (m_adjustmentType == AdjustmentType::AdjNone)
            {
                clearMenuHint();
                if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
            }
            return true;
        }
        const bool left  = (event.keyCode == IO::KeyCode::DPAD_LEFT);
        const bool right = (event.keyCode == IO::KeyCode::DPAD_RIGHT);
        const bool up    = (event.keyCode == IO::KeyCode::DPAD_UP);
        const bool down  = (event.keyCode == IO::KeyCode::DPAD_DOWN);
        if (left || right || up || down)
            { applyAdjustCamViewR2D(left, right, up, down); return true; }
    }

    return AppShell::onKeyEvent(event);
}

// ============================================================================
// Motion event
// ============================================================================

bool AppSvm::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_adjustmentType == AdjustmentType::CamViewV3D)
    {
        if (handleAdjV3DMotion(event) == true)
        {
            if (m_svmAnimator != nullptr)
                { m_svmAnimator->pushCameraInput(m_cameraInput); m_cameraInput = CameraInput{}; }
            return true;
        }
        return AppShell::onMotionEvent(event);
    }

    if (m_interactiveMode == false || m_menuVisible == true)
        return AppShell::onMotionEvent(event);

    // Double-tap → reset pose to current view's default
    if (event.action == IO::MotionAction::DOWN && isLayout1Hit(event.x, event.y) == true)
    {
        const float dx = event.x - m_lastTapPos.x, dy = event.y - m_lastTapPos.y;
        if ((dx*dx + dy*dy) <= kDoubleTapMaxDist2 &&
            (event.eventTime - m_lastTapTime) * 1e-9f <= kDoubleTapMaxSec &&
            m_svmAnimator != nullptr)
        {
            m_svmAnimator->resetToDefaultPose();
            m_lastTapTime = 0;
            m_lastTapPos  = { 0.0f, 0.0f };
            return true;
        }
        m_lastTapTime = event.eventTime;
        m_lastTapPos  = { event.x, event.y };
    }

    auto isMainHit = [&](float hx, float hy) -> bool { return isLayout1Hit(hx, hy); };
    const bool handled = handleOrbitZoomGesture(event, isMainHit,
                                                m_pointerDown,   m_twoFingerActive,
                                                m_prevSpan,      m_lastPointer);
    if (handled == false)
    {
        if (event.action == IO::MotionAction::DOWN ||
            event.action == IO::MotionAction::POINTER_DOWN)
            return AppShell::onMotionEvent(event);
        return false;
    }
    return true;
}

// ============================================================================
// Main screen setup
// ============================================================================

void AppSvm::setupMainScreenContent()
{
    m_svmImageView = new UI::ImageView();
    m_svmImageView->getLayoutParams().width   = UI::MATCH_PARENT;
    m_svmImageView->getLayoutParams().height  = UI::MATCH_PARENT;
    m_svmImageView->getLayoutParams().gravity = UI::Gravity::CENTER;
    m_svmImageView->setScaleType(UI::ScaleType::FIT_XY);
    m_svmImageView->setCornerRadius(24.0f);
    m_mainScreen->addView(m_svmImageView);

    initializeFocusSystem();
    loadSvmConfigValues();
}

// ============================================================================
// Menu setup
// ============================================================================

void AppSvm::buildExtraMenuItems()
{
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();
    {
        auto viewMenu = std::make_shared<UI::MenuItem>("view", TR("View"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(viewMenu, "View");
        UI::Texture* icn = texMgr.getTextureByName("04_icn_view");
        if (icn != nullptr) viewMenu->setIconTexture(icn);
        viewMenu->setShowPreview(true);
        buildViewMenuItems(viewMenu);
        m_menuRoot->addChild(viewMenu);
    }
    {
        auto actMenu = std::make_shared<UI::MenuItem>("activation", TR("Activation"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(actMenu, "Activation");
        UI::Texture* icn = texMgr.getTextureByName("04_icn_activation");
        if (icn != nullptr) actMenu->setIconTexture(icn);
        actMenu->setShowPreview(true);
        buildActivationMenuItems(actMenu);
        m_menuRoot->addChild(actMenu);
    }
}

void AppSvm::buildViewMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    struct ViewDef { const char* id, *locKey, *opt0Id, *opt1Id, *opt0Label, *opt1Label; int confIdx, camIdx; };
    static const ViewDef views[] = {
        { "view_front", "Front_View", "v_front_2d", "v_front_3d", "2D", "3D", 0, 0 },
        { "view_right", "Right_View", "v_right_2d", "v_right_3d", "2D", "3D", 1, 1 },
        { "view_rear",  "Rear_View",  "v_rear_2d",  "v_rear_3d",  "2D", "3D", 2, 2 },
        { "view_left",  "Left_View",  "v_left_2d",  "v_left_3d",  "2D", "3D", 3, 3 },
        { "view_emg",   "Emergency",  "v_emg_3df",  "v_emg_3dr",  "3D_Front", "3D_Rear", 4, 0 },
    };
    for (const auto& v : views)
    {
        auto row  = std::make_shared<UI::MenuItem>(v.id,     TR(v.locKey), UI::MenuItemType::CATEGORY);
        auto opt0 = std::make_shared<UI::MenuItem>(v.opt0Id, v.opt0Label,  UI::MenuItemType::ACTION);
        auto opt1 = std::make_shared<UI::MenuItem>(v.opt1Id, v.opt1Label,  UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(row,  v.locKey);
        SET_LOCALIZED_MENU_ITEM(opt0, v.opt0Label);
        SET_LOCALIZED_MENU_ITEM(opt1, v.opt1Label);
        row->setShowPreview(true);

        const int ci = v.confIdx, mi = v.camIdx;
        opt0->setOnActivate([this, ci, mi](UI::MenuItem*) { APP::appConf.menu_svm.view_setting[ci] = 0; startAdjustCamViewR2D(mi); });
        opt1->setOnActivate([this, ci, mi](UI::MenuItem*) { APP::appConf.menu_svm.view_setting[ci] = 1; startAdjustCamViewV3D(mi); });
        row->setSelectedChildIndex(APP::appConf.menu_svm.view_setting[ci]);
        row->setOnValueChanged([ci](UI::MenuItem* item) { APP::appConf.menu_svm.view_setting[ci] = item->getSelectedChildIndex(); });
        row->addChild(opt0);
        row->addChild(opt1);
        parent->addChild(row);
    }
}

void AppSvm::buildActivationMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    struct ActDef { const char* id, *locKey; int idx; };
    static const ActDef acts[] = {
        { "act_vbc",  "VBC",  0 }, { "act_pgs",  "PGS",  1 }, { "act_dgs",  "DGS",  2 },
        { "act_od",   "OD",   3 }, { "act_mobs", "MOBS", 4 },
    };
    for (const auto& a : acts)
    {
        auto row    = std::make_shared<UI::MenuItem>(a.id, TR(a.locKey), UI::MenuItemType::CATEGORY);
        auto optOff = std::make_shared<UI::MenuItem>((std::string(a.id)+"_off").c_str(), TR("OFF"), UI::MenuItemType::ACTION);
        auto optOn  = std::make_shared<UI::MenuItem>((std::string(a.id)+"_on").c_str(),  TR("ON"),  UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(row, a.locKey);
        SET_LOCALIZED_MENU_ITEM(optOff, "OFF");
        SET_LOCALIZED_MENU_ITEM(optOn,  "ON");
        row->setShowPreview(false);
        row->setSelectedChildIndex(APP::appConf.menu_svm.activation[a.idx] ? 1 : 0);
        const int idx = a.idx;
        row->setOnValueChanged([idx](UI::MenuItem* item) { APP::appConf.menu_svm.activation[idx] = (item->getSelectedChildIndex() == 1); });
        row->addChild(optOff);
        row->addChild(optOn);
        parent->addChild(row);
    }
}

void AppSvm::buildExtraSystemMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    auto row      = std::make_shared<UI::MenuItem>("vehicle",       TR("Vehicle"),             UI::MenuItemType::CATEGORY);
    auto calibBtn = std::make_shared<UI::MenuItem>("vehicle_calib", TR("Capture_Calibration"), UI::MenuItemType::ACTION);
    SET_LOCALIZED_MENU_ITEM(row,      "Vehicle");
    SET_LOCALIZED_MENU_ITEM(calibBtn, "Capture_Calibration");
    row->setShowPreview(false);
    calibBtn->setOnActivate([this](UI::MenuItem*)
    {
        if (hasStoredPassword() == true)
            showNumpad(TR("Calib_EnterPassword"), [this](std::array<int,6> entered)
            {
                if (passwordsMatch(entered, APP::appConf.system_calib_pw) == true) goToCalibration();
                else showPwPopup("Calib_WrongPassword");
            });
        else
            goToCalibration();
    });
    row->addChild(calibBtn);
    parent->addChild(row);
}

void AppSvm::onConfigReverted() { loadSvmConfigValues(); }

void AppSvm::syncMenuFromConfig()
{
    AppShell::syncMenuFromConfig();
    if (m_menuRoot == nullptr) return;
    const M_MENU_SVM& svm = APP::appConf.menu_svm;
    struct { const char* id; int ci; } views[] = {
        {"view_front",0},{"view_right",1},{"view_rear",2},{"view_left",3},{"view_emg",4} };
    for (auto& v : views) { UI::MenuItem* it = m_menuRoot->find(v.id); if (it != nullptr) it->setSelectedChildIndex(svm.view_setting[v.ci]); }
    struct { const char* id; int idx; } acts[] = {
        {"act_vbc",0},{"act_pgs",1},{"act_dgs",2},{"act_od",3},{"act_mobs",4} };
    for (auto& a : acts) { UI::MenuItem* it = m_menuRoot->find(a.id); if (it != nullptr) it->setSelectedChildIndex(svm.activation[a.idx] ? 1 : 0); }
}

// ============================================================================
// SVM framebuffer
// ============================================================================

void AppSvm::bindSvmFb()   { m_svmFb.bind();   }
void AppSvm::unbindSvmFb() { m_svmFb.unbind(); }

void AppSvm::pushSvmFbTexture()
{
    UI::Texture* tex = m_svmFb.getTexture();
    if (m_svmImageView != nullptr && tex != nullptr && tex->isValid() == true)
        m_svmImageView->setTexture(tex, true);
}

// ============================================================================
// Calibration transition
// ============================================================================

void AppSvm::goToCalibration() { closeMenu(false); Write_Webserver(Goto_CalibUi); }

// ============================================================================
// Camera input (main screen interactive mode)
// ============================================================================

void AppSvm::setInteractiveMode(bool enable)
{
    if (m_interactiveMode == enable) return;
    m_interactiveMode = enable;
    m_pointerDown     = false;
    m_twoFingerActive = false;
    m_cameraInput     = CameraInput{};
}

void AppSvm::setLayout1Rect(int x, int y, int width, int height)
{
    m_layout1FbRect = UI::RectF::fromXYWH(static_cast<float>(x),     static_cast<float>(y),
                                           static_cast<float>(width), static_cast<float>(height));
}

bool AppSvm::isLayout1Hit(float screenX, float screenY) const
{
    if (m_svmImageView == nullptr) return false;
    const UI::RectF img = m_svmImageView->getImageRect();
    const float pw = img.width(), ph = img.height();
    if (pw <= 0.0f || ph <= 0.0f) return false;
    const float fbX = (screenX - img.left) / pw * static_cast<float>(WND_WIDTH);
    const float fbY = (screenY - img.top)  / ph * static_cast<float>(WND_HEIGHT);
    return m_layout1FbRect.contains(fbX, fbY);
}

CameraInput AppSvm::takeCameraInput()
{
    CameraInput r = m_cameraInput;
    m_cameraInput = CameraInput{};
    return r;
}

// ============================================================================
// CamView adjustment
// ============================================================================

void AppSvm::startAdjustCamViewR2D(int camIdx)
{
    APP::appConf.menu_svm.selected_camIdx = camIdx;
    startAdjustment(AdjustmentType::CamViewR2D, 2);
    setMenuHint(TR("Adj_CamView_Rcam2D_step0"));
    if (m_menuNavigator != nullptr)
    {
        m_menuNavigator->setSubMenuAdjustmentMode(true);
        m_menuNavigator->setSubMenuOnAdjustmentConfirm([this]()
        {
            confirmAdjustment();
            if (m_adjustmentType == AdjustmentType::AdjNone)
            {
                clearMenuHint();
                if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
            }
        });
    }
}

void AppSvm::startAdjustCamViewV3D(int camIdx)
{
    m_v3dAdjCamIdx                        = camIdx;
    APP::appConf.menu_svm.selected_camIdx = camIdx;
    m_adjPointerDown                      = false;
    m_adjTwoFingerActive                  = false;

    if (m_svmAnimator != nullptr)
    {
        m_svmAnimator->setActiveViewMode(CAMVIEW3D_FRONT + camIdx);
        m_svmAnimator->setInteractive(true);
    }

    startAdjustment(AdjustmentType::CamViewV3D, 1);
    setMenuHint(TR("Adj_CamView_Vcam3D"));

    if (m_menuNavigator != nullptr)
    {
        m_menuNavigator->setSubMenuAdjustmentMode(true);
        m_menuNavigator->setSubMenuOnAdjustmentConfirm([this]()
        {
            if (m_svmAnimator != nullptr)
            {
                glm::vec3 pos, ori;
                m_svmAnimator->getCurrentPose(pos, ori);
                auto& v = APP::appConf.menu_svm.vcam[m_v3dAdjCamIdx];
                v = { pos.x, pos.y, pos.z, ori.x, ori.y, ori.z };
                m_svmAnimator->setInteractive(false);
            }
            confirmAdjustment();
            clearMenuHint();
            if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
        });
    }
}

void AppSvm::applyAdjustCamViewR2D(bool left, bool right, bool up, bool down)
{
    const int camIdx = APP::appConf.menu_svm.selected_camIdx;
    if (camIdx < 0 || camIdx >= 4) return;

    constexpr int MOVE = 10, SZ = 4;
    auto& rcam = APP::appConf.menu_svm.rcam[camIdx];

    if (left)  { rcam[0] = std::max(0, rcam[0] - MOVE); rcam[1] = std::max(0, rcam[1] - MOVE); }
    if (right) { const int cap = (WND_WIDTH - SZ) / 2;
                 rcam[0] = std::min(cap, rcam[0] + MOVE); rcam[1] = std::min(cap, rcam[1] + MOVE); }
    if (m_adjustmentStepIndex == 0)
    {
        if (up)   rcam[2] = std::min(WND_HEIGHT - SZ - rcam[3], rcam[2] + MOVE);
        if (down) rcam[2] = std::max(0, rcam[2] - MOVE);
    }
    else
    {
        if (up)   rcam[3] = std::min(WND_HEIGHT - SZ - rcam[2], rcam[3] + MOVE);
        if (down) rcam[3] = std::max(0, rcam[3] - MOVE);
    }
}

void AppSvm::stepEntered(int step)
{
    if      (m_adjustmentType == AdjustmentType::CamViewR2D && step == 0) setMenuHint(TR("Adj_CamView_Rcam2D_step0"));
    else if (m_adjustmentType == AdjustmentType::CamViewR2D && step == 1) setMenuHint(TR("Adj_CamView_Rcam2D_step1"));
}

// ============================================================================
// Helpers
// ============================================================================

void AppSvm::updateSvmPreview()
{
    if (m_menuNavigator == nullptr) return;
    UI::FrameLayout* root = m_menuNavigator->getRootMenuView();
    if (root == nullptr || root->getChildCount() < 2) return;
    UI::SubMenuView* smv = dynamic_cast<UI::SubMenuView*>(root->getChildAt(1));
    if (smv == nullptr) return;

    UI::MenuItem* current = m_menuNavigator->getCurrentItem();
    if (current != nullptr && current->getId() == "view")
    {
        const int   subIdx = m_menuNavigator->getSubMenuSelectedIndex();
        M_MENU_SVM& menu   = APP::appConf.menu_svm;
        menu.selected_viewIdx = subIdx;
        menu.selected_camIdx  = (subIdx == 4) ? ((menu.view_setting[4] == 0) ? 0 : 2) : subIdx;
    }

    UI::Texture*   fbTex = m_svmFb.getTexture();
    UI::ImageView* prev  = smv->getPreviewImage();
    if (fbTex == nullptr || prev == nullptr) return;
    prev->setScaleType(UI::ScaleType::FIT_CENTER);
    prev->setTexture(fbTex, true);
}

void AppSvm::loadSvmConfigValues()
{
    APP::appConf.menu_svm.selected_camIdx  = 0;
    APP::appConf.menu_svm.selected_viewIdx = 0;
    APP::appConf.menu_svm.visible          = false;
}

void  AppSvm::accumulateOrbit(float dAzPx, float dElPx) { m_cameraInput.orbitAzimuth += dAzPx; m_cameraInput.orbitElevation += dElPx; }
void  AppSvm::accumulateZoom (float spanPx)             { m_cameraInput.zoomDelta += spanPx; }

float AppSvm::touchSpan(const IO::MotionEvent& e) const
{
    const float dx = e.pointers[1].x - e.pointers[0].x;
    const float dy = e.pointers[1].y - e.pointers[0].y;
    return std::sqrt(dx*dx + dy*dy);
}

bool AppSvm::handleOrbitZoomGesture(const IO::MotionEvent& event,
                                    const HitTestFn& hitTest,
                                    bool&     pointerDown,
                                    bool&     twoFingerActive,
                                    float&    prevSpan,
                                    UI::Vec2& lastPointer)
{
    if (event.action == IO::MotionAction::DOWN ||
        event.action == IO::MotionAction::POINTER_DOWN)
    {
        const float hx = (event.pointerCount >= 2) ? (event.pointers[0].x + event.pointers[1].x) * 0.5f : event.x;
        const float hy = (event.pointerCount >= 2) ? (event.pointers[0].y + event.pointers[1].y) * 0.5f : event.y;
        if (hitTest(hx, hy) == false) return false;
    }

    // Mouse scroll → zoom
    if (event.isMouse() == true && event.action == IO::MotionAction::SCROLL)
        { accumulateZoom(-event.scrollY * kMouseScrollZoomScale); return true; }

    // Mouse drag → orbit
    if (event.isMouse() == true)
    {
        if (event.action == IO::MotionAction::DOWN)
            { lastPointer = UI::Vec2(event.x, event.y); pointerDown = true; return true; }
        if (event.action == IO::MotionAction::MOVE && pointerDown == true)
        {
            const UI::Vec2 pos(event.x, event.y);
            accumulateOrbit(pos.x - lastPointer.x, pos.y - lastPointer.y);
            lastPointer = pos;
            return true;
        }
        if (event.action == IO::MotionAction::UP || event.action == IO::MotionAction::CANCEL)
            { const bool was = pointerDown; pointerDown = false; return was; }
        return false;
    }

    // Touch: two-finger pinch → zoom
    if (event.pointerCount >= 2)
    {
        pointerDown = false;
        const float span = touchSpan(event);
        if (event.action == IO::MotionAction::POINTER_DOWN || twoFingerActive == false)
            { prevSpan = span; twoFingerActive = true; return true; }
        if (event.action == IO::MotionAction::MOVE)
            { accumulateZoom(span - prevSpan); prevSpan = span; }
        if (event.action == IO::MotionAction::POINTER_UP ||
            event.action == IO::MotionAction::UP         ||
            event.action == IO::MotionAction::CANCEL)
            twoFingerActive = false;
        return twoFingerActive;
    }

    // Touch: single-finger → orbit
    twoFingerActive = false;
    const UI::Vec2 pos(event.x, event.y);
    if (event.action == IO::MotionAction::DOWN)
        { lastPointer = pos; pointerDown = true; return true; }
    if (event.action == IO::MotionAction::MOVE && pointerDown == true)
        { accumulateOrbit(pos.x - lastPointer.x, pos.y - lastPointer.y); lastPointer = pos; return true; }
    if (event.action == IO::MotionAction::UP || event.action == IO::MotionAction::CANCEL)
        { const bool was = pointerDown; pointerDown = false; return was; }
    return false;
}

bool AppSvm::handleAdjV3DMotion(const IO::MotionEvent& event)
{
    if (m_menuNavigator == nullptr) return false;
    UI::SubMenuView* smv = m_menuNavigator->getSubMenuView();
    if (smv == nullptr || smv->getPreviewContainer() == nullptr) return false;

    auto isPreviewHit = [&](float sx, float sy) -> bool
    {
        UI::ImageView* prev = smv->getPreviewImage();
        if (prev == nullptr) return false;
        const UI::RectF img = prev->getImageRect();
        const float pw = img.width(), ph = img.height();
        if (pw <= 0.0f || ph <= 0.0f) return false;
        const float fbX = (sx - img.left) / pw * static_cast<float>(WND_WIDTH);
        const float fbY = (sy - img.top)  / ph * static_cast<float>(WND_HEIGHT);
        return m_layout1FbRect.contains(fbX, fbY);
    };

    return handleOrbitZoomGesture(event, isPreviewHit,
                                  m_adjPointerDown, m_adjTwoFingerActive,
                                  m_adjPrevSpan,    m_adjLastPointer);
}

} // namespace APP
