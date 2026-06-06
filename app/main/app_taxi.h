#ifndef APP_TAXI_H
#define APP_TAXI_H

#include "app_aicam.h"

namespace APP
{

// ── AppTaxi ───────────────────────────────────────────────────────────────────

class AppTaxi : public AppAICam
{
public:
    AppTaxi();
    ~AppTaxi() override;

    // ── Camera config table ───────────────────────────────────────────────────
    static constexpr std::array<AICamConfig, AI_CAM_NUM> s_cameraConfigs = {{
        // {roiLevels, label,   roiLabels}
        { 0, "Front", { "2m", ""    } },
        { 0, "Right", { "1m", ""    } },
        { 1, "Rear",  { "2m", "4m"  } },
        { 0, "Left",  { "1m", ""    } },
    }};

protected:
    // ── AppBase status bar hook ───────────────────────────────────────────
    void onStatusBarCreated() override;

    // ── AppAICam interface ────────────────────────────────────────────────
    void setupMainScreenContent()                                    override;
    void buildExtraMenuItems()                                       override;
    void onUpdate(float deltaTime)                                   override;
    void onCameraTexturesUpdated()                                   override;
    void onConfigReverted()                                          override;
    void syncMenuFromConfig()                                        override;

    bool        getCamSlotEnabled(int camIdx)                  const override;
    int         getCamRoiLevelCount(int camIdx)                const override;
    const char* getCamLabel(int camIdx)                        const override;
    const char* getCamRoiLevelLabel(int camIdx, int levelIdx)  const override;

private:
    void buildPiPMenuItems(std::shared_ptr<UI::MenuItem> parent);
    void loadPiPFromConfig();
    void rebuildPiPCells();
    void syncPrimaryRowItems();
    void applyPiPSettings();
    void setPrimaryCamera(int camIdx);

    UI::PiPView*                m_pipView;
    std::weak_ptr<UI::MenuItem> m_primaryRowItem;
};

} // namespace APP

#endif // APP_TAXI_H
