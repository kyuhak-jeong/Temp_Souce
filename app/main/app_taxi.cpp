/*==========================================================================
 * app_taxi.cpp
 *==========================================================================*/
#include "app_taxi.h"
#include "app_vars.h"
#include "logger.h"
#include "ui_locale.h"
#include <ctime>

namespace APP
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

AppTaxi::AppTaxi()
    : AppAICam()
    , m_pipView(nullptr)
{
}

AppTaxi::~AppTaxi() {}

// ============================================================================
// Camera config accessors
// ============================================================================

bool        AppTaxi::getCamSlotEnabled(int camIdx)   const
{
    if (camIdx < 0 || camIdx >= AI_CAM_NUM) return false;
    return s_cameraConfigs[camIdx].roiLevelCount > 0
        && APP::appConf.menu_taxi.cam_enabled[camIdx];
}
int         AppTaxi::getCamRoiLevelCount(int camIdx) const { return (camIdx < 0 || camIdx >= AI_CAM_NUM) ? 0 : s_cameraConfigs[camIdx].roiLevelCount; }
const char* AppTaxi::getCamLabel(int camIdx)         const { return (camIdx < 0 || camIdx >= AI_CAM_NUM) ? "Cam?" : s_cameraConfigs[camIdx].label; }

const char* AppTaxi::getCamRoiLevelLabel(int camIdx, int levelIdx) const
{
    if (camIdx < 0 || camIdx >= AI_CAM_NUM)                   return "?m";
    if (levelIdx < 0 || levelIdx >= AI_CAM_ROI_LEVEL_MAX_NUM) return "?m";
    if (levelIdx >= s_cameraConfigs[camIdx].roiLevelCount)    return "?m";
    return s_cameraConfigs[camIdx].roiLevelLabels[levelIdx];
}

// ============================================================================
// onStatusBarCreated  — add taxi-specific status slots to the bar
// ============================================================================

void AppTaxi::onStatusBarCreated()
{
    AppShell::onStatusBarCreated();
}

// ============================================================================
// Config sync helpers
// ============================================================================
void AppTaxi::loadPiPFromConfig()
{
    rebuildPiPCells();
    syncPrimaryRowItems();
    refreshSafeZoneMenuItems();
}

void AppTaxi::onConfigReverted()
{
    loadPiPFromConfig();
    LOG_APP_INFO("AppTaxi: PiP config reverted");
}

void AppTaxi::syncMenuFromConfig()
{
    AppAICam::syncMenuFromConfig();   // base handles system/DVR/etc.

    const M_MENU_TAXI& conf = APP::appConf.menu_taxi;

    // Re-sync cam_enabled checkboxes
    {
        UI::MenuItem* row = m_menuRoot ? m_menuRoot->find("pip_cameras") : nullptr;
        if (row != nullptr)
        {
            int mask = 0;
            for (int i = 0; i < AI_CAM_NUM; i++)
                if (conf.cam_enabled[i]) mask |= (1 << i);
            row->setCheckedMask(mask);
        }
    }

    // Re-sync primary camera selection and dock/orientation
    {
        UI::MenuItem* row = m_menuRoot ? m_menuRoot->find("pip_primary") : nullptr;
        if (row != nullptr) row->setSelectedChildIndex(conf.primary_cam_idx);
    }
    {
        UI::MenuItem* row = m_menuRoot ? m_menuRoot->find("pip_dock") : nullptr;
        if (row != nullptr) row->setSelectedChildIndex(conf.pip_dock);
    }
    {
        UI::MenuItem* row = m_menuRoot ? m_menuRoot->find("pip_orient") : nullptr;
        if (row != nullptr) row->setSelectedChildIndex(conf.pip_orientation);
    }

    // Re-sync enabled state on primary children and SafeZone items
    syncPrimaryRowItems();
    refreshSafeZoneMenuItems();
}

// ============================================================================
// Main screen setup
// ============================================================================

void AppTaxi::setupMainScreenContent()
{
    LOG_APP_INFO("AppTaxi: Setting up main screen content");
    UI::TextureManager& texMgr = UI::TextureManager::getInstance();

    // ── PiP view ──────────────────────────────────────────────────────────────
    m_pipView = new UI::PiPView();
    m_pipView->getLayoutParams().width  = UI::MATCH_PARENT;
    m_pipView->getLayoutParams().height = UI::MATCH_PARENT;

    for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
    {
        AICameraCell* cell = new AICameraCell(camIdx);
        cell->setFocusable(false);

        const std::string locKey = std::string(s_cameraConfigs[camIdx].label);
        if (cell->getNameText() != nullptr) SET_LOCALIZED_TEXT(cell->getNameText(), locKey.c_str());

        cell->setBackgroundColor(UI::Color::Transparent);
        cell->setHoverColor(UI::Color::Transparent);
        cell->setFocusedColor(UI::Color::Transparent);
        cell->setPressedColor(UI::Color::CardBackgroundHover);
        cell->setCornerRadius(12.0f);

        UI::Texture* bgTex = texMgr.getTextureByName("03_bg_cam");
        if (bgTex != nullptr) cell->setImage(bgTex);

        cell->setOnClickListener([this, camIdx](UI::View*) { setPrimaryCamera(camIdx); });

        m_cameraCells[camIdx] = cell;
    }

    rebuildPiPCells();
    m_mainScreen->addView(m_pipView);

    initializeFocusSystem();
    LOG_APP_SUCCESS("AppTaxi: Main screen content setup complete");
}

// ============================================================================
// Extra menu items — PiP Setup
// ============================================================================

void AppTaxi::buildExtraMenuItems()
{
    AppAICam::buildExtraMenuItems();    // adds SafeZone first
    loadPiPFromConfig();

    UI::TextureManager& texMgr = UI::TextureManager::getInstance();
    auto pipMenu = std::make_shared<UI::MenuItem>("pip_setup", TR("PiP_Setup"), UI::MenuItemType::CATEGORY);
    SET_LOCALIZED_MENU_ITEM(pipMenu, "PiP_Setup");
    UI::Texture* icn = texMgr.getTextureByName("04_icn_pip");
    if (icn != nullptr) pipMenu->setIconTexture(icn);
    pipMenu->setShowPreview(false);
    buildPiPMenuItems(pipMenu);
    m_menuRoot->addChild(pipMenu);
}

void AppTaxi::buildPiPMenuItems(std::shared_ptr<UI::MenuItem> parent)
{
    // ── Active cameras (CHECKBOX) ─────────────────────────────────────────
    {
        auto row = std::make_shared<UI::MenuItem>("pip_cameras", TR("PiP_Cameras"), UI::MenuItemType::CHECKBOX);
        SET_LOCALIZED_MENU_ITEM(row, "PiP_Cameras");
        row->setShowPreview(false);

        int mask = 0;
        for (int i = 0; i < AI_CAM_NUM; i++)
            if (APP::appConf.menu_taxi.cam_enabled[i] == true) mask |= (1 << i);
        row->setCheckedMask(mask);

        for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
        {
            const std::string locKey = std::string(s_cameraConfigs[camIdx].label);
            auto child = std::make_shared<UI::MenuItem>(
                "cam_en_" + std::to_string(camIdx), TR(locKey.c_str()), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(child, locKey.c_str());
            row->addChild(child);
        }

        row->setOnValueChanged([this, row](UI::MenuItem*)
        {
            int newMask = row->getValueInt();
            if (newMask == 0) { newMask = 1; row->setCheckedMask(newMask); }
            for (int i = 0; i < AI_CAM_NUM; i++)
                APP::appConf.menu_taxi.cam_enabled[i] = ((newMask & (1 << i)) != 0);
            rebuildPiPCells();
            syncPrimaryRowItems();
            refreshSafeZoneMenuItems();
        });
        parent->addChild(row);
    }

    // ── Primary camera (CATEGORY) ─────────────────────────────────────────
    {
        auto row = std::make_shared<UI::MenuItem>("pip_primary", TR("PiP_Primary"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "PiP_Primary");
        row->setShowPreview(false);

        // primary_cam_idx is the raw camIdx of the primary camera
        row->setSelectedChildIndex(APP::appConf.menu_taxi.primary_cam_idx);

        for (int camIdx = 0; camIdx < AI_CAM_NUM; camIdx++)
        {
            const std::string locKey = std::string(s_cameraConfigs[camIdx].label);
            auto child = std::make_shared<UI::MenuItem>(
                "pip_primary_" + std::to_string(camIdx), TR(locKey.c_str()), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(child, locKey.c_str());
            child->setEnabled(APP::appConf.menu_taxi.cam_enabled[camIdx]);
            row->addChild(child);
        }

        row->setOnValueChanged([this, row](UI::MenuItem*)
        {
            // Store the raw camIdx directly
            APP::appConf.menu_taxi.primary_cam_idx = row->getSelectedChildIndex();
            applyPiPSettings();
        });
        m_primaryRowItem = row;
        parent->addChild(row);
    }

    // ── Dock position (CATEGORY) ──────────────────────────────────────────
    {
        auto row = std::make_shared<UI::MenuItem>("pip_dock", TR("PiP_Dock"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "PiP_Dock");
        row->setShowPreview(false);
        row->setSelectedChildIndex(APP::appConf.menu_taxi.pip_dock);
        row->setOnValueChanged([this, row](UI::MenuItem*)
        {
            APP::appConf.menu_taxi.pip_dock = row->getSelectedChildIndex();
            applyPiPSettings();
        });

        struct DockEntry { const char* id; const char* locKey; };
        static constexpr DockEntry docks[] = {
            { "dock_tl", "PiP_TopLeft"  },
            { "dock_tr", "PiP_TopRight" },
            { "dock_bl", "PiP_BotLeft"  },
            { "dock_br", "PiP_BotRight" },
        };
        for (const auto& d : docks)
        {
            auto item = std::make_shared<UI::MenuItem>(d.id, TR(d.locKey), UI::MenuItemType::ACTION);
            SET_LOCALIZED_MENU_ITEM(item, d.locKey);
            row->addChild(item);
        }
        parent->addChild(row);
    }

    // ── Strip orientation (CATEGORY) ──────────────────────────────────────
    {
        auto row = std::make_shared<UI::MenuItem>("pip_orient", TR("PiP_Orientation"), UI::MenuItemType::CATEGORY);
        SET_LOCALIZED_MENU_ITEM(row, "PiP_Orientation");
        row->setShowPreview(false);
        row->setSelectedChildIndex(APP::appConf.menu_taxi.pip_orientation);
        row->setOnValueChanged([this, row](UI::MenuItem*)
        {
            APP::appConf.menu_taxi.pip_orientation = row->getSelectedChildIndex();
            applyPiPSettings();
        });

        auto hItem = std::make_shared<UI::MenuItem>("orient_h", TR("PiP_Horizontal"), UI::MenuItemType::ACTION);
        auto vItem = std::make_shared<UI::MenuItem>("orient_v", TR("PiP_Vertical"),   UI::MenuItemType::ACTION);
        SET_LOCALIZED_MENU_ITEM(hItem, "PiP_Horizontal");
        SET_LOCALIZED_MENU_ITEM(vItem, "PiP_Vertical");
        row->addChild(hItem);
        row->addChild(vItem);
        parent->addChild(row);
    }
}

// ============================================================================
// PiP helpers
// ============================================================================

void AppTaxi::rebuildPiPCells()
{
    if (m_pipView == nullptr) return;

    M_MENU_TAXI& conf = APP::appConf.menu_taxi;

    // Ensure primary camIdx is enabled; fall back to first enabled camera
    if (conf.primary_cam_idx < 0 || conf.primary_cam_idx >= AI_CAM_NUM
        || conf.cam_enabled[conf.primary_cam_idx] == false)
    {
        conf.primary_cam_idx = 0;
        for (int i = 0; i < AI_CAM_NUM; i++)
            if (conf.cam_enabled[i] == true) { conf.primary_cam_idx = i; break; }
    }

    m_pipView->clearCells();
    for (int i = 0; i < AI_CAM_NUM; i++)
        if (conf.cam_enabled[i] == true && m_cameraCells[i] != nullptr)
            m_pipView->addCell(m_cameraCells[i]);

    applyPiPSettings();
}

void AppTaxi::syncPrimaryRowItems()
{
    auto row = m_primaryRowItem.lock();
    if (row == nullptr) return;

    const M_MENU_TAXI& conf = APP::appConf.menu_taxi;
    const auto& children = row->getChildren();
    for (int camIdx = 0; camIdx < AI_CAM_NUM && camIdx < (int)children.size(); camIdx++)
        children[camIdx]->setEnabled(conf.cam_enabled[camIdx]);

    // primary_cam_idx is raw camIdx — ensure it is still enabled, else fall back
    int sel = conf.primary_cam_idx;
    if (sel < 0 || sel >= AI_CAM_NUM || conf.cam_enabled[sel] == false)
    {
        sel = 0;
        for (int i = 0; i < AI_CAM_NUM; i++)
            if (conf.cam_enabled[i] == true) { sel = i; break; }
        APP::appConf.menu_taxi.primary_cam_idx = sel;
    }
    row->setSelectedChildIndex(sel);
    // Force the submenu to redraw pill states immediately
    if (m_menuNavigator != nullptr) m_menuNavigator->refreshSubMenuContent();
}

void AppTaxi::applyPiPSettings()
{
    if (m_pipView == nullptr) return;
    const M_MENU_TAXI& conf = APP::appConf.menu_taxi;

    // primary_cam_idx is a raw camIdx; PiPView needs the position within the enabled list
    int pipPos = 0;
    for (int i = 0; i < conf.primary_cam_idx && i < AI_CAM_NUM; i++)
        if (conf.cam_enabled[i] == true) pipPos++;

    m_pipView->setPrimaryIndex(pipPos);
    m_pipView->setDock(static_cast<UI::PiPDock>(conf.pip_dock));
    m_pipView->setOrientation(static_cast<UI::Orientation>(conf.pip_orientation));
    m_pipView->rebuildLayout(m_screenWidth, m_screenHeight);
}

void AppTaxi::setPrimaryCamera(int camIdx)
{
    if (camIdx < 0 || camIdx >= AI_CAM_NUM)                          return;
    if (APP::appConf.menu_taxi.cam_enabled[camIdx] == false)         return;
    if (APP::appConf.menu_taxi.primary_cam_idx == camIdx)            return;

    APP::appConf.menu_taxi.primary_cam_idx = camIdx;
    rebuildPiPCells();
    syncPrimaryRowItems();
    LOG_APP_INFOF("AppTaxi: Primary camera switched to %d (%s)", camIdx, getCamLabel(camIdx));
}

// ============================================================================
// Update / Texture callbacks
// ============================================================================

void AppTaxi::onUpdate(float deltaTime) { AppAICam::onUpdate(deltaTime); }

void AppTaxi::onCameraTexturesUpdated() { }

} // namespace APP
