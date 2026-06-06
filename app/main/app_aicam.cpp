#include "app_aicam.h"
#include "app_vars.h"
#include "logger.h"
#include "ui_locale.h"
#include <algorithm>

namespace APP
{

// ============================================================================
// AICameraCell
// ============================================================================

AICameraCell::AICameraCell(int camIdx)
    : UI::GridCellView()
    , m_camIdx(camIdx)
{
    m_roiLineColors[0] = UI::Color4::fromRGBA(255,   0,   0, m_roiLineAlpha);
    m_roiLineColors[1] = UI::Color4::fromRGBA(255, 255,   0, m_roiLineAlpha);
    m_roiFillColors[0] = UI::Color4::fromRGBA(255,   0,   0, m_roiFillAlpha);
    m_roiFillColors[1] = UI::Color4::fromRGBA(255, 255,   0, m_roiFillAlpha);
    m_roiFillColors[2] = UI::Color4::fromRGBA(  0,   0,   0,              0); // transparent = no overlap
}

UI::Button* AICameraCell::addCamAction(UI::Texture* icon, ActionCallback cb)
{
    UI::ActionButton btn;
    btn.setColors(UI::Color::CardBackground, UI::Color::CardBackground,
                  UI::Color::AccentSecondary, UI::Color::CardBackground);
    int idx = m_camIdx;
    btn.callback = [idx, cb]() { if (cb != nullptr) cb(idx); };
    addAction(btn);

    UI::Button* btnView = getActionButton(getActionCount() - 1);
    if (btnView != nullptr && icon != nullptr)
    {
        btnView->setTexture(icon);
        btnView->setImageSize(24.0f, 24.0f);
    }
    return btnView;
}

void AICameraCell::updateROI(const std::array<XY, AI_CAM_ROI_PTS_NUM>& roiPoints, int roiLevel)
{
    UI::AIImageView* iv = getAIImageView();
    if (iv == nullptr) return;

    std::vector<UI::Vec2> pts;
    pts.reserve(AI_CAM_ROI_PTS_NUM);
    for (const XY& pt : roiPoints)
        pts.push_back({ static_cast<float>(pt.x), static_cast<float>(pt.y) });

    AI::ROIZone zone(pts, UI::Color::Transparent, m_roiLineColors[roiLevel], 5.0f, true);
    iv->addROIZone(zone);
}

int AICameraCell::updateDetections(const std::vector<AI::BoundingBox>&        detections,
                                   const std::vector<std::vector<UI::Vec2>>& roiLevelsNorm)
{
    UI::AIImageView* iv = getAIImageView();

    int worstLevel = -1;   // lowest-index level hit = highest danger

    const UI::Color4& noOverlapColor = m_roiFillColors[AI_CAM_ROI_LEVEL_MAX_NUM];

    std::vector<AI::BoundingBox> colored = detections;
    for (AI::BoundingBox& bb : colored)
    {
        int lvl = AI::bboxWarnLevel(bb, roiLevelsNorm);
        bb.color = (lvl >= 0) ? m_roiFillColors[lvl] : noOverlapColor;
        if (lvl >= 0 && (worstLevel < 0 || lvl < worstLevel)) worstLevel = lvl;
    }

    if (iv != nullptr) iv->setBoundingBoxes(colored);
    setWarningState(worstLevel >= 0);
    return worstLevel;   // -1 = no hit, 0..N = most dangerous level that was triggered
}

void AICameraCell::setWarningState(bool warning) { setWarningVisible(warning); }

// ============================================================================
// Constructor / Destructor
// ============================================================================

AppAICam::AppAICam()
    : AppShell()
    , m_cameraCells{nullptr}
    , m_currentCameraTexture{nullptr}
    , m_selectedLevelIndex(0)
    , m_roiDragPointIndex(-1)
    , m_roiDragActive(false)
    , m_fullscreenCamIndex(-1)
{
}

AppAICam::~AppAICam() {}

// ============================================================================
// AppShell lifecycle overrides
// ============================================================================

bool AppAICam::onInitialize()
{
    if (AppShell::onInitialize() == false) return false;
    initializeCameraFocusContexts();
    return true;
}

void AppAICam::onUpdate(float deltaTime)
{
    AppShell::onUpdate(deltaTime);
    updateCameraTextures();
    #ifdef AICAM_SIM_DETECTIONS
        updateSimDetections(deltaTime);
    #endif
    updateCameraCellData();

    // Update preview after navigator has processed dpad/touch — index is now current
    if (m_menuVisible == true && m_menuNavigator != nullptr)
    {
        UI::MenuItem* current = m_menuNavigator->getCurrentItem();
        if (current != nullptr && current->getId() == "safezone")
            updateSafeZonePreview();
    }
}

void AppAICam::onRender()
{
    AppShell::onRender();
}

bool AppAICam::onKeyEvent(const IO::KeyEvent& event)
{
    if (event.action != IO::KeyAction::DOWN && event.action != IO::KeyAction::MULTIPLE)
        return false;

    // Fullscreen camera mode
    if (m_fullscreenCamIndex >= 0)
    {
        if (event.keyCode == IO::KeyCode::ESCAPE      ||
            event.keyCode == IO::KeyCode::DPAD_CENTER ||
            event.keyCode == IO::KeyCode::ENTER       ||
            event.keyCode == IO::KeyCode::SPACE)
        {
            exitCameraFullscreen();
            return true;
        }
        if (event.keyCode == IO::KeyCode::M || event.keyCode == IO::KeyCode::MENU)
            { showMenuScreen(); return true; }
        return true;
    }

    // ROI adjustment mode
    if (m_adjustmentType == AdjustmentType::CamROI)
    {
        constexpr float ROI_STEP = 5.0f;
        int camIdx   = APP::appConf.menu_aicam.selected_camIdx;
        int levelIdx = m_selectedLevelIndex;

        if (event.keyCode == IO::KeyCode::ESCAPE)
        {
            cancelAdjustment();
            return true;
        }

        if (event.keyCode == IO::KeyCode::DPAD_CENTER ||
            event.keyCode == IO::KeyCode::ENTER       ||
            event.keyCode == IO::KeyCode::SPACE)
        {
            confirmAdjustment();
            if (m_adjustmentType == AdjustmentType::CamROI)
            {
                m_roiDragPointIndex = m_adjustmentStepIndex;
                if (camIdx >= 0 && camIdx < AI_CAM_NUM && m_cameraCells[camIdx] != nullptr)
                {
                    UI::AIImageView* iv = m_cameraCells[camIdx]->getAIImageView();
                    if (iv != nullptr) iv->setDpadSelectedPoint(m_roiDragPointIndex);
                }
            }
            else
            {
                exitCamROIAdjustment();
            }
            return true;
        }

        float dx = 0.0f, dy = 0.0f;
        if      (event.keyCode == IO::KeyCode::DPAD_LEFT)  dx = -ROI_STEP;
        else if (event.keyCode == IO::KeyCode::DPAD_RIGHT) dx =  ROI_STEP;
        else if (event.keyCode == IO::KeyCode::DPAD_UP)    dy = -ROI_STEP;
        else if (event.keyCode == IO::KeyCode::DPAD_DOWN)  dy =  ROI_STEP;

        if ((dx != 0.0f || dy != 0.0f) && camIdx >= 0 && camIdx < AI_CAM_NUM)
        {
            int ptIdx = m_roiDragPointIndex;
            if (ptIdx >= 0 && ptIdx < AI_CAM_ROI_PTS_NUM)
            {
                XY& pt    = APP::appConf.menu_aicam.roi_pts[camIdx][levelIdx][ptIdx];
                float newX = std::max(0.0f, std::min(static_cast<float>(pt.x) + dx, static_cast<float>(WND_WIDTH)));
                float newY = std::max(0.0f, std::min(static_cast<float>(pt.y) + dy, static_cast<float>(WND_HEIGHT)));
                pt.x = static_cast<int>(newX);
                pt.y = static_cast<int>(newY);
                if (m_cameraCells[camIdx] != nullptr)
                {
                    m_cameraCells[camIdx]->getAIImageView()->clearROIZones();
                    for (int lvIdx = 0; lvIdx < getCamRoiLevelCount(camIdx); lvIdx++)
                        m_cameraCells[camIdx]->updateROI(APP::appConf.menu_aicam.roi_pts[camIdx][lvIdx], lvIdx);
                }
            }
        }
        return true;
    }

    // Delegate remainder to AppShell (handles menu toggle, datetime adj, etc.)
    if (AppShell::onKeyEvent(event) == true) return true;

    // Main screen: fullscreen toggle via DPAD_CENTER on focused camera cell
    if (event.keyCode == IO::KeyCode::DPAD_CENTER ||
        event.keyCode == IO::KeyCode::ENTER       ||
        event.keyCode == IO::KeyCode::SPACE)
    {
        UI::View* focused = m_focusManager.getFocusedView();
        if (focused != nullptr)
        {
            for (int i = 0; i < AI_CAM_NUM; i++)
                if (m_cameraCells[i] == focused) { toggleCameraFullscreen(i); return true; }
        }
    }
    return false;
}

bool AppAICam::onMotionEvent(const IO::MotionEvent& event)
{
    if (m_menuVisible == true)
    {
        // ROI drag during CamROI adjustment (handled inside menu motion)
        if (m_adjustmentType == AdjustmentType::CamROI)
        {
            int camIdx   = APP::appConf.menu_aicam.selected_camIdx;
            int levelIdx = m_selectedLevelIndex;

            if (camIdx >= 0 && camIdx < AI_CAM_NUM && m_cameraCells[camIdx] != nullptr)
            {
                UI::AIImageView* iv = m_cameraCells[camIdx]->getAIImageView();

                UI::FrameLayout* rootMenuView = (m_menuNavigator != nullptr)
                                                ? m_menuNavigator->getRootMenuView() : nullptr;
                UI::SubMenuView* subMenuView  = nullptr;
                if (rootMenuView != nullptr && rootMenuView->getChildCount() >= 2)
                    subMenuView = dynamic_cast<UI::SubMenuView*>(rootMenuView->getChildAt(1));

                UI::ImageView* previewImg = (subMenuView != nullptr) ? subMenuView->getPreviewImage() : nullptr;

                if (iv != nullptr && previewImg != nullptr)
                {
                    UI::RectF previewRect = previewImg->getImageRect();

                    if (event.action == IO::MotionAction::DOWN && previewRect.contains(event.x, event.y))
                    {
                        float pw = previewRect.width(), ph = previewRect.height();
                        if (pw > 0.0f && ph > 0.0f)
                        {
                            float nativeX = (event.x - previewRect.left) / pw * static_cast<float>(iv->getNativeW());
                            float nativeY = (event.y - previewRect.top)  / ph * static_cast<float>(iv->getNativeH());
                            constexpr float HIT_RADIUS = 30.0f;
                            int   hitPt    = -1;
                            float bestDist = HIT_RADIUS * HIT_RADIUS;
                            for (int pi = 0; pi < AI_CAM_ROI_PTS_NUM; pi++)
                            {
                                const XY& pt = APP::appConf.menu_aicam.roi_pts[camIdx][levelIdx][pi];
                                float dx = nativeX - static_cast<float>(pt.x);
                                float dy = nativeY - static_cast<float>(pt.y);
                                float dist2 = dx * dx + dy * dy;
                                if (dist2 < bestDist) { bestDist = dist2; hitPt = pi; }
                            }
                            m_roiDragPointIndex = hitPt;
                            m_roiDragActive     = (hitPt >= 0);
                            if (m_roiDragActive == true) { iv->setDpadSelectedPoint(m_roiDragPointIndex); return true; }
                        }
                    }
                    else if ((event.action == IO::MotionAction::MOVE || event.action == IO::MotionAction::UP) &&
                             m_roiDragActive == true && m_roiDragPointIndex >= 0)
                    {
                        float pw = previewRect.width(), ph = previewRect.height();
                        if (pw > 0.0f && ph > 0.0f)
                        {
                            float nX = std::max(0.0f, std::min((event.x - previewRect.left) / pw * static_cast<float>(iv->getNativeW()), static_cast<float>(iv->getNativeW())));
                            float nY = std::max(0.0f, std::min((event.y - previewRect.top)  / ph * static_cast<float>(iv->getNativeH()), static_cast<float>(iv->getNativeH())));
                            APP::appConf.menu_aicam.roi_pts[camIdx][levelIdx][m_roiDragPointIndex].x = static_cast<int>(nX);
                            APP::appConf.menu_aicam.roi_pts[camIdx][levelIdx][m_roiDragPointIndex].y = static_cast<int>(nY);
                            iv->clearROIZones();
                            for (int lvIdx = 0; lvIdx < getCamRoiLevelCount(camIdx); lvIdx++)
                                m_cameraCells[camIdx]->updateROI(APP::appConf.menu_aicam.roi_pts[camIdx][lvIdx], lvIdx);

                            if (event.action == IO::MotionAction::UP)
                            {
                                int draggedPt       = m_roiDragPointIndex;
                                m_roiDragActive     = false;
                                m_roiDragPointIndex = -1;
                                iv->setDpadSelectedPoint(-1);
                                if (draggedPt >= m_adjustmentStepIndex)
                                {
                                    int nextStep = draggedPt + 1;
                                    m_adjustmentStepIndex = (nextStep < m_adjustmentSteps) ? nextStep : draggedPt;
                                    m_roiDragPointIndex   = m_adjustmentStepIndex;
                                }
                                else
                                    m_roiDragPointIndex = m_adjustmentStepIndex;
                                iv->setDpadSelectedPoint(m_roiDragPointIndex);
                            }
                        }
                        return true;
                    }
                }
            }
        }
        return AppShell::onMotionEvent(event);
    }

    if (m_fullscreenCamIndex >= 0) return false;
    return AppShell::onMotionEvent(event);
}

bool AppAICam::onTerminalCommand(const std::string& command)
{
    if (AppShell::onTerminalCommand(command) == true) return true;

    if (command.substr(0, 2) == "fs" || command == "fullscreen")
    {
        int camIdx = -1;
        if (command.length() > 2 && command[2] == ' ')
            try { camIdx = std::stoi(command.substr(3)); } catch (...) { camIdx = -1; }

        if      (camIdx >= 0 && camIdx < AI_CAM_NUM) toggleCameraFullscreen(camIdx);
        else if (m_fullscreenCamIndex >= 0)          exitCameraFullscreen();
        else                                         enterCameraFullscreen(0);
        return true;
    }
    return false;
}

// ============================================================================
// AppShell hook — adds SafeZone category before System
// ============================================================================

void AppAICam::buildExtraMenuItems()
{
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();

    UI::Texture* warnTex = texMgr.getTextureByName("03_bg_warning_red");
    if (warnTex != nullptr)
    {
        for (int i = 0; i < AI_CAM_NUM; i++)
            if (m_cameraCells[i] != nullptr)
                m_cameraCells[i]->setWarningTexture(warnTex);
    }

    auto safeZone = std::make_shared<UI::MenuItem>("safezone", TR("SafeZone"), UI::MenuItemType::CATEGORY);
    SET_LOCALIZED_MENU_ITEM(safeZone, "SafeZone");
    UI::Texture* icn = texMgr.getTextureByName("04_icn_safezone");
    if (icn != nullptr) safeZone->setIconTexture(icn);
    safeZone->setShowPreview(true);
    buildSafeZoneMenuItems(safeZone);
    safeZone->setOnActivate([this](UI::MenuItem* item)
    {
        // On entry, advance selectedChildIndex past any leading disabled cameras
        const auto& children = item->getChildren();
        int sel = item->getSelectedChildIndex();
        // If current selection is already enabled, keep it
        if (sel >= 0 && sel < static_cast<int>(children.size())
            && children[sel] != nullptr && children[sel]->isEnabled() == true)
            return;
        // Otherwise find the first enabled child
        for (int i = 0; i < static_cast<int>(children.size()); ++i)
        {
            if (children[i] != nullptr && children[i]->isEnabled() == true)
            {
                item->setSelectedChildIndex(i);
                return;
            }
        }
    });
    safeZone->setOnValueChanged([this](UI::MenuItem*) { updateSafeZonePreview(); });
    m_menuRoot->addChild(safeZone);
}

// ============================================================================
// Focus Contexts
// ============================================================================

void AppAICam::initializeCameraFocusContexts()
{
    // m_mainScreenContext already created by AppShell::initializeFocusSystem();
    // register camera cells into it.
    for (int i = 0; i < AI_CAM_NUM; i++)
    {
        if (m_cameraCells[i] == nullptr) continue;
        m_cameraCells[i]->setFocusManager(&m_focusManager);
        m_focusManager.registerView(m_cameraCells[i], m_mainScreenContext);
    }

    m_fullscreenContext = m_focusManager.createContext(UI::FocusContextType::OVERLAY);
    m_fullscreenContext->setActive(false);
    m_fullscreenContext->setBlocking(true);
    m_fullscreenContext->setZOrder(100);
}

// ============================================================================
// Detection Simulation  (compiled only when AICAM_SIM_DETECTIONS is defined)
// ============================================================================

#ifdef AICAM_SIM_DETECTIONS
void AppAICam::updateSimDetections(float deltaTime)
{
    // One-time init: two FloatingObjects per camera, distinct positions/velocities/sizes.
    if (m_simInitialized == false)
    {
        // {startX, startY, velX, velY, w, h}  — all normalised [0,1]
        const float init[AI_CAM_NUM][SIM_OBJ_PER_CAM][6] = {
            { { 0.10f, 0.15f,  0.12f,  0.09f, 0.20f, 0.25f },
              { 0.55f, 0.50f, -0.07f,  0.11f, 0.13f, 0.16f } },
            { { 0.60f, 0.55f, -0.10f,  0.13f, 0.18f, 0.22f },
              { 0.20f, 0.70f,  0.09f, -0.08f, 0.24f, 0.15f } },
            { { 0.30f, 0.65f,  0.08f, -0.11f, 0.15f, 0.20f },
              { 0.70f, 0.25f, -0.11f,  0.07f, 0.22f, 0.18f } },
            { { 0.70f, 0.20f, -0.13f, -0.08f, 0.19f, 0.23f },
              { 0.25f, 0.45f,  0.10f,  0.12f, 0.12f, 0.17f } },
        };

        const UI::Color4 colors[AI_CAM_NUM][SIM_OBJ_PER_CAM] = {
            { UI::Color4::fromRGBA(255,  80,  80, 220), UI::Color4::fromRGBA(255, 160,  80, 220) },
            { UI::Color4::fromRGBA( 80, 220,  80, 220), UI::Color4::fromRGBA( 80, 220, 200, 220) },
            { UI::Color4::fromRGBA( 80, 120, 255, 220), UI::Color4::fromRGBA(180,  80, 255, 220) },
            { UI::Color4::fromRGBA(255, 230,  60, 220), UI::Color4::fromRGBA(255, 100, 180, 220) },
        };

        for (int cam = 0; cam < AI_CAM_NUM; cam++)
        {
            for (int o = 0; o < SIM_OBJ_PER_CAM; o++)
            {
                UI::FloatingObject& obj = m_simObjects[cam][o];
                obj.setShape(UI::FloatingShape::RECTANGLE);
                obj.setPosition({ init[cam][o][0], init[cam][o][1] });
                obj.setVelocity({ init[cam][o][2], init[cam][o][3] });
                obj.setSize    ({ init[cam][o][4], init[cam][o][5] });
                obj.setFilled(false);
                obj.setStrokeWidth(2.0f);
                obj.setFgColor(colors[cam][o]);
            }
        }
        m_simInitialized = true;
    }

    // Class labels cycling through a small set so class IDs vary over time.
    static const char* simLabels[] = { "person", "bicycle", "vehicle", "cone" };
    static const int   simLabelCount = static_cast<int>(std::size(simLabels));

    const UI::RectF normBounds(0.0f, 0.0f, 1.0f, 1.0f);

    for (int cam = 0; cam < AI_CAM_NUM; cam++)
    {
        APP::detected_objs[cam].clear();

        for (int o = 0; o < SIM_OBJ_PER_CAM; o++)
        {
            UI::FloatingObject& obj = m_simObjects[cam][o];
            obj.update(deltaTime, normBounds);

            const UI::Vec2& pos  = obj.getPosition();
            const UI::Vec2& size = obj.getSize();

            // Assign a label that slowly rotates so class IDs vary.
            int labelIdx = (cam * SIM_OBJ_PER_CAM + o) % simLabelCount;

            AI::BoundingBox bb;
            bb.x          = pos.x;
            bb.y          = pos.y;
            bb.width      = size.x;
            bb.height     = size.y;
            bb.color      = obj.getFgColor();
            bb.label      = simLabels[labelIdx];
            bb.confidence = 0.75f + static_cast<float>(o) * 0.1f;
            APP::detected_objs[cam].push_back(bb);
        }
    }
}
#endif // AICAM_SIM_DETECTIONS

// ============================================================================
// Camera Textures
// ============================================================================

void AppAICam::updateCameraTextures()
{
    for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
    {
        if (m_cameraCells[camIdx] == nullptr) continue;
        if (APP::cam_tex_ids[camIdx] == 0)    continue;

        if (m_currentCameraTexture[camIdx] == nullptr)
            m_currentCameraTexture[camIdx] = new UI::Texture();
 
        m_currentCameraTexture[camIdx]->setExternalTexture(APP::cam_tex_ids[camIdx], IMG_WIDTH, IMG_HEIGHT);
        m_cameraCells[camIdx]->setImage(m_currentCameraTexture[camIdx]);
    }
 
    onCameraTexturesUpdated();
}

void AppAICam::updateSafeZonePreview()
{
    if (m_menuNavigator == nullptr) return;
    UI::MenuItem* item = m_menuNavigator->getCurrentItem();
    if (item == nullptr || item->getId() != "safezone") return;

    UI::FrameLayout* rootMenuView = m_menuNavigator->getRootMenuView();
    if (rootMenuView == nullptr || rootMenuView->getChildCount() < 2) return;

    UI::SubMenuView* subMenuView = dynamic_cast<UI::SubMenuView*>(rootMenuView->getChildAt(1));
    if (subMenuView == nullptr) return;

    int subItemIndex = subMenuView->getSelectedSubItemIndex();
    int camIndex = -1, built = 0;
    for (int i = 0; i < AI_CAM_NUM; i++)
    {
        if (getCamRoiLevelCount(i) <= 0) continue;
        if (built == subItemIndex) { camIndex = i; break; }
        built++;
    }
    if (camIndex < 0 || camIndex >= AI_CAM_NUM) return;
    if (getCamSlotEnabled(camIndex) == false) return;

    APP::appConf.menu_aicam.selected_camIdx = camIndex;

    AICameraCell* camCell = m_cameraCells[camIndex];
    if (camCell == nullptr) return;

    UI::AIImageView* iv = camCell->getAIImageView();
    if (iv == nullptr || iv->isFramebufferEnabled() == false) return;

    UI::Texture*   fbTex        = iv->getFramebufferTexture();
    UI::ImageView* previewImage = subMenuView->getPreviewImage();
    if (fbTex == nullptr || previewImage == nullptr) return;

    previewImage->setScaleType(UI::ScaleType::FIT_CENTER);
    previewImage->setTexture(fbTex, true);
}

void AppAICam::updateCameraCellData()
{
    M_MENU_AICAM& flConf = APP::appConf.menu_aicam;

    // ROI config points are in native pixel space (0..WND_WIDTH / 0..WND_HEIGHT).
    // BoundingBoxes are in normalised [0,1] image space.
    // Convert once per frame so the geometry helpers share the same coordinate space.
    constexpr float invW = 1.0f / static_cast<float>(WND_WIDTH);
    constexpr float invH = 1.0f / static_cast<float>(WND_HEIGHT);

    for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
    {
        if (m_cameraCells[camIdx] == nullptr) continue;

        UI::AIImageView* iv = m_cameraCells[camIdx]->getAIImageView();
        if (iv != nullptr) iv->clearROIZones();

        const int numLevels = getCamRoiLevelCount(camIdx);

        // Build per-level normalised polygons (for both rendering and overlap test).
        std::vector<std::vector<UI::Vec2>> roiLevelsNorm(numLevels);
        for (int lvIdx = 0; lvIdx < numLevels; lvIdx++)
        {
            roiLevelsNorm[lvIdx].reserve(AI_CAM_ROI_PTS_NUM);
            for (const XY& pt : flConf.roi_pts[camIdx][lvIdx])
                roiLevelsNorm[lvIdx].push_back({ pt.x * invW, pt.y * invH });

            // updateROI passes native-pixel pts to AIImageView (renderer handles its own transform).
            m_cameraCells[camIdx]->updateROI(flConf.roi_pts[camIdx][lvIdx], lvIdx);
        }

        // Run overlap filter; updateDetections colours each bbox, sets the cell
        // warning overlay, and returns the worst ROI level hit (-1 = none).
        int worstLevel = m_cameraCells[camIdx]->updateDetections(APP::detected_objs[camIdx], roiLevelsNorm);
        APP::aicam_warning_level[camIdx] = (worstLevel >= 0) ? 1 : 0;
    }
}

// ============================================================================
// Fullscreen Management
// ============================================================================

void AppAICam::toggleCameraFullscreen(int camIdx)
{
    if (m_fullscreenCamIndex == camIdx) exitCameraFullscreen();
    else                                enterCameraFullscreen(camIdx);
}

void AppAICam::enterCameraFullscreen(int camIdx)
{
    if (camIdx < 0 || camIdx >= AI_CAM_NUM || m_cameraCells[camIdx] == nullptr) return;
    m_fullscreenCamIndex = camIdx;

    m_mainScreenContext->setActive(false);
    m_fullscreenContext->setActive(true);
    m_mainScreenContext->removeView(m_cameraCells[camIdx]);
    m_fullscreenContext->addView(m_cameraCells[camIdx]);
    m_focusManager.setFocus(m_cameraCells[camIdx]);

    onEnterCameraFullscreen(camIdx);
}

void AppAICam::exitCameraFullscreen()
{
    if (m_fullscreenCamIndex < 0) return;

    if (m_cameraCells[m_fullscreenCamIndex] != nullptr)
    {
        m_fullscreenContext->removeView(m_cameraCells[m_fullscreenCamIndex]);
        m_mainScreenContext->addView(m_cameraCells[m_fullscreenCamIndex]);
        m_focusManager.setFocus(m_cameraCells[m_fullscreenCamIndex]);
    }
    m_fullscreenContext->setActive(false);
    m_mainScreenContext->setActive(true);

    onExitCameraFullscreen();
    m_fullscreenCamIndex = -1;
}

// ============================================================================
// SafeZone Menu Builder
// ============================================================================

void AppAICam::buildSafeZoneMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
    {
        int numLevels = getCamRoiLevelCount(camIdx);
        if (numLevels <= 0) continue;

        const std::string label  = getCamLabel(camIdx);
        std::string locKey = label + "_Camera";
        std::string itemId = "cam_" + label;
        for (char& c : itemId) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        auto cam = std::make_shared<UI::MenuItem>(itemId, TR(locKey.c_str()), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(cam, locKey.c_str());
        cam->setShowPreview(true);
        cam->setEnabled(getCamSlotEnabled(camIdx));

        for (int li = 0; li < numLevels; li++)
        {
            const char* lbl = getCamRoiLevelLabel(camIdx, li);
            auto lvl = std::make_shared<UI::MenuItem>(itemId + "_lv" + std::to_string(li), lbl, UI::MenuItemType::ACTION);
            lvl->setEnabled(getCamSlotEnabled(camIdx));
            int ci = camIdx, idx = li;
            lvl->setOnActivate([this, ci, idx](UI::MenuItem*)
            {
                if (getCamSlotEnabled(ci) == false) return;
                APP::appConf.menu_aicam.selected_camIdx = ci;
                m_selectedLevelIndex = idx;
                startCamROIAdjustment();
            });
            cam->addChild(lvl);
        }
        m_safeZoneCamItems[camIdx] = cam;
        parent->addChild(cam);
    }
}

void AppAICam::refreshSafeZoneMenuItems()
{
    for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
    {
        auto item = m_safeZoneCamItems[camIdx].lock();
        if (item == nullptr) continue;

        bool enabled = getCamSlotEnabled(camIdx) && getCamRoiLevelCount(camIdx) > 0;
        item->setEnabled(enabled);

        for (const auto& child : item->getChildren())
            if (child != nullptr) child->setEnabled(enabled);
    }
    if (m_menuNavigator != nullptr) m_menuNavigator->refreshSubMenuContent();
}

// ============================================================================
// ROI Adjustment
// ============================================================================

void AppAICam::onCancelAdjustment()
{
    if (m_adjustmentType == AdjustmentType::CamROI) exitCamROIAdjustment();
}

void AppAICam::exitCamROIAdjustment()
{
    int camIdx = APP::appConf.menu_aicam.selected_camIdx;
    if (camIdx >= 0 && camIdx < AI_CAM_NUM && m_cameraCells[camIdx] != nullptr)
    {
        UI::AIImageView* iv = m_cameraCells[camIdx]->getAIImageView();
        if (iv != nullptr)
        {
            iv->setOnROIPointMoved(nullptr);
            iv->setActiveZoneIndex(-1);
            iv->setDpadSelectedPoint(-1);
        }
    }
    m_roiDragPointIndex = -1;
    m_roiDragActive     = false;
    if (m_menuNavigator != nullptr) m_menuNavigator->setSubMenuAdjustmentMode(false);
}

void AppAICam::startCamROIAdjustment()
{
    int camIdx   = APP::appConf.menu_aicam.selected_camIdx;
    int levelIdx = m_selectedLevelIndex;

    if (camIdx >= 0 && camIdx < AI_CAM_NUM && m_cameraCells[camIdx] != nullptr)
    {
        UI::AIImageView* iv = m_cameraCells[camIdx]->getAIImageView();
        if (iv != nullptr)
        {
            iv->setOnROIPointMoved([this, camIdx, levelIdx](int ptIdx, float nativeX, float nativeY)
            {
                if (ptIdx < 0 || ptIdx >= AI_CAM_ROI_PTS_NUM) return;
                APP::appConf.menu_aicam.roi_pts[camIdx][levelIdx][ptIdx].x = static_cast<int>(nativeX);
                APP::appConf.menu_aicam.roi_pts[camIdx][levelIdx][ptIdx].y = static_cast<int>(nativeY);
                m_cameraCells[camIdx]->getAIImageView()->clearROIZones();
                for (int lvIdx = 0; lvIdx < getCamRoiLevelCount(camIdx); lvIdx++)
                    m_cameraCells[camIdx]->updateROI(APP::appConf.menu_aicam.roi_pts[camIdx][lvIdx], lvIdx);
            });
            iv->setActiveZoneIndex(levelIdx);
            iv->setDpadSelectedPoint(0);
        }
    }

    startAdjustment(AdjustmentType::CamROI, AI_CAM_ROI_PTS_NUM);
    m_roiDragPointIndex = 0;
    m_roiDragActive     = false;

    if (m_menuNavigator != nullptr)
    {
        m_menuNavigator->setSubMenuAdjustmentMode(true);
        m_menuNavigator->setSubMenuOnAdjustmentConfirm([this]()
        {
            int camIdx = APP::appConf.menu_aicam.selected_camIdx;
            confirmAdjustment();
            if (m_adjustmentType == AdjustmentType::CamROI)
            {
                m_roiDragPointIndex = m_adjustmentStepIndex;
                if (camIdx >= 0 && camIdx < AI_CAM_NUM && m_cameraCells[camIdx] != nullptr)
                {
                    UI::AIImageView* iv = m_cameraCells[camIdx]->getAIImageView();
                    if (iv != nullptr) iv->setDpadSelectedPoint(m_roiDragPointIndex);
                }
            }
            else
            {
                exitCamROIAdjustment();
            }
        });
    }
}

} // namespace APP
