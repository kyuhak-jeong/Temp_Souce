#include "app_calib.h"
#include "app_vars.h"
#include "logger.h"
#include "ui_locale.h"
#include "ui_elements.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <thread>

static constexpr const char* CALIB_STEP_IDS[CALIB_STEP_COUNT] = {
    "ac_fisheye", "ac_defisheye", "ac_feature",
    "ac_contour", "ac_grid",      "ac_mask",    "ac_result"
};

extern void write_string_pipeline_config(std::string fileName);

namespace APP
{

// ============================================================================
// Local helpers
// ============================================================================

static UI::SubMenuView* getSubMenuView(UI::MenuNavigator* nav)
{
    if (nav == nullptr) return nullptr;
    UI::FrameLayout* root = nav->getRootMenuView();
    if (root == nullptr || root->getChildCount() < 2) return nullptr;
    return dynamic_cast<UI::SubMenuView*>(root->getChildAt(1));
}

static bool isSubMenuWithId(UI::MenuNavigator* nav, const char* id)
{
    if (nav == nullptr) return false;
    UI::MenuItem* cur = nav->getCurrentItem();
    return (cur != nullptr && cur->getId() == id && nav->getState() == UI::MenuState::SUB_MENU);
}

static bool isVehicleSubMenu(UI::MenuNavigator* nav)   { return isSubMenuWithId(nav, "select_vehicle"); }
static bool isCaptureSubMenu(UI::MenuNavigator* nav)   { return isSubMenuWithId(nav, "capture");        }
static bool isAutoCalibSubMenu(UI::MenuNavigator* nav) { return isSubMenuWithId(nav, "auto_calib");     }

// ============================================================================
// Constructor / Destructor
// ============================================================================

AppCalib::AppCalib()
    : AppShell()
    , m_currentVehicleIdx(0)
    , m_hoveredVehicleIdx(0)
    , m_previewDebounce(0)
    , m_vehicleOverlay(nullptr)
    , m_vehicleViewport(nullptr)
    , m_vehicleModel(nullptr)
    , m_captureLeftPanelTexId(0)
    , m_streamLabelsOverlay(nullptr)
    , m_streamCellLabels{}
    , m_calibNeedRefreshContext(false)
    , m_pendingPointPos(-1.0f, -1.0f)
    , m_pendingAdjCam(-1)
    , m_pointConfirmed(false)
    , m_featureDragActive(false)
    , m_streamPipelineUpdated(false)
    , m_streamPipelineRunning(false)
{}

AppCalib::~AppCalib() {}

// ============================================================================
// AppBase overrides — lifecycle
// ============================================================================

bool AppCalib::onInitialize()
{
    if (AppShell::onInitialize() == false) return false;

    loadCalibConfigValues();

    if (m_calibFb.create(WND_WIDTH, WND_HEIGHT) == false)
    {
        LOG_APP_ERROR("AppCalib: Failed to create calib framebuffer");
        return false;
    }
    LOG_APP_SUCCESSF("AppCalib: Calib framebuffer created [%d, %d]", WND_WIDTH, WND_HEIGHT);

    showMenuScreen();

    UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
    if (smv != nullptr)
    {
        UI::FrameLayout* previewContainer = smv->getPreviewContainer();
        if (previewContainer != nullptr)
        {
            // Vehicle info overlay
            m_vehicleOverlay = new UI::LinearLayout();
            m_vehicleOverlay->setOrientation(UI::Orientation::VERTICAL);
            m_vehicleOverlay->setGravity(UI::Gravity::LEFT | UI::Gravity::TOP);
            m_vehicleOverlay->setSpacing(6.0f);
            m_vehicleOverlay->setBackgroundColor(UI::Color::OverlayLight);
            m_vehicleOverlay->setCornerRadius(24.0f);
            m_vehicleOverlay->getLayoutParams().width   = UI::WRAP_CONTENT;
            m_vehicleOverlay->getLayoutParams().height  = UI::WRAP_CONTENT;
            m_vehicleOverlay->getLayoutParams().gravity = UI::Gravity::LEFT | UI::Gravity::TOP;
            m_vehicleOverlay->getLayoutParams().setMargin(10.0f);
            m_vehicleOverlay->getLayoutParams().setPadding(20.0f, 14.0f);
            m_vehicleOverlay->setVisibility(UI::Visibility::GONE);

            auto makeTV = []() -> UI::TextView*
            {
                auto* tv = new UI::TextView();
                tv->setTextColor(UI::Color::TextPrimary);
                tv->setTextSize(36.0f);
                tv->getLayoutParams().width  = UI::WRAP_CONTENT;
                tv->getLayoutParams().height = UI::WRAP_CONTENT;
                return tv;
            };
            m_vehicleOverlay->addView(makeTV());  // child 0 – Manufacturer
            m_vehicleOverlay->addView(makeTV());  // child 1 – Model
            m_vehicleOverlay->addView(makeTV());  // child 2 – Year

            // 3D viewport
            m_vehicleViewport = new UI3D::Viewport();
            m_vehicleViewport->getLayoutParams().width   = UI::MATCH_PARENT;
            m_vehicleViewport->getLayoutParams().height  = UI::MATCH_PARENT;
            m_vehicleViewport->getLayoutParams().gravity = UI::Gravity::CENTER;
            m_vehicleViewport->setBackgroundColor(UI::Color::Transparent);
            m_vehicleViewport->setShowGrid(true);
            m_vehicleViewport->setShowAxis(true);
            m_vehicleViewport->setInteractionEnabled(true);
            m_vehicleViewport->addLight(UI3D::Light::makeDirectional(
                UI3D::Vec3(-0.5f, -1.0f, -0.8f).normalized(), UI::Color::White, 1.2f));
            m_vehicleViewport->addLight(UI3D::Light::makeDirectional(
                UI3D::Vec3(1.0f, -0.3f, 0.5f).normalized(), UI::Color::White, 0.4f));

            UI3D::Camera cam;
            cam.setPerspective(0.7854f, 1.0f, 0.01f, 100.0f);
            cam.setUp(UI3D::Vec3(0.0f, 1.0f, 0.0f));
            m_vehicleViewport->setCamera(cam);
            m_vehicleViewport->setVisibility(UI::Visibility::GONE);

            previewContainer->addView(m_vehicleViewport);
            previewContainer->addView(m_vehicleOverlay);

            // Stream grid labels overlay — 2×2 layout matching glvideomixer quadrants:
            // top-left=Left, top-right=Right, bottom-left=Front, bottom-right=Rear
            static const char* LABEL_KEYS[2][2] = { { "Left", "Right" }, { "Front", "Rear" } };

            m_streamLabelsOverlay = new UI::LinearLayout();
            m_streamLabelsOverlay->setOrientation(UI::Orientation::VERTICAL);
            m_streamLabelsOverlay->getLayoutParams().width   = UI::MATCH_PARENT;
            m_streamLabelsOverlay->getLayoutParams().height  = UI::MATCH_PARENT;
            m_streamLabelsOverlay->getLayoutParams().gravity = UI::Gravity::NO_GRAVITY;
            m_streamLabelsOverlay->setBackgroundColor(UI::Color::Transparent);
            m_streamLabelsOverlay->setVisibility(UI::Visibility::GONE);

            for (int row = 0; row < 2; row++)
            {
                auto* rowLayout = new UI::LinearLayout();
                rowLayout->setOrientation(UI::Orientation::HORIZONTAL);
                rowLayout->getLayoutParams().width  = UI::MATCH_PARENT;
                rowLayout->getLayoutParams().height = UI::WRAP_CONTENT;
                rowLayout->getLayoutParams().weight = 1.0f;

                for (int col = 0; col < 2; col++)
                {
                    auto* cell = new UI::FrameLayout();
                    cell->getLayoutParams().width  = UI::MATCH_PARENT;
                    cell->getLayoutParams().height = UI::MATCH_PARENT;
                    cell->getLayoutParams().weight = 1.0f;
                    cell->setBackgroundColor(UI::Color::Transparent);

                    auto* label = new UI::TextView();
                    label->setText(TR(LABEL_KEYS[row][col]));
                    label->setTextSize(28.0f);
                    label->setTextColor(UI::Color::TextPrimary);
                    label->setBackgroundColor(UI::Color::OverlayLight);
                    label->setCornerRadius(16.0f);
                    label->getLayoutParams().width   = UI::WRAP_CONTENT;
                    label->getLayoutParams().height  = UI::WRAP_CONTENT;
                    label->getLayoutParams().gravity = UI::Gravity::LEFT | UI::Gravity::TOP;
                    label->getLayoutParams().setMargin(8.0f);
                    label->getLayoutParams().setPadding(14.0f, 6.0f);

                    m_streamCellLabels[row * 2 + col] = label;
                    cell->addView(label);
                    rowLayout->addView(cell);
                }
                m_streamLabelsOverlay->addView(rowLayout);
            }
            previewContainer->addView(m_streamLabelsOverlay);
        }
    }

    return true;
}

void AppCalib::onCleanup()
{
    m_vehicleInstance.unload();
    if (m_vehicleModel != nullptr) { m_vehicleModel->release(); delete m_vehicleModel; m_vehicleModel = nullptr; }
    m_calibFb.release();
    AppShell::onCleanup();
}

void AppCalib::onUpdate(float deltaTime)
{
    AppShell::onUpdate(deltaTime);
    updateCalibPreview();

    if (m_previewDebounce > 0.0f)
    {
        m_previewDebounce -= deltaTime;
        if (m_previewDebounce <= 0.0f) syncVehiclePreview(m_hoveredVehicleIdx);
    }

    if (m_vehicleViewport == nullptr
     || m_vehicleViewport->getVisibility() != UI::Visibility::VISIBLE
     || m_vehicleModel == nullptr) return;

    m_vehicleInstance.update(deltaTime);
    m_vehicleViewport->update(deltaTime);
}

void AppCalib::onRender() { AppShell::onRender(); }

bool AppCalib::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE)
        return AppShell::onKeyEvent(event);

    // Feature-point adjustment mode
    if (m_adjustmentType == AdjustmentType::CalibFeaturePoint)
    {
        if (event.keyCode == IO::KeyCode::ESCAPE || event.keyCode == IO::KeyCode::BACK)
        {
            m_featureDragActive = false;
            cancelAdjustment();
            return true;
        }

        constexpr float kStep = 1.0f;
        switch (event.keyCode)
        {
            case IO::KeyCode::DPAD_LEFT:   if (m_pendingPointPos.x >= 0.f) m_pendingPointPos.x -= kStep; return true;
            case IO::KeyCode::DPAD_RIGHT:  if (m_pendingPointPos.x >= 0.f) m_pendingPointPos.x += kStep; return true;
            case IO::KeyCode::DPAD_UP:     if (m_pendingPointPos.x >= 0.f) m_pendingPointPos.y -= kStep; return true;
            case IO::KeyCode::DPAD_DOWN:   if (m_pendingPointPos.x >= 0.f) m_pendingPointPos.y += kStep; return true;
            case IO::KeyCode::DPAD_CENTER:
            case IO::KeyCode::ENTER:
            case IO::KeyCode::SPACE:       m_pointConfirmed = true; return true;
            default: return true;
        }
    }

    // Calib params adjustment mode
    if (m_adjustmentType == AdjustmentType::CalibParams)
    {
        if (event.keyCode == IO::KeyCode::ESCAPE || event.keyCode == IO::KeyCode::BACK)
            cancelAdjustment();
        return true;
    }

    if (isAutoCalibSubMenu(m_menuNavigator.get()) == true)
    {
        UI::SubMenuView* smv  = m_menuNavigator->getSubMenuView();
        UI::MenuItem*    root = (smv != nullptr) ? smv->getMenuItem() : nullptr;
        UI::MenuItem*    sel  = nullptr;
        if (root != nullptr)
        {
            const int selRow = smv->getSelectedSubItemIndex();
            int rowIdx = 0;
            for (const auto& child : root->getChildren())
            {
                if (child == nullptr || child->isVisible() == false) continue;
                if (rowIdx == selRow) { sel = child.get(); break; }
                rowIdx++;
            }
        }

        if (event.keyCode == IO::KeyCode::DPAD_UP || event.keyCode == IO::KeyCode::DPAD_DOWN)
            return AppShell::onKeyEvent(event);

        if (sel != nullptr && sel->getId() == "ac_feature")
        {
            const int camCount = static_cast<int>(sel->getChildren().size());
            const int curCam   = sel->getSelectedChildIndex();
            if      (event.keyCode == IO::KeyCode::DPAD_RIGHT && curCam >= camCount - 1) return true;
            else if (event.keyCode == IO::KeyCode::DPAD_LEFT  && curCam <= 0)            return true;
            else if (event.keyCode == IO::KeyCode::DPAD_DOWN  && curCam < camCount - 1)  return true;
        }
    }

    if (isVehicleSubMenu(m_menuNavigator.get()) == true
     && (event.keyCode == IO::KeyCode::DPAD_UP || event.keyCode == IO::KeyCode::DPAD_DOWN))
    {
        bool handled = AppShell::onKeyEvent(event);
        const int total = static_cast<int>(m_vehiclesMap.size());
        const int idx   = std::max(0, std::min(m_menuNavigator->getSubMenuSelectedIndex(), total - 1));
        if (idx != m_hoveredVehicleIdx)
        {
            m_hoveredVehicleIdx = idx;
            m_previewDebounce   = 0.30f;
        }
        return handled;
    }

    if (isCaptureSubMenu(m_menuNavigator.get()) == true)
    {
        if (event.keyCode == IO::KeyCode::ESCAPE || event.keyCode == IO::KeyCode::BACK)
        {
            stopStreamPipeline();
            return AppShell::onKeyEvent(event);
        }
        const bool isUpDown = (event.keyCode == IO::KeyCode::DPAD_UP ||
                               event.keyCode == IO::KeyCode::DPAD_DOWN);
        bool handled = AppShell::onKeyEvent(event);
        if (isUpDown == true) syncStreamPipeline(m_topBarFocus == TopBarFocus::BACK);
        return handled;
    }

    return AppShell::onKeyEvent(event);
}

bool AppCalib::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_adjustmentType == AdjustmentType::CalibFeaturePoint)
    {
        UI::SubMenuView* smv = (m_menuNavigator != nullptr) ? m_menuNavigator->getSubMenuView() : nullptr;
        UI::ImageView* previewImg = (smv != nullptr) ? smv->getPreviewImage() : nullptr;

        if (previewImg != nullptr)
        {
            const UI::RectF previewRect = previewImg->getImageRect();
            const float pw = previewRect.width();
            const float ph = previewRect.height();

            if (event.action == IO::MotionAction::DOWN && pw > 0.0f && ph > 0.0f &&
                previewRect.contains(event.x, event.y))
            {
                // Map touch to native calib framebuffer space and hit-test against selected point
                const float nativeX = (event.x - previewRect.left) / pw * static_cast<float>(WND_WIDTH);
                const float nativeY = (event.y - previewRect.top)  / ph * static_cast<float>(WND_HEIGHT);

                // true  = any touch in preview grabs the current point immediately
                // false = must touch within HIT_RADIUS of the current point to start drag
                constexpr bool  FEATURE_DRAG_ALWAYS_GRAB = true;
                constexpr float HIT_RADIUS               = 40.0f;

                const XY&  selPt = m_lastSyncedState.selectedPtPos;
                const float dx   = nativeX - selPt.x;
                const float dy   = nativeY - selPt.y;
                if (FEATURE_DRAG_ALWAYS_GRAB == true || dx * dx + dy * dy < HIT_RADIUS * HIT_RADIUS)
                {
                    m_featureDragActive = true;
                    m_pendingPointPos   = UI::Vec2{nativeX, nativeY};
                    return true;
                }
            }
            else if (event.action == IO::MotionAction::MOVE && m_featureDragActive == true &&
                     pw > 0.0f && ph > 0.0f)
            {
                const float nativeX = std::max(0.0f, std::min((event.x - previewRect.left) / pw * static_cast<float>(WND_WIDTH),  static_cast<float>(WND_WIDTH)));
                const float nativeY = std::max(0.0f, std::min((event.y - previewRect.top)  / ph * static_cast<float>(WND_HEIGHT), static_cast<float>(WND_HEIGHT)));
                m_pendingPointPos = UI::Vec2{nativeX, nativeY};
                return true;
            }
            else if (event.action == IO::MotionAction::UP && m_featureDragActive == true &&
                     pw > 0.0f && ph > 0.0f)
            {
                const float nativeX = std::max(0.0f, std::min((event.x - previewRect.left) / pw * static_cast<float>(WND_WIDTH),  static_cast<float>(WND_WIDTH)));
                const float nativeY = std::max(0.0f, std::min((event.y - previewRect.top)  / ph * static_cast<float>(WND_HEIGHT), static_cast<float>(WND_HEIGHT)));
                m_pendingPointPos   = UI::Vec2{nativeX, nativeY};
                m_featureDragActive = false;
                return true;
            }
        }

        // Check if the touch is on the currently-adjusting camera pill — confirm on UP, block otherwise
        if (smv != nullptr && smv->getBounds().contains(event.x, event.y))
        {
            const int adjCam = m_lastSyncedState.camIndex;
            UI::MenuItem* featItem = (m_menuRoot != nullptr) ? m_menuRoot->find("ac_feature") : nullptr;
            if (featItem != nullptr)
            {
                UI::LinearLayout* valueWidget = featItem->viewData().valueWidget;
                if (valueWidget != nullptr && adjCam >= 0 && adjCam < static_cast<int>(valueWidget->getChildCount()))
                {
                    UI::View* camPill = valueWidget->getChildAt(static_cast<size_t>(adjCam));
                    if (camPill != nullptr && camPill->getBounds().contains(event.x, event.y))
                    {
                        if (event.action == IO::MotionAction::UP) m_pointConfirmed = true;
                        return true;
                    }
                }
            }
            return true;
        }
    }

    if (isCaptureSubMenu(m_menuNavigator.get()) == true
     && m_popupVisible == false && m_menuBackButton != nullptr)
    {
        const bool insideBack  = m_menuBackButton->getBounds().contains(event.x, event.y);
        const bool backFocused = (m_topBarFocus == TopBarFocus::BACK);
        if (insideBack != backFocused) syncStreamPipeline(insideBack);
    }
    return AppShell::onMotionEvent(event);
}

// ============================================================================
// AppShell overrides
// ============================================================================

void AppCalib::stepEntered(int step)
{
    setMenuHint(TR("Adj_FeatureView") + " — " + TR("Point") + " " + std::to_string(step + 1));
}

void AppCalib::onConfigReverted() { loadCalibConfigValues(); }
void AppCalib::onConfigSaved()    { saveCalibParamsData(); }

int AppCalib::getSelectedCalibViewMode() const
{
    if (isAutoCalibSubMenu(m_menuNavigator.get()) == false) return -1;
    UI::SubMenuView* smv  = m_menuNavigator->getSubMenuView();
    UI::MenuItem*    root = (smv != nullptr) ? smv->getMenuItem() : nullptr;
    if (root == nullptr) return -1;

    const int selRow = smv->getSelectedSubItemIndex();
    int rowIdx = 0;
    for (const auto& child : root->getChildren())
    {
        if (child == nullptr || child->isVisible() == false) continue;
        if (rowIdx == selRow)
        {
            for (int i = 0; i < CALIB_STEP_COUNT; i++)
                if (child->getId() == CALIB_STEP_IDS[i]) return i; // i == VIEW_MODE value
            return -1;
        }
        rowIdx++;
    }
    return -1;
}

int AppCalib::getSelectedCalibCamIdx() const
{
    if (isAutoCalibSubMenu(m_menuNavigator.get()) == false) return 0;
    UI::SubMenuView* smv  = m_menuNavigator->getSubMenuView();
    UI::MenuItem*    root = (smv != nullptr) ? smv->getMenuItem() : nullptr;
    if (root == nullptr) return 0;

    const int selRow = smv->getSelectedSubItemIndex();
    int rowIdx = 0;
    for (const auto& child : root->getChildren())
    {
        if (child == nullptr || child->isVisible() == false) continue;
        if (rowIdx == selRow)
            return (child->getId() == "ac_feature") ? child->getSelectedChildIndex() : 0;
        rowIdx++;
    }
    return 0;
}

// ── UI sync ───────────────────────────────────────────────────────────────────

void AppCalib::startAdjustmentForCam(int cam)
{
    if (m_adjustmentType == AdjustmentType::CalibFeaturePoint) return;
    startAdjustment(AdjustmentType::CalibFeaturePoint, CONTROL_POINTS_NUM);
    stepEntered(0);
}

void AppCalib::syncCalibState(const CalibState& state)
{
    if (state.stepDenied) setMenuHint(TR("Complete_Previous_Steps"));

    UI::SubMenuView* smv = m_menuNavigator ? m_menuNavigator->getSubMenuView() : nullptr;

    if (state.enterAdjustMode)
    {
        if (m_pendingPointPos.x < 0.f)
            m_pendingPointPos = UI::Vec2{state.selectedPtPos.x, state.selectedPtPos.y};

        if (state.selectedPtIdx != m_lastSyncedState.selectedPtIdx ||
            state.camIndex      != m_lastSyncedState.camIndex      ||
            !m_lastSyncedState.enterAdjustMode)
        {
            m_pendingPointPos = UI::Vec2{state.selectedPtPos.x, state.selectedPtPos.y};
            stepEntered(state.selectedPtIdx);
        }

        if (!m_lastSyncedState.enterAdjustMode && smv != nullptr)
            smv->setAdjusting(true);
    }
    else if (m_lastSyncedState.enterAdjustMode)
    {
        m_featureDragActive = false;
        if (smv != nullptr) smv->setAdjusting(false);
        cancelAdjustment();
    }

    const bool stepsChanged = (state.stepStates         != m_lastSyncedState.stepStates ||
                               state.featureCamsComplete != m_lastSyncedState.featureCamsComplete ||
                               state.camStepStates       != m_lastSyncedState.camStepStates);
    const bool adjChanged   = (state.enterAdjustMode != m_lastSyncedState.enterAdjustMode ||
                               state.camIndex        != m_lastSyncedState.camIndex);
    if (stepsChanged || adjChanged) refreshStepVisuals(state);

    m_lastSyncedState = state;
}

void AppCalib::refreshStepVisuals(const CalibState& state)
{
    if (m_menuRoot == nullptr || m_menuNavigator == nullptr) return;

    for (int i = 0; i < CALIB_STEP_COUNT; i++)
    {
        UI::MenuItem* item = m_menuRoot->find(CALIB_STEP_IDS[i]);
        if (item == nullptr) continue;

        const bool locked = (state.stepStates[i] == CalibStepStatus::LOCKED);
        item->setEnabled(!locked);
        item->viewData().tintColor = (state.stepStates[i] == CalibStepStatus::COMPLETE)
            ? UI::Color::AccentSuccess.withOpacity(0.35f)
            : UI::Color::Transparent;

        for (int c = 0; c < (int)item->getChildren().size(); c++)
        {
            UI::MenuItem* child = item->getChildren()[c].get();
            if (child != nullptr) child->setEnabled(!locked);
        }
    }

    const bool featureLocked = (state.stepStates[2] == CalibStepStatus::LOCKED); // index 2 = ac_feature
    for (int cam = 0; cam < SVM_CAMERAS_NUM; cam++)
    {
        UI::MenuItem* camItem = m_menuRoot->find("ac_feature_cam" + std::to_string(cam));
        if (camItem == nullptr) continue;

        const bool camLocked = featureLocked || (state.camStepStates[cam] == CalibStepStatus::LOCKED);
        camItem->setEnabled(!camLocked);

        const bool done = ((state.featureCamsComplete >> cam) & 1) == 1;
        const bool adj  = (m_adjustmentType == AdjustmentType::CalibFeaturePoint &&
                           state.camIndex == cam);
        camItem->viewData().tintColor = (done || adj)
            ? UI::Color::AccentSuccess.withOpacity(adj ? 0.85f : 0.35f)
            : UI::Color::Transparent;
    }

    UI::SubMenuView* smv = m_menuNavigator->getSubMenuView();
    if (smv != nullptr) smv->applyAllRowStyles();
}

// ── Menu builders ─────────────────────────────────────────────────────────────

void AppCalib::buildExtraMenuItems()
{
    auto addIcon = [&](std::shared_ptr<UI::MenuItem> item, const char* name)
    {
        UI::Texture* t = UI::TextureManager::getInstance().getTextureByName(name);
        if (t != nullptr) item->setIconTexture(t);
    };

    // ── Select Vehicle ────────────────────────────────────────────────────
    {
        auto vehicleMenu = std::make_shared<UI::MenuItem>(
            "select_vehicle", TR("Select Vehicle"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(vehicleMenu, "Select Vehicle");
        addIcon(vehicleMenu, "04_icn_vehicle_select");
        vehicleMenu->setShowPreview(true);
        vehicleMenu->setSelectedChildIndex(m_currentVehicleIdx);
        vehicleMenu->setOnActivate([this](UI::MenuItem*)
        {
            if (m_menuNavigator != nullptr)
            {
                m_menuNavigator->setSubMenuLayoutWeights(
                    UI::SubMenuView::DEFAULT_PREVIEW_WEIGHT,
                    UI::SubMenuView::DEFAULT_SUBITEMS_WEIGHT);
                m_menuNavigator->setSubMenuGridMode(false);
                UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
                if (smv != nullptr) smv->setOnSubItemSelected(nullptr);
            }
            stopStreamPipeline();
            m_hoveredVehicleIdx   = m_currentVehicleIdx;
            syncVehiclePreview(m_hoveredVehicleIdx);
        });
        m_menuRoot->addChild(vehicleMenu);
    }

    // ── Calib Params ──────────────────────────────────────────────────────
    {
        auto paramsMenu = std::make_shared<UI::MenuItem>(
            "calib_params", TR("Calib Params"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(paramsMenu, "Calib Params");
        addIcon(paramsMenu, "04_icn_calib_params");
        paramsMenu->setShowPreview(true);
        paramsMenu->setOnActivate([this](UI::MenuItem*)
        {
            if (m_menuNavigator == nullptr) return;
            if (m_vehicleOverlay        != nullptr) m_vehicleOverlay->setVisibility(UI::Visibility::GONE);
            if (m_vehicleViewport       != nullptr) m_vehicleViewport->setVisibility(UI::Visibility::GONE);
            if (m_streamLabelsOverlay   != nullptr) m_streamLabelsOverlay->setVisibility(UI::Visibility::GONE);

            m_menuNavigator->setSubMenuLayoutWeights(1.1f, 2.9f);
            m_menuNavigator->setSubMenuGridMode(true, 3);

            UI::SubMenuView::GridColumnDef col0, col1, col2;
            col0.weight = 1.0f; for (int r = 0; r < 10; r++) col0.addRow();
            col1.weight = 1.0f; for (int r = 0; r < 10; r++) col1.addRow();
            col2.weight = 1.0f; for (int r = 0; r < 10; r++) col2.addRow();
            m_menuNavigator->setSubMenuGridColumns({ col0, col1, col2 });

            m_menuNavigator->setSubMenuNumpadCallback(
                [this](UI::MenuItem* item, std::function<void(float)> onConfirm)
                {
                    if (item == nullptr || m_numpadPopup == nullptr) return;
                    const float lo  = item->getMinValue();
                    const float hi  = item->getMaxValue();
                    const std::string initialVal = std::to_string(static_cast<int>(item->getValueFloat()));
                    m_numpadPopup->setAllowDecimal(false);
                    m_numpadPopup->setAllowNegative(lo < 0.0f);
                    m_numpadPopup->setMinValue(lo);
                    m_numpadPopup->setMaxValue(hi);
                    m_numpadPopup->setMaxLength(6);

                    auto clearAdjusting = [this]()
                    {
                        if (m_menuNavigator != nullptr)
                        {
                            auto* smv = m_menuNavigator->getSubMenuView();
                            if (smv != nullptr) smv->setAdjusting(false);
                        }
                    };
                    m_numpadPopup->setOnConfirm([this, item, onConfirm, lo, hi, clearAdjusting](const std::string& s)
                    {
                        float v = 0.0f;
                        try { v = std::stof(s); } catch (...) { v = item->getValueFloat(); }
                        v = std::max(lo, std::min(v, hi));
                        item->setValueFloat(v);
                        onConfirm(v);
                        clearAdjusting();
                    });
                    m_numpadPopup->setOnCancel([clearAdjusting]() { clearAdjusting(); });
                    m_numpadPopup->setCancelText(TR("Cancel"));
                    m_numpadPopup->setConfirmText(TR("Confirm"));
                    m_numpadPopup->show(item->getName(), initialVal, false);
                });

            auto updatePreviewTex = [this]()
            {
                if (m_menuNavigator == nullptr) return;
                auto* smv = m_menuNavigator->getSubMenuView();
                if (smv == nullptr) return;
                UI::ImageView* img = smv->getPreviewImage();
                if (img == nullptr) return;

                const int flatIdx = smv->getSelectedCol() * 10 + smv->getSelectedSubItemIndex();
                const char* texName = nullptr;
                if      (flatIdx < 10) texName = "04_bg_vehicle_specs";
                else if (flatIdx < 14) texName = "04_bg_pattern_offset";
                else if (flatIdx < 20) texName = "04_bg_pattern_roi_auto_detect";
                else
                {
                    auto* patItem = m_menuRoot ? m_menuRoot->find("pat_num") : nullptr;
                    const int nPat = patItem ? static_cast<int>(patItem->getValueFloat()) : 4;
                    texName = (nPat >= 5) ? "04_bg_pattern_8" : "04_bg_pattern_4";
                }

                UI::Texture* t = UI::TextureManager::getInstance().getTextureByName(texName);
                if (t != nullptr) { img->setScaleType(UI::ScaleType::FIT_CENTER); img->setTexture(t, false); }
            };

            auto* smv = m_menuNavigator->getSubMenuView();
            if (smv != nullptr)
            {
                smv->setOnSubItemSelected([updatePreviewTex](int) { updatePreviewTex(); });
                updatePreviewTex();
            }
        });
        buildCalibParamsMenuItems(paramsMenu);
        m_menuRoot->addChild(paramsMenu);
    }

    // ── Capture ───────────────────────────────────────────────────────────
    {
        auto captureMenu = std::make_shared<UI::MenuItem>(
            "capture", TR("Capture"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(captureMenu, "Capture");
        addIcon(captureMenu, "04_icn_capture");
        captureMenu->setShowPreview(true);
        captureMenu->setOnActivate([this](UI::MenuItem*)
        {
            if (m_menuNavigator != nullptr)
            {
                m_menuNavigator->setSubMenuLayoutWeights(
                    UI::SubMenuView::DEFAULT_PREVIEW_WEIGHT,
                    UI::SubMenuView::DEFAULT_SUBITEMS_WEIGHT);
                m_menuNavigator->setSubMenuGridMode(false);
            }
            if (m_vehicleOverlay        != nullptr) m_vehicleOverlay->setVisibility(UI::Visibility::GONE);
            if (m_vehicleViewport       != nullptr) m_vehicleViewport->setVisibility(UI::Visibility::GONE);
            if (m_streamLabelsOverlay   != nullptr) m_streamLabelsOverlay->setVisibility(UI::Visibility::GONE);

            UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
            if (smv != nullptr)
            {
                UI::ImageView* img = smv->getPreviewImage();
                if (img != nullptr) img->setTexture(nullptr, false);
                smv->setOnSubItemSelected([this](int idx) { startStreamPipeline(idx == 1); });
            }
            if (m_streamLabelsOverlay != nullptr) m_streamLabelsOverlay->setVisibility(UI::Visibility::VISIBLE);

            // Refresh cell labels in case locale changed
            static const char* CELL_KEYS[4] = { "Left", "Right", "Front", "Rear" };
            for (int i = 0; i < 4; i++)
                if (m_streamCellLabels[i] != nullptr) m_streamCellLabels[i]->setText(TR(CELL_KEYS[i]));

            startStreamPipeline(false);
        });
        buildCaptureMenuItems(captureMenu);
        m_menuRoot->addChild(captureMenu);
    }

    // ── AutoCalib ─────────────────────────────────────────────────────────
    {
        auto autoCalibMenu = std::make_shared<UI::MenuItem>(
            "auto_calib", TR("AutoCalib"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(autoCalibMenu, "AutoCalib");
        addIcon(autoCalibMenu, "04_icn_auto_calib");
        autoCalibMenu->setShowPreview(true);
        autoCalibMenu->setOnActivate([this](UI::MenuItem*)
        {
            if (m_menuNavigator != nullptr)
            {
                m_menuNavigator->setSubMenuLayoutWeights(
                    UI::SubMenuView::DEFAULT_PREVIEW_WEIGHT,
                    UI::SubMenuView::DEFAULT_SUBITEMS_WEIGHT);
                m_menuNavigator->setSubMenuGridMode(false);
                UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
                if (smv != nullptr) smv->setOnSubItemSelected(nullptr);
            }
            stopStreamPipeline();
            if (m_vehicleOverlay        != nullptr) m_vehicleOverlay->setVisibility(UI::Visibility::GONE);
            if (m_vehicleViewport       != nullptr) m_vehicleViewport->setVisibility(UI::Visibility::GONE);
            if (m_streamLabelsOverlay   != nullptr) m_streamLabelsOverlay->setVisibility(UI::Visibility::GONE);
            clearMenuHint();

            if (m_menuRoot != nullptr)
            {
                auto* feat = m_menuRoot->find("ac_feature");
                if (feat != nullptr) feat->setSelectedChildIndex(0);
            }
        });
        buildAutoCalibMenuItems(autoCalibMenu);
        m_menuRoot->addChild(autoCalibMenu);
    }

    // ── Restore Default ───────────────────────────────────────────────────
    {
        auto restoreMenu = std::make_shared<UI::MenuItem>(
            "restore_default", TR("Restore Default"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(restoreMenu, "Restore Default");
        addIcon(restoreMenu, "04_icn_restore_default");
        restoreMenu->setShowPreview(false);
        restoreMenu->setOnActivate([this](UI::MenuItem*)
        {
            UI::Texture* icon = UI::TextureManager::getInstance().getTextureByName("09_icn_popup_system");
            m_popupVisible = true;
            m_popupType    = PopupType::PopupNone;
            if (m_popupView != nullptr)
            {
                m_popupView->setOnConfirm([this]() { hidePopup(); restoreDefaultForCurrentVehicle(); });
                m_popupView->setOnCancel([this]()  { hidePopup(); });
                m_popupView->show(TR("Restore_Default_Confirm"), "", icon,
                                  UI::PopupButtonMode::OK_CANCEL, TR("OK"), TR("Cancel"));
            }
        });
        m_menuRoot->addChild(restoreMenu);
    }
}

void AppCalib::buildCalibParamsMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    auto addParam = [&](const char* id, const char* labelKey, float lo, float hi)
    {
        auto item = std::make_shared<UI::MenuItem>(id, TR(labelKey), UI::MenuItemType::SLIDER);
        SET_LOCALIZED_MENU_ITEM(item, labelKey);
        item->setRange(lo, hi, 1.0f);
        item->setValueFloat(0.0f);
        item->setOnActivate([this](UI::MenuItem*) { startAdjustment(AdjustmentType::CalibParams, 1); });
        parent->addChild(item);
    };

    // Col 0: Vehicle spec
    addParam("vspec_width",     "Width",               0,    9999);
    addParam("vspec_length",    "Length",              0,    9999);
    addParam("vspec_height",    "Height",              0,    9999);
    addParam("vspec_fo",        "Front Overhang",      0,    9999);
    addParam("vspec_wb",        "Wheelbase",           0,    9999);
    addParam("vspec_ro",        "Rear Overhang",       0,    9999);
    addParam("vspec_ft",        "Front Track",         0,    9999);
    addParam("vspec_rt",        "Rear Track",          0,    9999);
    addParam("vspec_min_steer", "Min Steering Angle", -999,   999);
    addParam("vspec_max_steer", "Max Steering Angle", -999,   999);

    // Col 1: Pattern offsets + Detection/ROI
    addParam("arr_front_off", "Front Offset", 0, 9999);
    addParam("arr_right_off", "Right Offset", 0, 9999);
    addParam("arr_rear_off",  "Rear Offset",  0, 9999);
    addParam("arr_left_off",  "Left Offset",  0, 9999);

    {
        auto item = std::make_shared<UI::MenuItem>("arr_auto", TR("Auto Detect"), UI::MenuItemType::SLIDER);
        SET_LOCALIZED_MENU_ITEM(item, "Auto Detect");
        item->setRange(0, 1, 1.0f);
        item->setValueFloat(0.0f);
        item->setOnActivate([this](UI::MenuItem*) { startAdjustment(AdjustmentType::CalibParams, 1); });
        item->setOnValueChanged([this](UI::MenuItem* src)
        {
            if (m_menuRoot == nullptr) return;
            const bool autoOn = (static_cast<int>(src->getValueFloat()) == 1);
            for (const char* id : { "arr_roi_x", "arr_roi_y", "arr_roi_w", "arr_roi_h", "arr_cma" })
            {
                auto* dep = m_menuRoot->find(id);
                if (dep != nullptr) dep->setEnabled(autoOn);
            }
        });
        parent->addChild(item);
    }

    addParam("arr_roi_x", "ROI Start X",      0, 9999);
    addParam("arr_roi_y", "ROI Start Y",      0, 9999);
    addParam("arr_roi_w", "ROI Width",        0, 9999);
    addParam("arr_roi_h", "ROI Height",       0, 9999);
    addParam("arr_cma",   "Contour Max Area", 0, 9999);

    // Col 2: Camera num/ idex / pattern
    addParam("cam_num", "Camera Number", 1, 6);
    {
        auto item = std::make_shared<UI::MenuItem>("cam_idx", TR("Camera Index"), UI::MenuItemType::SLIDER);
        SET_LOCALIZED_MENU_ITEM(item, "Camera Index");
        item->setRange(0, 9999, 1.0f);
        item->setValueFloat(0.0f);
        item->setZeroPad(true);
        item->setOnActivate([this](UI::MenuItem*) { startAdjustment(AdjustmentType::CalibParams, 1); });
        parent->addChild(item);
    }

    {
        auto item = std::make_shared<UI::MenuItem>("pat_num", TR("Pattern Number"), UI::MenuItemType::SLIDER);
        SET_LOCALIZED_MENU_ITEM(item, "Pattern Number");
        item->setRange(4, 6, 2.0f);
        item->setValueFloat(4.0f);
        item->setOnActivate([](UI::MenuItem* src)
        {
            src->setValueFloat(static_cast<int>(src->getValueFloat()) == 4 ? 6.0f : 4.0f);
        });

        auto lastValid = std::make_shared<int>(4);
        item->setOnValueChanged([this, lastValid](UI::MenuItem* src)
        {
            if (m_menuRoot == nullptr) return;
            int v = static_cast<int>(src->getValueFloat());
            if (v != 4 && v != 6)
            {
                v = (*lastValid == 4) ? 6 : 4;
                src->setValueFloat(static_cast<float>(v));
                return;
            }
            *lastValid = v;

            auto* d2 = m_menuRoot->find("pat_dist2");
            auto* d3 = m_menuRoot->find("pat_dist3");
            if (d2 != nullptr) d2->setEnabled(v >= 5);
            if (d3 != nullptr) d3->setEnabled(v >= 6);

            if (m_menuNavigator != nullptr)
            {
                UI::SubMenuView* smv = m_menuNavigator->getSubMenuView();
                if (smv == nullptr) return;
                UI::ImageView* img = smv->getPreviewImage();
                if (img == nullptr) return;
                const char* texName = (v >= 5) ? "04_bg_pattern_8" : "04_bg_pattern_4";
                UI::Texture* t = UI::TextureManager::getInstance().getTextureByName(texName);
                if (t != nullptr) { img->setScaleType(UI::ScaleType::FIT_CENTER); img->setTexture(t, false); }
            }
        });
        parent->addChild(item);
    }

    addParam("pat_unit",  "Unit Space",    0, 999);
    addParam("pat_dist1", "1st Distance",  0, 999);
    addParam("pat_dist2", "2nd Distance",  0, 999);
    addParam("pat_dist3", "3rd Distance",  0, 999);
}

void AppCalib::buildCaptureMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    auto prevItem = std::make_shared<UI::MenuItem>(
        "prev_captured", TR("Previous Captured"), UI::MenuItemType::ACTION);
    SET_LOCALIZED_MENU_ITEM(prevItem, "Previous Captured");
    prevItem->setOnActivate([this](UI::MenuItem*) { startStreamPipeline(false); });
    parent->addChild(prevItem);

    auto liveItem = std::make_shared<UI::MenuItem>(
        "live_view", TR("Live View"), UI::MenuItemType::CATEGORY);
    SET_LOCALIZED_MENU_ITEM(liveItem, "Live View");
    liveItem->setOnActivate([this](UI::MenuItem*) { startStreamPipeline(true); });

    auto captureBtn = std::make_shared<UI::MenuItem>(
        "capture_btn", TR("Capture"), UI::MenuItemType::ACTION);
    SET_LOCALIZED_MENU_ITEM(captureBtn, "Capture");
    captureBtn->setOnActivate([this](UI::MenuItem*)
    {
        UI::Texture* icon = UI::TextureManager::getInstance().getTextureByName("09_icn_popup_system");
        m_popupVisible = true;
        m_popupType    = PopupType::PopupNone;
        if (m_popupView == nullptr) return;
        m_popupView->setOnConfirm([this]() { hidePopup(); startCapturePipeline(); });
        m_popupView->setOnCancel([this]()  { hidePopup(); });
        m_popupView->show(TR("Capture_Confirm"), "", icon,
                          UI::PopupButtonMode::OK_CANCEL, TR("OK"), TR("Cancel"));
    });
    liveItem->addChild(captureBtn);
    parent->addChild(liveItem);
}

void AppCalib::buildAutoCalibMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    // Simple view items: request goes to context, lock is enforced there
    auto addView = [&](const char* id, const char* key)
    {
        auto item = std::make_shared<UI::MenuItem>(id, TR(key), UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(item, key);
        item->setOnActivate([this](UI::MenuItem*) { clearMenuHint(); });
        parent->addChild(item);
    };

    addView("ac_fisheye", "Fisheye View");
    addView("ac_defisheye", "Defisheye View");

    // Feature View — CATEGORY with 4 inline camera tabs
    {
        auto featureItem = std::make_shared<UI::MenuItem>(
            "ac_feature", TR("Feature View"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(featureItem, "Feature View");

        for (int cam = 0; cam < SVM_CAMERAS_NUM; cam++)
        {
            auto camItem = std::make_shared<UI::MenuItem>(
                "ac_feature_cam" + std::to_string(cam),
                std::to_string(cam + 1),
                UI::MenuItemType::ACTION);
            camItem->setOnActivate([this, cam](UI::MenuItem* sender)
            {
                if (sender != nullptr && sender->isEnabled() == false) return;
                m_pendingAdjCam   = cam;
                m_pendingPointPos = UI::Vec2{-1.f, -1.f};
            });
            featureItem->addChild(camItem);
        }

        featureItem->setOnValueChanged([this](UI::MenuItem*)  { clearMenuHint(); });
        featureItem->setOnActivate   ([this](UI::MenuItem*)   { clearMenuHint(); });
        parent->addChild(featureItem);
    }

    addView("ac_contour", "Contour View");
    addView("ac_grid", "Grid View");
    addView("ac_mask", "Mask View");
    addView("ac_result", "Result View");
}

// ============================================================================
// Vehicle data
// ============================================================================

void AppCalib::loadVehicleData()
{
    M_MENU_CALIB currentCalib;
    APP::appConf.TryReadCalibMenuInfoFromXml(
        std::string(_SETTINGS_PATH_) + "/settings_common.xml", currentCalib);

    const std::string listPath = std::string(_SVM_RESOURCES_PATH_) + "/vehicle_list.txt";
    std::ifstream file(listPath);
    if (file.is_open() == false)
    {
        LOG_APP_WARNINGF("AppCalib: vehicle_list.txt not found at %s", listPath.c_str());
        return;
    }

    m_calibMenuData.clear();
    m_vehiclesMap.clear();
    m_uniqueVehicle.clear();
    m_vehicleTexturePaths.clear();
    m_vehicleModelPaths.clear();
    m_currentVehicleIdx = 0;
    m_hoveredVehicleIdx = 0;
    int idx = 0;

    try
    {
        std::string name;
        while (file >> name)
        {
            if (name[0] == '#') continue;
            if (m_uniqueVehicle.find(name) != m_uniqueVehicle.end()) continue;

            m_vehiclesMap[idx] = name;
            m_uniqueVehicle.insert(name);

            M_MENU_CALIB cm;
            const std::string base = std::string(_SETTINGS_PATH_) + "/" + name + "/backup/";
            APP::appConf.TryReadCalibMenuInfoFromXml(base + "settings_view.xml",        cm);
            APP::appConf.TryReadCalibMenuInfoFromXml(base + "settings_common.xml",      cm);
            APP::appConf.TryReadCalibMenuInfoFromXml(base + "settings_calibration.xml", cm);
            m_calibMenuData.push_back(cm);

            if (currentCalib.veh_info.model_name   == cm.veh_info.model_name   &&
                currentCalib.veh_info.vehicle_type == cm.veh_info.vehicle_type &&
                currentCalib.veh_info.vehicle_id   == cm.veh_info.vehicle_id)
            {
                m_currentVehicleIdx                   = idx;
                m_hoveredVehicleIdx                   = idx;
                APP::appConf.system_calib_vehicle_idx = idx;
                APP::appConf.writeConfig(app_conf_file);
            }

            const size_t dot = cm.model.model_file.rfind('.');
            std::string modelPath, texPath;
            if (dot != std::string::npos)
            {
                const std::string stem = std::string(_MODELS_PATH_) + "/" +
                                         cm.model.model_file.substr(0, dot);
                modelPath = stem + ".glb";
                texPath   = stem + ".png";
            }
            m_vehicleModelPaths.push_back(modelPath);
            m_vehicleTexturePaths.push_back(texPath);
            if (texPath.empty() == false) UI::TextureManager::getInstance().loadTexture(texPath);

            ++idx;
        }
    }
    catch (const std::exception& e)
    {
        LOG_APP_ERRORF("AppCalib: Error reading vehicle_list.txt: %s", e.what());
    }
    file.close();
    LOG_APP_INFOF("AppCalib: Loaded %d vehicles, current idx=%d", idx, m_currentVehicleIdx);
}

void AppCalib::populateVehicleListItems()
{
    auto* vehicleCat = m_menuRoot->find("select_vehicle");
    if (vehicleCat == nullptr) return;

    std::vector<std::string> ids;
    ids.reserve(vehicleCat->getChildren().size());
    for (const auto& child : vehicleCat->getChildren())
        ids.push_back(child->getId());
    for (const auto& id : ids)
        vehicleCat->removeChild(id);

    for (int i = 0; i < static_cast<int>(m_vehiclesMap.size()); i++)
    {
        auto item = std::make_shared<UI::MenuItem>(
            "vehicle_" + std::to_string(i), m_vehiclesMap[i], UI::MenuItemType::ACTION);
        item->setOnActivate([this, i](UI::MenuItem*)
        {
            m_hoveredVehicleIdx = i;
            syncVehiclePreview(m_hoveredVehicleIdx);
            showVehicleConfirmPopup(m_hoveredVehicleIdx);
        });
        vehicleCat->addChild(item);
    }
    vehicleCat->setSelectedChildIndex(m_currentVehicleIdx);
}

void AppCalib::syncVehiclePreview(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(m_calibMenuData.size())) return;
    if (getSubMenuView(m_menuNavigator.get()) == nullptr) return;

    loadVehicleModel(idx);

    if (m_vehicleOverlay != nullptr)
    {
        const M_MENU_CALIB& cm = m_calibMenuData[idx];
        auto* tv0 = dynamic_cast<UI::TextView*>(m_vehicleOverlay->getChildAt(0));
        auto* tv1 = dynamic_cast<UI::TextView*>(m_vehicleOverlay->getChildAt(1));
        auto* tv2 = dynamic_cast<UI::TextView*>(m_vehicleOverlay->getChildAt(2));
        if (tv0 != nullptr) tv0->setText(TR("Manufacturer") + ": " + cm.veh_info.manufacturer);
        if (tv1 != nullptr) tv1->setText(TR("Model")        + ": " + cm.veh_info.model_name);
        if (tv2 != nullptr) tv2->setText(TR("Year")         + ": " + cm.veh_info.model_year);
        m_vehicleOverlay->setVisibility(UI::Visibility::VISIBLE);
    }
}

void AppCalib::loadVehicleModel(int idx)
{
    UI::SubMenuView* smv          = getSubMenuView(m_menuNavigator.get());
    UI::ImageView*   previewImage = (smv != nullptr) ? smv->getPreviewImage() : nullptr;
    if (previewImage == nullptr || m_vehicleViewport == nullptr) return;

    const bool has3D = (idx < static_cast<int>(m_vehicleModelPaths.size()) &&
                        m_vehicleModelPaths[idx].empty() == false);

    if (has3D == false)
    {
        previewImage->setVisibility(UI::Visibility::VISIBLE);
        m_vehicleViewport->setVisibility(UI::Visibility::GONE);

        UI::Texture* tex = nullptr;
        if (idx < static_cast<int>(m_vehicleTexturePaths.size()) &&
            m_vehicleTexturePaths[idx].empty() == false)
            tex = UI::TextureManager::getInstance().getTexture(m_vehicleTexturePaths[idx]);
        if (tex == nullptr)
            tex = UI::TextureManager::getInstance().getTextureByName("08_bg_fileview");
        if (tex != nullptr) { previewImage->setScaleType(UI::ScaleType::FIT_CENTER); previewImage->setTexture(tex, false); }
        return;
    }

    previewImage->setVisibility(UI::Visibility::GONE);
    m_vehicleViewport->setVisibility(UI::Visibility::VISIBLE);

    m_vehicleInstance.unload();
    if (m_vehicleModel != nullptr) { m_vehicleModel->release(); delete m_vehicleModel; m_vehicleModel = nullptr; }
    m_vehicleViewport->getScene().clear();
    m_vehicleViewport->addLight(UI3D::Light::makeDirectional(
        UI3D::Vec3(-0.5f, -1.0f, -0.8f).normalized(), UI::Color::White, 1.2f));
    m_vehicleViewport->addLight(UI3D::Light::makeDirectional(
        UI3D::Vec3( 1.0f, -0.3f,  0.5f).normalized(), UI::Color::White, 0.4f));

    m_vehicleModel = UI3D::ModelLoader::loadFromFile(m_vehicleModelPaths[idx]);
    if (m_vehicleModel == nullptr)
    {
        LOG_APP_WARNINGF("AppCalib: Failed to load 3D model: %s", m_vehicleModelPaths[idx].c_str());
        return;
    }
    if (m_vehicleInstance.load(m_vehicleModel, *m_vehicleViewport) == false)
    {
        LOG_APP_WARNINGF("AppCalib: Model has no renderable geometry: %s", m_vehicleModelPaths[idx].c_str());
        return;
    }

    const float modelCentY = m_vehicleInstance.getGroundLift();
    m_vehicleInstance.setPosition(UI3D::Vec3(0.0f, modelCentY, 0.0f));

    const UI::RectF& vb     = m_vehicleViewport->getBounds();
    const float      vpW    = vb.width();
    const float      vpH    = vb.height();
    const float      aspect = (vpW > 1.0f && vpH > 1.0f) ? (vpW / vpH) : 1.0f;

    constexpr float kDist = 3.2f;
    UI3D::Camera cam;
    cam.setPosition(UI3D::Vec3(kDist * 0.7f, kDist * 0.4f + modelCentY, kDist * 0.7f));
    cam.setTarget(UI3D::Vec3(0.0f, modelCentY, 0.0f));
    cam.setUp(UI3D::Vec3(0.0f, 1.0f, 0.0f));
    cam.setPerspective(0.7854f, aspect, 0.05f, 50.0f);
    m_vehicleViewport->setCamera(cam);
    m_vehicleViewport->setAutoSpin(0.4f);
}

void AppCalib::showVehicleConfirmPopup(int vehicleIdx)
{
    if (vehicleIdx < 0 || vehicleIdx >= static_cast<int>(m_vehiclesMap.size())) return;

    UI::Texture*      icon        = UI::TextureManager::getInstance().getTextureByName("04_icn_vehicle_select");
    const std::string vehicleName = m_vehiclesMap[vehicleIdx];

    m_popupVisible = true;
    m_popupType    = PopupType::PopupNone;
    if (m_popupView == nullptr) return;

    m_popupView->setOnConfirm([this, vehicleIdx]()
    {
        hidePopup();
        if (vehicleIdx != m_currentVehicleIdx) applyVehicleSelection(vehicleIdx);
    });
    m_popupView->setOnCancel([this]() { hidePopup(); });
    m_popupView->show(TR("Calib_vehicleChange"), "( " + vehicleName + " )", icon,
                      UI::PopupButtonMode::OK_CANCEL, TR("OK"), TR("Cancel"));
}

void AppCalib::applyVehicleSelection(int vehicleIdx)
{
    if (vehicleIdx < 0 || vehicleIdx >= static_cast<int>(m_vehiclesMap.size())) return;

    std::thread([this, vehicleIdx]()
    {
        static const std::string paths[] = {
            _SETTINGS_PATH_, _ARRAYS_PATH_,
            _SOURCE_IMAGES_PATH_, _MAPPINGS_PATH_, _MASK_IMAGES_PATH_
        };
        for (const auto& p : paths)
        {
            const std::string backup  = p + "/" + m_vehiclesMap[m_currentVehicleIdx] + "/backup";
            const std::string restore = p + "/" + m_vehiclesMap[vehicleIdx]          + "/backup";
            if (system(("find " + p + "/ -maxdepth 1 -type f -exec mv {} " + backup  + "/ \\;").c_str())) {}
            if (system(("find " + restore + "/ -maxdepth 1 -type f -exec cp {} " + p + "/ \\;").c_str())) {}
        }
        m_currentVehicleIdx                   = vehicleIdx;
        m_calibNeedRefreshContext             = true;
        APP::appConf.system_calib_vehicle_idx = vehicleIdx;
        loadCalibParamsData();
        APP::appConf.writeConfig(app_conf_file, true);

        auto vehicleCat = m_menuRoot->find("select_vehicle");
        if (vehicleCat != nullptr) vehicleCat->setSelectedChildIndex(m_currentVehicleIdx);
        usleep(1000 * 1000);
    }).detach();
}

void AppCalib::restoreDefaultForCurrentVehicle()
{
    std::thread([this]()
    {
        static const std::string paths[] = {
            _SETTINGS_PATH_, _ARRAYS_PATH_,
            _SOURCE_IMAGES_PATH_, _MAPPINGS_PATH_, _MASK_IMAGES_PATH_
        };
        for (const auto& p : paths)
        {
            const std::string def = p + "/" + m_vehiclesMap[m_currentVehicleIdx] + "/default";
            if (system(("find " + def + "/ -maxdepth 1 -type f -exec cp {} " + p + "/ \\;").c_str())) {}
        }
        m_calibNeedRefreshContext = true;
        loadCalibParamsData();
        APP::appConf.writeConfig(app_conf_file, true);
        usleep(1000 * 1000);
    }).detach();
}

// ============================================================================
// Calib params — load / save
// ============================================================================

void AppCalib::loadCalibParamsData()
{
    M_MENU_CALIB& cm = APP::appConf.menu_calib;
    APP::appConf.TryReadCalibMenuInfoFromXml(
        std::string(_SETTINGS_PATH_) + "/settings_calibration.xml", cm);
    APP::appConf.TryReadCalibMenuInfoFromXml(
        std::string(_SETTINGS_PATH_) + "/settings_common.xml", cm);

    if (m_menuRoot == nullptr) return;

    auto setF = [this](const std::string& id, float v)
    {
        auto item = m_menuRoot->find(id);
        if (item != nullptr) item->setValueFloat(v);
    };
    auto setI = [&setF](const std::string& id, int v) { setF(id, static_cast<float>(v)); };

    setF("vspec_width",     cm.veh_spec.width);
    setF("vspec_length",    cm.veh_spec.length);
    setF("vspec_height",    cm.veh_spec.height);
    setF("vspec_fo",        cm.veh_spec.front_overhang);
    setF("vspec_wb",        cm.veh_spec.wheel_base);
    setF("vspec_ro",        cm.veh_spec.rear_overhang);
    setF("vspec_ft",        cm.veh_spec.front_track);
    setF("vspec_rt",        cm.veh_spec.rear_track);
    setF("vspec_min_steer", cm.veh_spec.min_steering_angle);
    setF("vspec_max_steer", cm.veh_spec.max_steering_angle);

    setF("arr_front_off", cm.arrangement.pattern_offset_from_car.front_pattern_offset);
    setF("arr_right_off", cm.arrangement.pattern_offset_from_car.right_pattern_offset);
    setF("arr_rear_off",  cm.arrangement.pattern_offset_from_car.rear_pattern_offset);
    setF("arr_left_off",  cm.arrangement.pattern_offset_from_car.left_pattern_offset);
    setI("arr_auto",      cm.svm_calibration_type);
    setF("arr_roi_x",     cm.contour.roi_start_x);
    setF("arr_roi_y",     cm.contour.roi_start_y);
    setF("arr_roi_w",     cm.contour.roi_width);
    setF("arr_roi_h",     cm.contour.roi_height);
    setF("arr_cma",       cm.contour.contour_max_area);

    setI("cam_num",       cm.svm_cameras_num);
    setI("cam_idx",       APP::video_channels[0] * 1000
                          + APP::video_channels[1] * 100
                          + APP::video_channels[2] * 10
                          + APP::video_channels[3]);
    setI("pat_num",       cm.svm_patterns_num);
    setF("pat_unit",      cm.svm_pattern.unit_space);
    setF("pat_dist1",     cm.arrangement.distance_between_patterns.vertical_1st_distance);
    setF("pat_dist2",     cm.arrangement.distance_between_patterns.vertical_2nd_distance);
    setF("pat_dist3",     cm.arrangement.distance_between_patterns.vertical_3rd_distance);

    const bool autoDetect = (cm.svm_calibration_type == 1);
    for (const char* id : { "arr_roi_x", "arr_roi_y", "arr_roi_w", "arr_roi_h", "arr_cma" })
    {
        auto item = m_menuRoot->find(id);
        if (item != nullptr) item->setEnabled(autoDetect);
    }
    auto pd2 = m_menuRoot->find("pat_dist2");
    auto pd3 = m_menuRoot->find("pat_dist3");
    if (pd2 != nullptr) pd2->setEnabled(cm.svm_patterns_num >= 5);
    if (pd3 != nullptr) pd3->setEnabled(cm.svm_patterns_num >= 6);

    if (m_menuNavigator != nullptr)
    {
        UI::SubMenuView* smv = m_menuNavigator->getSubMenuView();
        if (smv != nullptr && smv->isGridMode() == true) smv->updateContent();
    }
}

void AppCalib::saveCalibParamsData()
{
    M_MENU_CALIB& cm = APP::appConf.menu_calib;

    auto getF = [this](const std::string& id) -> float
    {
        auto item = m_menuRoot->find(id);
        return (item != nullptr) ? item->getValueFloat() : 0.0f;
    };
    auto getI = [&getF](const std::string& id) -> int { return static_cast<int>(getF(id)); };

    cm.veh_spec.width              = getF("vspec_width");
    cm.veh_spec.length             = getF("vspec_length");
    cm.veh_spec.height             = getF("vspec_height");
    cm.veh_spec.front_overhang     = getF("vspec_fo");
    cm.veh_spec.wheel_base         = getF("vspec_wb");
    cm.veh_spec.rear_overhang      = getF("vspec_ro");
    cm.veh_spec.front_track        = getF("vspec_ft");
    cm.veh_spec.rear_track         = getF("vspec_rt");
    cm.veh_spec.min_steering_angle = getF("vspec_min_steer");
    cm.veh_spec.max_steering_angle = getF("vspec_max_steer");

    cm.arrangement.pattern_offset_from_car.front_pattern_offset    = getF("arr_front_off");
    cm.arrangement.pattern_offset_from_car.right_pattern_offset    = getF("arr_right_off");
    cm.arrangement.pattern_offset_from_car.rear_pattern_offset     = getF("arr_rear_off");
    cm.arrangement.pattern_offset_from_car.left_pattern_offset     = getF("arr_left_off");
    cm.svm_calibration_type                                        = getI("arr_auto");
    cm.contour.roi_start_x                                         = getF("arr_roi_x");
    cm.contour.roi_start_y                                         = getF("arr_roi_y");
    cm.contour.roi_width                                           = getF("arr_roi_w");
    cm.contour.roi_height                                          = getF("arr_roi_h");
    cm.contour.contour_max_area                                    = getF("arr_cma");
    cm.svm_cameras_num                                             = getI("cam_num");
    {
        const int packed       = getI("cam_idx");
        APP::video_channels[0] = (packed / 1000) % 10;
        APP::video_channels[1] = (packed /  100) % 10;
        APP::video_channels[2] = (packed /   10) % 10;
        APP::video_channels[3] = (packed        ) % 10;
    }
    cm.svm_patterns_num                                            = getI("pat_num");
    cm.svm_pattern.unit_space                                      = getF("pat_unit");
    cm.arrangement.distance_between_patterns.vertical_1st_distance = getF("pat_dist1");
    cm.arrangement.distance_between_patterns.vertical_2nd_distance = getF("pat_dist2");
    cm.arrangement.distance_between_patterns.vertical_3rd_distance = getF("pat_dist3");

    APP::appConf.UpdateCalibMenuInfoXmlFile(
        std::string(_SETTINGS_PATH_) + "/settings_common.xml",      cm);
    APP::appConf.UpdateCalibMenuInfoXmlFile(
        std::string(_SETTINGS_PATH_) + "/settings_calibration.xml", cm);
    write_string_pipeline_config(pipeline_conf_file);
}

// ============================================================================
// Capture / stream
// ============================================================================

void AppCalib::backupPreviousCapture()
{
    LOG_APP_INFO("AppCalib: Backing up previous captures");
    const std::string src = std::string(_SOURCE_IMAGES_PATH_);
    const std::string dst = src + "/backup";
    if (system(("[ ! -d " + dst + " ] && mkdir " + dst).c_str())) {}
    if (system(("find " + src + "/ -maxdepth 1 -type f -exec cp {} " + dst + "/ \\;").c_str())) {}
}

void AppCalib::startCapturePipeline()
{
    LOG_APP_INFO("AppCalib: Starting capture pipeline");
    stopStreamPipeline();
    backupPreviousCapture();
    APP::capture_taken     = false;
    APP::capture_requested = true;
}

void AppCalib::startStreamPipeline(bool live)
{
    static constexpr const char* MIXER_HEAD =
        "glvideomixer name=mix "
        "sink_0::xpos=0   sink_0::ypos=540 sink_0::width=960 sink_0::height=540 "
        "sink_1::xpos=960 sink_1::ypos=0   sink_1::width=960 sink_1::height=540 "
        "sink_2::xpos=960 sink_2::ypos=540 sink_2::width=960 sink_2::height=540 "
        "sink_3::xpos=0   sink_3::ypos=0   sink_3::width=960 sink_3::height=540 "
        "! glcolorconvert "
        "! video/x-raw(memory:GLMemory),width=1920,height=1080,format=RGB "
        "! gldownload ! queue ! appsink name=streamsink";

    if (live == false)
    {
        const std::string fp = std::string(_SOURCE_IMAGES_PATH_);
        gchar* pstr = g_strdup_printf("%s "
            "filesrc location=%s/src0.jpg ! decodebin ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_0 "
            "filesrc location=%s/src1.jpg ! decodebin ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_1 "
            "filesrc location=%s/src2.jpg ! decodebin ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_2 "
            "filesrc location=%s/src3.jpg ! decodebin ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_3",
            MIXER_HEAD, fp.c_str(), fp.c_str(), fp.c_str(), fp.c_str());

        m_streamPipeline = (pstr != nullptr) ? std::string(pstr) : std::string();
        g_free(pstr);
    }
    else
    {
        #if defined(USE_CAMERA) && defined(EMBEDDED_DEVICE)
            gchar* pstr = g_strdup_printf("%s "
                "v4l2src device=/dev/video0 io-mode=dmabuf ! video/x-raw,width=%d,height=%d,framerate=%d/1 ! queue ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_0 "
                "v4l2src device=/dev/video1 io-mode=dmabuf ! video/x-raw,width=%d,height=%d,framerate=%d/1 ! queue ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_1 "
                "v4l2src device=/dev/video2 io-mode=dmabuf ! video/x-raw,width=%d,height=%d,framerate=%d/1 ! queue ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_2 "
                "v4l2src device=/dev/video3 io-mode=dmabuf ! video/x-raw,width=%d,height=%d,framerate=%d/1 ! queue ! videoscale ! video/x-raw,width=960,height=540 ! glupload ! mix.sink_3",
                MIXER_HEAD, IMG_WIDTH, IMG_HEIGHT, SRC_FRAMERATE, IMG_WIDTH, IMG_HEIGHT, SRC_FRAMERATE, IMG_WIDTH, IMG_HEIGHT, SRC_FRAMERATE, IMG_WIDTH, IMG_HEIGHT, SRC_FRAMERATE);

            m_streamPipeline = (pstr != nullptr) ? std::string(pstr) : std::string();
            g_free(pstr);
        #else
            gchar* pstr = g_strdup_printf("%s "
                "videotestsrc pattern=ball   is-live=true ! video/x-raw,width=960,height=540,framerate=%d/1 ! glupload ! mix.sink_0 "
                "videotestsrc pattern=snow   is-live=true ! video/x-raw,width=960,height=540,framerate=%d/1 ! glupload ! mix.sink_1 "
                "videotestsrc pattern=smpte  is-live=true ! video/x-raw,width=960,height=540,framerate=%d/1 ! glupload ! mix.sink_2 "
                "videotestsrc pattern=pinwheel is-live=true ! video/x-raw,width=960,height=540,framerate=%d/1 ! glupload ! mix.sink_3",
                MIXER_HEAD, SRC_FRAMERATE, SRC_FRAMERATE, SRC_FRAMERATE, SRC_FRAMERATE);

            m_streamPipeline = (pstr != nullptr) ? std::string(pstr) : std::string();
            g_free(pstr);
        #endif
    }

    m_streamPipelineUpdated = true;
}

void AppCalib::syncStreamPipeline(bool stop)
{
    if (stop == true) { stopStreamPipeline(); return; }
    UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
    const int selIdx = (smv != nullptr) ? smv->getSelectedSubItemIndex() : 0;
    startStreamPipeline(selIdx == 1);
}

void AppCalib::stopStreamPipeline()
{
    m_streamPipeline        = "";
    m_streamPipelineUpdated = true;
    m_captureOwnedTex.reset();

    UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
    if (smv == nullptr) return;
    UI::ImageView* img = smv->getPreviewImage();
    if (img == nullptr) return;
    UI::Texture* bg = UI::TextureManager::getInstance().getTextureByName("08_bg_fileview_4");
    if (bg != nullptr) { img->setScaleType(UI::ScaleType::FIT_CENTER); img->setTexture(bg, false); }
}

// ============================================================================
// Preview update — called from onUpdate() every frame
// ============================================================================

void AppCalib::updateCalibPreview()
{
    if (m_menuNavigator == nullptr) return;
    UI::SubMenuView* smv = getSubMenuView(m_menuNavigator.get());
    if (smv == nullptr) return;
    UI::MenuItem*  current      = m_menuNavigator->getCurrentItem();
    UI::ImageView* previewImage = smv->getPreviewImage();
    if (current == nullptr || previewImage == nullptr) return;

    const std::string& id = current->getId();
    if (id == "select_vehicle") return;

    if (id == "calib_params")
        { previewImage->setVisibility(UI::Visibility::VISIBLE); return; }

    if (id == "capture")
    {
        if (m_captureOwnedTex == nullptr || m_captureOwnedTex->id == 0) return;
        previewImage->setVisibility(UI::Visibility::VISIBLE);
        previewImage->setScaleType(UI::ScaleType::FIT_CENTER);
        previewImage->setTexture(m_captureOwnedTex.get(), false);
        return;
    }

    if (id == "auto_calib")
    {
        UI::Texture* fbTex = m_calibFb.getTexture();
        if (fbTex == nullptr) return;
        previewImage->setVisibility(UI::Visibility::VISIBLE);
        previewImage->setScaleType(UI::ScaleType::FIT_CENTER);
        previewImage->setTexture(fbTex, true);
    }
}

// ============================================================================
// Config / texture / framebuffer helpers
// ============================================================================

void AppCalib::loadCalibConfigValues()
{
    loadVehicleData();
    populateVehicleListItems();
    loadCalibParamsData();
}

void AppCalib::setCapturePanelTexture(unsigned int id)
{
    m_captureLeftPanelTexId = id;
    if (m_captureOwnedTex == nullptr) m_captureOwnedTex = std::make_unique<UI::Texture>();
    m_captureOwnedTex->setExternalTexture(id, WND_WIDTH, WND_HEIGHT);
}

void         AppCalib::bindCalibFb()   { m_calibFb.bind();   }
void         AppCalib::unbindCalibFb() { m_calibFb.unbind(); }
unsigned int AppCalib::getCalibFbTextureId() const
{
    UI::Texture* t = m_calibFb.getTexture();
    return (t != nullptr) ? t->id : 0;
}

} // namespace APP
