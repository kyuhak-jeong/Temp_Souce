#ifndef APP_SVM_H
#define APP_SVM_H

#include "app_shell.h"

namespace APP
{

class AppSvm : public AppShell
{
public:
    AppSvm();
    ~AppSvm() override;

    // Svm Framebuffer
    void bindSvmFb();
    void unbindSvmFb();
    void pushSvmFbTexture();

    // Calibration transition
    void goToCalibration();

    // Animator & Interaction
    void        setSvmAnimator(ISvmAnimator* animator) { m_svmAnimator = animator; }
    bool        isInteractiveMode() const              { return m_interactiveMode; }
    void        setInteractiveMode(bool enable);
    void        setLayout1Rect(int x, int y, int width, int height);
    bool        isLayout1Hit(float screenX, float screenY) const;
    CameraInput takeCameraInput();
    bool        isV3dAdjustmentActive() const          { return m_adjustmentType == AdjustmentType::CamViewV3D; }

protected:
    // AppShell overrides
    bool onInitialize ()                                            override;
    void onCleanup    ()                                            override;
    void onUpdate     (float deltaTime)                             override;
    void onRender     ()                                            override;
    bool onKeyEvent   (const IO::KeyEvent& event)                   override;
    bool onMotionEvent(const IO::MotionEvent& event)                override;
    void stepEntered  (int step)                                    override;

    void setupMainScreenContent   ()                                override;
    void buildExtraMenuItems      ()                                override;
    void buildExtraSystemMenuItems(std::shared_ptr<UI::MenuItem> p) override;
    void onConfigReverted         ()                                override;
    void syncMenuFromConfig       ()                                override;

private:
    // Menu tree builders
    void buildViewMenuItems      (std::shared_ptr<UI::MenuItem> parent);
    void buildActivationMenuItems(std::shared_ptr<UI::MenuItem> parent);

    // CamView adjustment
    void startAdjustCamViewR2D(int camIdx);
    void startAdjustCamViewV3D(int camIdx);
    void applyAdjustCamViewR2D(bool left, bool right, bool up, bool down);

    // SVM preview
    void updateSvmPreview();

    // Config helpers
    void loadSvmConfigValues();

    // Gesture helpers
    void  accumulateOrbit(float dAzimPx, float dElevPx);
    void  accumulateZoom (float spanDeltaPx);
    float touchSpan      (const IO::MotionEvent& event) const;

    // Shared orbit/zoom gesture handler for both main screen and V3D preview.
    using HitTestFn = std::function<bool(float x, float y)>;
    bool handleOrbitZoomGesture(const IO::MotionEvent& event,
                                const HitTestFn& hitTest,
                                bool&     pointerDown,
                                bool&     twoFingerActive,
                                float&    prevSpan,
                                UI::Vec2& lastPointer);

    bool handleAdjV3DMotion(const IO::MotionEvent& event);

    // Views
    UI::ImageView*  m_svmImageView = nullptr;
    UI::Framebuffer m_svmFb;

    // SvmAnimator bridge (set by svm_ui_process, not owned)
    ISvmAnimator* m_svmAnimator = nullptr;

    // Interactive gesture state (main screen)
    bool        m_interactiveMode  = false;
    CameraInput m_cameraInput;
    UI::RectF   m_layout1FbRect;
    bool        m_pointerDown      = false;
    bool        m_twoFingerActive  = false;
    float       m_prevSpan         = 0.0f;
    UI::Vec2    m_lastPointer      = { 0.0f, 0.0f };
    int64_t     m_lastTapTime      = 0;
    UI::Vec2    m_lastTapPos       = { 0.0f, 0.0f };

    // Interactive gesture state (V3D adjustment preview)
    int      m_v3dAdjCamIdx       = 0;
    bool     m_adjPointerDown     = false;
    bool     m_adjTwoFingerActive = false;
    float    m_adjPrevSpan        = 0.0f;
    UI::Vec2 m_adjLastPointer     = { 0.0f, 0.0f };

    // Constants
    static constexpr float   kMouseScrollZoomScale = 3.0f;
    static constexpr float   kLongPressThreshold   = 0.4f;           // seconds
    static constexpr float   kOrbitStep            = 2.0f;           // deg per key event
    static constexpr float   kZoomStep             = 0.2f;           // metres per key event
    static constexpr float   kDoubleTapMaxSec       = 0.2f;           // 200 ms in sec
    static constexpr float   kDoubleTapMaxDist2    = 20.0f * 20.0f;  // squared px threshold
};

} // namespace APP

#endif // APP_SVM_H