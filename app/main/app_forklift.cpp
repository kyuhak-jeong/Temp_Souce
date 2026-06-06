#include "app_forklift.h"
#include "app_vars.h"
#include "logger.h"
#include "ui_locale.h"
#include <ctime>
#include <cstdio>

namespace APP
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

AppForklift::AppForklift()
    : AppAICam()
    , m_cameraGrid(nullptr)
{}

AppForklift::~AppForklift() {}

// ============================================================================
// Camera config accessors
// ============================================================================

bool        AppForklift::getCamSlotEnabled(int camIdx)   const { return (camIdx < 0 || camIdx >= AI_CAM_NUM) ? false : s_cameraConfigs[camIdx].slot.enabled; }
int         AppForklift::getCamRoiLevelCount(int camIdx) const { return (camIdx < 0 || camIdx >= AI_CAM_NUM) ? 0     : s_cameraConfigs[camIdx].roi.roiLevelCount; }
const char* AppForklift::getCamLabel(int camIdx)         const { return (camIdx < 0 || camIdx >= AI_CAM_NUM) ? "Cam?": s_cameraConfigs[camIdx].roi.label; }

const char* AppForklift::getCamRoiLevelLabel(int camIdx, int levelIdx) const
{
    if (camIdx   < 0 || camIdx   >= AI_CAM_NUM)                    return "?m";
    if (levelIdx < 0 || levelIdx >= AI_CAM_ROI_LEVEL_MAX_NUM)      return "?m";
    if (levelIdx >= s_cameraConfigs[camIdx].roi.roiLevelCount)     return "?m";
    return s_cameraConfigs[camIdx].roi.roiLevelLabels[levelIdx];
}

// ============================================================================
// onStatusBarCreated  — add forklift-specific status slots to the bar
// ============================================================================

void AppForklift::onStatusBarCreated()
{
    AppShell::onStatusBarCreated();
}

// ============================================================================
// setupMainScreenContent
// ============================================================================

void AppForklift::setupMainScreenContent()
{
    LOG_APP_INFO("AppForklift: Setting up main screen content");
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();
    UI::Texture* fsIcon = texMgr.getTextureByName("08_icn_fullscreen");

    // ── Camera grid ────────────────────────────────────────────────────────
    m_cameraGrid = new UI::CustomGridView();
    m_cameraGrid->getLayoutParams().width   = UI::MATCH_PARENT;
    m_cameraGrid->getLayoutParams().height  = UI::MATCH_PARENT;
    m_cameraGrid->getLayoutParams().gravity = UI::Gravity::TOP | UI::Gravity::CENTER_HORIZONTAL;
    m_cameraGrid->setGap(8.0f);

    std::vector<UI::GridCellSlot> slots(AI_CAM_NUM);
    for (int i = 0; i < AI_CAM_NUM; i++) slots[i] = s_cameraConfigs[i].slot;
    m_cameraGrid->setSlots(slots);
    m_mainScreen->addView(m_cameraGrid);

    for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
    {
        if (s_cameraConfigs[camIdx].slot.enabled == false) { m_cameraCells[camIdx] = nullptr; continue; }

        AICameraCell* cell = new AICameraCell(camIdx);
        cell->setFocusable(true);

        std::string camLocKey = std::string(s_cameraConfigs[camIdx].roi.label);
        if (cell->getNameText() != nullptr) SET_LOCALIZED_TEXT(cell->getNameText(), camLocKey.c_str());

        cell->setBackgroundColor(UI::Color::Transparent);
        cell->setHoverColor(UI::Color::Transparent);
        cell->setFocusedColor(UI::Color::Transparent);
        cell->setPressedColor(UI::Color::CardBackgroundHover);
        cell->setCornerRadius(16.0f);

        UI::Texture* bgTex = texMgr.getTextureByName("03_bg_cam");
        if (bgTex != nullptr) cell->setImage(bgTex);

        if (cell->getAIImageView() != nullptr)
        {
            cell->getAIImageView()->setCornerRadius(16.0f);
            if (cell->getAIImageView()->getImageView() != nullptr)
                cell->getAIImageView()->getImageView()->setCornerRadius(16.0f);
        }

        cell->addCamAction(fsIcon, [this](int idx) { toggleCameraFullscreen(idx); });
        cell->getAIImageView()->setFramebufferEnabled(true);
        m_cameraCells[camIdx] = cell;
        m_cameraGrid->addCell(camIdx, cell);
    }
    m_cameraGrid->rebuildLayout(m_screenWidth, m_screenHeight);

    initializeFocusSystem();
    LOG_APP_SUCCESS("AppForklift: Main screen content setup complete");
}

void AppForklift::onEnterCameraFullscreen(int camIdx)
{
    m_cameraGrid->enterFullscreen(camIdx);
    if (m_statusBar != nullptr) m_statusBar->setVisibility(UI::Visibility::GONE);
    if (m_mainScreen != nullptr) m_mainScreen->getLayoutParams().setPadding(0.0f, 0.0f, 0.0f, 0.0f);
}

void AppForklift::onExitCameraFullscreen()
{
    m_cameraGrid->exitFullscreen();
    if (m_statusBar != nullptr) m_statusBar->setVisibility(UI::Visibility::VISIBLE);
    if (m_mainScreen != nullptr) m_mainScreen->getLayoutParams().setPadding(0.0f, 0.0f, 0.0f, static_cast<float>(StatusBar::HEIGHT));
}

// ============================================================================
// Update / Camera texture callback
// ============================================================================

void AppForklift::onUpdate(float deltaTime) { AppAICam::onUpdate(deltaTime); }

void AppForklift::onCameraTexturesUpdated() { }

} // namespace APP
