#ifndef APP_AICAM_H
#define APP_AICAM_H

#include "app_shell.h"
#include <vector>
#include <string>
#include <array>
#include <functional>

#ifndef USE_RKNN
    #define AICAM_SIM_DETECTIONS 1
#endif

namespace APP
{

// ── AICamConfig ───────────────────────────────────────────────────────────────
// ROI descriptor shared by all AICam-derived apps.

struct AICamConfig
{
    int         roiLevelCount;                              // 0 = no ROI, 1..AI_CAM_ROI_LEVEL_MAX_NUM
    const char* label;                                      // e.g. "Front" — used for localization + menu IDs
    const char* roiLevelLabels[AI_CAM_ROI_LEVEL_MAX_NUM];   // distance label per level
};

// ── AICameraCell ──────────────────────────────────────────────────────────────
// GridCellView subclass hosting a camera feed with ROI/detection overlays.

class AICameraCell : public UI::GridCellView
{
public:
    using ActionCallback = std::function<void(int)>;    // called with camIdx

    explicit AICameraCell(int camIdx);

    UI::Button* addCamAction(UI::Texture* icon, ActionCallback cb);
    int         getCamIndex() const { return m_camIdx; }
    void        updateROI(const std::array<XY, AI_CAM_ROI_PTS_NUM>& roiPoints, int roiLevel);
    // roiLevelsNorm: per-level polygons in the same normalised [0,1] space as the BoundingBoxes.
    int         updateDetections(const std::vector<AI::BoundingBox>&        detections,
                                 const std::vector<std::vector<UI::Vec2>>& roiLevelsNorm);
    void        setWarningState(bool warning);

private:
    int m_camIdx;

    static constexpr uint8_t m_roiLineAlpha = 100;
    static constexpr uint8_t m_roiFillAlpha =  40;
    std::array<UI::Color4, AI_CAM_ROI_LEVEL_MAX_NUM>     m_roiLineColors;
    std::array<UI::Color4, AI_CAM_ROI_LEVEL_MAX_NUM + 1> m_roiFillColors;
};

// ── AppAICam ──────────────────────────────────────────────────────────────────
// Adds AI camera cells, ROI adjustment, detection overlays, and camera
// fullscreen on top of AppShell.
//
// Derived classes additionally implement:
//   getCamSlotEnabled()       — whether a camera slot is active
//   getCamRoiLevelCount()     — number of ROI levels for a camera (0 = no ROI)
//   getCamLabel()             — display/localization label for a camera
//   getCamRoiLevelLabel()     — label string for a given camera / level index
//   onEnterCameraFullscreen() — optional hook when entering fullscreen
//   onExitCameraFullscreen()  — optional hook when exiting fullscreen
//   onCameraTexturesUpdated() — optional hook after camera textures refresh

class AppAICam : public AppShell
{
public:
    AppAICam();
    ~AppAICam() override;

protected:
    // ── AppShell / AppBase lifecycle ───────────────────────────────────────
    bool onInitialize()                                 override;
    void onUpdate(float deltaTime)                      override;
    void onRender()                                     override;
    bool onKeyEvent(const IO::KeyEvent& event)          override;
    bool onMotionEvent(const IO::MotionEvent& event)    override;
    bool onTerminalCommand(const std::string& command)  override;

    // ── AppShell hook overrides ────────────────────────────────────────────
    void buildExtraMenuItems()  override;
    void onCancelAdjustment()   override;

    // ── Hooks for derived classes ──────────────────────────────────────────
    virtual void onCameraTexturesUpdated()             {}
    virtual void onEnterCameraFullscreen(int camIdx)   {}
    virtual void onExitCameraFullscreen()              {}

    // ── Per-camera config queries (derived classes override all four) ──────
    virtual bool        getCamSlotEnabled(int camIdx)                 const { return true; }
    virtual int         getCamRoiLevelCount(int camIdx)               const { return AI_CAM_ROI_LEVEL_MAX_NUM; }
    virtual const char* getCamLabel(int camIdx)                       const
    {
        static const char* defaults[] = { "Cam0", "Cam1", "Cam2", "Cam3" };
        return (camIdx >= 0 && camIdx < AI_CAM_NUM) ? defaults[camIdx] : "Cam?";
    }
    virtual const char* getCamRoiLevelLabel(int camIdx, int levelIdx) const
    {
        static const char* defaults[] = { "Lv0", "Lv1" };
        return (levelIdx >= 0 && levelIdx < AI_CAM_ROI_LEVEL_MAX_NUM) ? defaults[levelIdx] : "Lv?";
    }

    // ── Camera helpers ─────────────────────────────────────────────────────
    void initializeCameraFocusContexts();
    void updateCameraTextures();
    void updateSafeZonePreview();
    void updateCameraCellData();
    void toggleCameraFullscreen(int camIdx);
    void enterCameraFullscreen(int camIdx);
    void exitCameraFullscreen();

    // ── Menu builder ───────────────────────────────────────────────────────
    void buildSafeZoneMenuItems(std::shared_ptr<UI::MenuItem> parent);
    void refreshSafeZoneMenuItems();

    // ── ROI adjustment ─────────────────────────────────────────────────────
    void startCamROIAdjustment();
    void exitCamROIAdjustment();

    // ── Members ────────────────────────────────────────────────────────────
    std::array<AICameraCell*, AI_CAM_NUM>  m_cameraCells;
    std::array<UI::Texture*,  AI_CAM_NUM>  m_currentCameraTexture;

    std::shared_ptr<UI::FocusContext> m_fullscreenContext;

    // SafeZone menu item handles — indexed by camIdx, valid after buildSafeZoneMenuItems
    std::array<std::weak_ptr<UI::MenuItem>, AI_CAM_NUM> m_safeZoneCamItems;

    int  m_selectedLevelIndex;
    int  m_roiDragPointIndex;
    bool m_roiDragActive;
    int  m_fullscreenCamIndex;

    // ── Detection simulation ───────────────────────────────────────────────
    #ifdef AICAM_SIM_DETECTIONS
        static constexpr int SIM_OBJ_PER_CAM = 2;
        void updateSimDetections(float deltaTime);
        std::array<std::array<UI::FloatingObject, SIM_OBJ_PER_CAM>, AI_CAM_NUM> m_simObjects;
        bool m_simInitialized = false;
    #endif
};

} // namespace APP

#endif // APP_AICAM_H