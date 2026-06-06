#ifndef APP_CALIB_H
#define APP_CALIB_H

#include "app_shell.h"
#include "ui3d_core.h"
#include <map>
#include <memory>
#include <set>

namespace APP
{

// ── AppCalib ──────────────────────────────────────────────────────────────────

class AppCalib : public AppShell
{
public:
    AppCalib();
    ~AppCalib() override;

    // ── Framebuffer ───────────────────────────────────────────────────────────
    void         bindCalibFb();
    void         unbindCalibFb();
    unsigned int getCalibFbTextureId() const;

    // ── Stream pipeline ───────────────────────────────────────────────────────
    bool               isStreamPipelineUpdated() const { return m_streamPipelineUpdated; }
    bool               isStreamPipelineRunning() const { return m_streamPipelineRunning; }
    const std::string& getStreamPipeline()       const { return m_streamPipeline; }
    void               markStreamPipelineApplied()     { m_streamPipelineUpdated = false; }
    void               setStreamPipelineRunning(bool v){ m_streamPipelineRunning = v; }

    // ── Capture ───────────────────────────────────────────────────────────────
    void setCapturePanelTexture(unsigned int id);
    void setCaptureContextRefresh() { m_calibNeedRefreshContext = true; syncStreamPipeline(false); }

    // ── Context refresh ───────────────────────────────────────────────────────
    bool needsContextRefresh() const { return m_calibNeedRefreshContext; }
    void clearContextRefresh()       { m_calibNeedRefreshContext = false; }

    // ── Calib menu state — read by calib_ui_process ───────────────────────────
    int  getSelectedCalibViewMode() const;  // -1 if not in auto_calib sub-menu
    int  getSelectedCalibCamIdx()   const;  // selected camera tab in feature row
    bool isCalibAdjMode()           const { return m_adjustmentType == AdjustmentType::CalibFeaturePoint; }

    // ── Pending point position (sentinel {-1,-1} = none) ─────────────────────
    UI::Vec2 getPendingPointPos()  const { return m_pendingPointPos; }
    void     clearPendingPointPos()      { m_pendingPointPos = UI::Vec2{-1.f, -1.f}; }

    // ── Pending cam from direct pill tap ─────────────────────────────────────
    int  getPendingAdjCam() const { return m_pendingAdjCam; }
    void clearPendingAdjCam()     { m_pendingAdjCam = -1; }
    void startAdjustmentForCam(int cam);

    // ── Point confirmation ────────────────────────────────────────────────────
    bool isPointConfirmed()   const { return m_pointConfirmed; }
    void clearPointConfirmed()      { m_pointConfirmed = false; }

    // ── Calib state sync — called each frame by calib_ui_process ─────────────
    void syncCalibState(const CalibState& state);

protected:
    // AppBase lifecycle
    bool onInitialize()                              override;
    void onCleanup()                                 override;
    void onUpdate(float deltaTime)                   override;
    void onRender()                                  override;
    bool onKeyEvent(const IO::KeyEvent& event)       override;
    bool onMotionEvent(const IO::MotionEvent& event) override;

    // AppShell hooks
    void setupMainScreenContent() override {}
    void buildExtraMenuItems()    override;
    void buildShellMenuItems()    override {}
    void onConfigReverted()       override;
    void onConfigSaved()          override;
    void stepEntered(int step)    override;

private:
    // ── Menu builders ─────────────────────────────────────────────────────────
    void buildCalibParamsMenuItems(std::shared_ptr<UI::MenuItem> parent);
    void buildCaptureMenuItems    (std::shared_ptr<UI::MenuItem> parent);
    void buildAutoCalibMenuItems  (std::shared_ptr<UI::MenuItem> parent);

    // ── Vehicle ───────────────────────────────────────────────────────────────
    void loadVehicleData();
    void populateVehicleListItems();
    void syncVehiclePreview(int idx);
    void loadVehicleModel(int idx);
    void applyVehicleSelection(int vehicleIdx);
    void showVehicleConfirmPopup(int vehicleIdx);
    void restoreDefaultForCurrentVehicle();

    // ── Calib params ──────────────────────────────────────────────────────────
    void loadCalibParamsData();
    void saveCalibParamsData();

    // ── Stream / capture ──────────────────────────────────────────────────────
    void startCapturePipeline();
    void backupPreviousCapture();
    void startStreamPipeline(bool live);
    void stopStreamPipeline();
    void syncStreamPipeline(bool stop);

    // ── Calib UI helpers ──────────────────────────────────────────────────────
    void updateCalibPreview();
    void loadCalibConfigValues();
    void refreshStepVisuals(const CalibState& state);

    // ── Vehicle data ──────────────────────────────────────────────────────────
    std::vector<M_MENU_CALIB>  m_calibMenuData;
    std::map<int, std::string> m_vehiclesMap;
    std::set<std::string>      m_uniqueVehicle;
    std::vector<std::string>   m_vehicleTexturePaths;
    std::vector<std::string>   m_vehicleModelPaths;
    int                        m_currentVehicleIdx;
    int                        m_hoveredVehicleIdx;
    float                      m_previewDebounce;
    UI::LinearLayout*          m_vehicleOverlay;
    UI3D::Viewport*            m_vehicleViewport;
    UI3D::Model*               m_vehicleModel;
    UI3D::ModelInstance        m_vehicleInstance;

    // ── Capture ───────────────────────────────────────────────────────────────
    std::unique_ptr<UI::Texture> m_captureOwnedTex;
    unsigned int                 m_captureLeftPanelTexId;
    UI::LinearLayout*            m_streamLabelsOverlay;
    UI::TextView*                m_streamCellLabels[4];

    // ── Calib state ───────────────────────────────────────────────────────────
    bool        m_calibNeedRefreshContext;
    UI::Vec2    m_pendingPointPos;
    int         m_pendingAdjCam;
    bool        m_pointConfirmed;
    CalibState  m_lastSyncedState;

    // ── Feature point touch drag ──────────────────────────────────────────────
    bool        m_featureDragActive;

    // ── Stream pipeline ───────────────────────────────────────────────────────
    std::string m_streamPipeline;
    bool        m_streamPipelineUpdated;
    bool        m_streamPipelineRunning;

    // ── Framebuffer ───────────────────────────────────────────────────────────
    UI::Framebuffer m_calibFb;
};

} // namespace APP

#endif // APP_CALIB_H
