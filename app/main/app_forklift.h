#ifndef APP_FORKLIFT_H
#define APP_FORKLIFT_H

#include "app_aicam.h"

namespace APP
{

// ── ForkliftCameraConfig ──────────────────────────────────────────────────────

struct ForkliftCameraConfig
{
    UI::GridCellSlot  slot;  // slot.enabled is the single enable/disable flag; also carries grid position
    AICamConfig       roi;
};

// ── AppForklift ───────────────────────────────────────────────────────────────

class AppForklift : public AppAICam
{
public:
    AppForklift();
    ~AppForklift() override;

    // ── Camera config table ───────────────────────────────────────────────────
    // Layout (2 cols, 2 rows, rowWeights 1:2):
    //   col 0        col 1
    //  ┌──────────┬──────────┐  row 0  (weight 1)
    //  │  Left(3) │ Right(1) │
    //  ├──────────┴──────────┤  row 1  (weight 2)
    //  │      Rear (2)       │  colSpan = 2
    //  └─────────────────────┘
    static constexpr std::array<ForkliftCameraConfig, AI_CAM_NUM> s_cameraConfigs = {{
        //  slot{enabled,row,col,rSpan,cSpan}  roi{roiLevels, label,   roiLabels}
        { { false, 1, 0, 1, 1 },  { 1, "Front", { "0.6m", ""    } } },
        { { true,  0, 1, 1, 1 },  { 1, "Right", { "0.6m", ""    } } },
        { { true,  1, 0, 2, 2 },  { 2, "Rear",  { "3m",   "5m"  } } },
        { { true,  0, 0, 1, 1 },  { 1, "Left",  { "0.6m", ""    } } },

        // { { true,  1, 0, 1, 1 },  { 1, "Front", { "0.6m", ""   } } },
        // { { true,  0, 1, 1, 1 },  { 1, "Right", { "0.6m", ""   } } },
        // { { true,  1, 1, 1, 1 },  { 2, "Rear",  { "3m",   "5m" } } },
        // { { true,  0, 0, 1, 1 },  { 1, "Left",  { "0.6m", ""   } } },
    }};

protected:
    // ── AppBase status bar hook ───────────────────────────────────────────
    void onStatusBarCreated() override;

    // ── AppAICam hooks ────────────────────────────────────────────────────
    void setupMainScreenContent()                                    override;
    void onEnterCameraFullscreen(int camIdx)                         override;
    void onExitCameraFullscreen()                                    override;
    void onUpdate(float deltaTime)                                   override;
    void onCameraTexturesUpdated()                                   override;

    bool        getCamSlotEnabled(int camIdx)                  const override;
    int         getCamRoiLevelCount(int camIdx)                const override;
    const char* getCamLabel(int camIdx)                        const override;
    const char* getCamRoiLevelLabel(int camIdx, int levelIdx)  const override;

private:
    UI::CustomGridView* m_cameraGrid;
};

} // namespace APP

#endif // APP_FORKLIFT_H