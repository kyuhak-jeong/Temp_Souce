#ifndef SVMSCENEANIMATOR_HPP_
#define SVMSCENEANIMATOR_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"

#if defined(_WIN32) || defined(_WIN64)
    #pragma warning(disable: 4819)
#endif

class sanSceneAnimator : public ISvmAnimator
{
public:
    struct Settings
    {
        float orbitAzimuthSensitivity   = 0.4f;  // deg/px, horizontal drag
        float orbitElevationSensitivity = 0.4f;  // deg/px, vertical drag
        float zoomSensitivity           = 0.02f; // m/px,   pinch spread
        float minRadius                 = 1.0f;  // m
        float maxRadius                 = 30.0f; // m
        float minElevation              = 2.0f;  // deg - prevents ground clip
        float maxElevation              = 88.0f; // deg - prevents gimbal flip
    };

    explicit sanSceneAnimator(sanXML* pxml);
    ~sanSceneAnimator() override;

    void      initialize();
    glm::mat4 get_view_matrix_of_virual_camera(VIEWMODE viewMode);

    // ── ISvmAnimator ──────────────────────────────────────────────────────
    void pushCameraInput   (const CameraInput& input)                   override;
    void getCurrentPose    (glm::vec3& outPos, glm::vec3& outOri) const override;
    void resetToDefaultPose()                                           override;
    void setInteractive    (bool enable)                                override;
    void setActiveViewMode (int viewModeInt)                            override;
    void nudgeRadius       (float metres)                               override;
    bool isInteractive     ()                                     const override;

    // ── Configuration ─────────────────────────────────────────────────────
    void setSettings      (const Settings& s) { m_settings       = s;     }
    void setAnimationSteps(int steps)         { m_animationSteps = steps; }

    // ── State queries ──────────────────────────────────────────────────────
    bool     isAnimating()       const { return m_isAnimating;    }
    VIEWMODE getActiveViewMode() const { return m_activeViewMode; }

    // ── Public data (accessed by svm_ui_process) ──────────────────────────
    sanXML*  m_pxml         = nullptr;
    VIEWMODE m_prevViewMode = CAMVIEW3D_FRONT;
    bool     m_isAnimating  = false;

private:
    static int camIdx            (VIEWMODE mode);
    int        defaultVcamIdx    ()                             const;
    glm::vec3  samplePos         (VIEWMODE targetMode)          const;
    glm::quat  sampleTilt        (VIEWMODE targetMode)          const;
    float      arcAnimT          (const glm::vec3& fromPos,
                                  const glm::vec3& toPos,
                                  VIEWMODE         xmlFromMode) const;
    glm::mat4  animateTransition (VIEWMODE viewMode);
    glm::mat4  animateReset      ();
    glm::mat4  buildInteractiveVM()                             const;
    void       snapToDefaultPose ();
    void       applyOrbit        (float dAzimDeg, float dElevDeg);
    void       applyZoom         (float spanDeltaPx);

    bool      m_interactive    = false;
    VIEWMODE  m_activeViewMode = CAMVIEW3D_FRONT;
    Settings  m_settings;
    float     m_radius         = 5.0f;
    float     m_azimuth        = 0.0f;
    float     m_elevation      = 30.0f;
    glm::quat m_tilt           = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    int       m_animationSteps = 20;
    float     m_animT          = 1.0f;
    glm::vec3 m_prevPos        = glm::vec3(0.0f);
    glm::vec3 m_prevOri        = glm::vec3(0.0f);

    bool      m_resetAnimating = false;
    glm::quat m_resetQFrom     = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 m_resetTargetPos = glm::vec3(0.0f);
    glm::vec3 m_resetTargetOri = glm::vec3(0.0f);
};

#endif // SVMSCENEANIMATOR_HPP_
