#include "svmSceneAnimator.hpp"
#include "svmCamvec.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

// ── File-scope helpers ────────────────────────────────────────────────────────

namespace
{
    struct SphericalPos { float r, azimuth, elevation; };

    SphericalPos toSpherical(const glm::vec3& p)
    {
        const float r = glm::length(p);
        if (r < 1e-6f) return { 0.0f, 0.0f, 0.0f };
        return { r, std::atan2(p.y, p.x), std::asin(glm::clamp(p.z / r, -1.0f, 1.0f)) };
    }

    glm::vec3 fromSpherical(float r, float az, float el)
    {
        return { r * std::cos(el) * std::cos(az),
                 r * std::cos(el) * std::sin(az),
                 r * std::sin(el) };
    }

    float angleDiff(float a, float b)
    {
        float d = b - a;
        while (d >  (float)M_PI) d -= 2.0f * (float)M_PI;
        while (d < -(float)M_PI) d += 2.0f * (float)M_PI;
        return d;
    }

    glm::mat4 lookAtOrigin(const glm::vec3& pos)
    {
        const glm::vec3 fwd = glm::normalize(-pos);
        const glm::vec3 up  = (std::fabs(fwd.z) < 0.99f) ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
        const glm::vec3 r   = glm::normalize(glm::cross(fwd, up));
        const glm::vec3 u   = glm::cross(r, fwd);

        glm::mat4 vm(0.0f);
        vm[0][0] =  r.x;   vm[1][0] =  r.y;   vm[2][0] =  r.z;
        vm[0][1] =  u.x;   vm[1][1] =  u.y;   vm[2][1] =  u.z;
        vm[0][2] = -fwd.x; vm[1][2] = -fwd.y; vm[2][2] = -fwd.z;
        vm[3][0] = -glm::dot(r,   pos);
        vm[3][1] = -glm::dot(u,   pos);
        vm[3][2] =  glm::dot(fwd, pos);
        vm[3][3] = 1.0f;
        return vm;
    }

    // tilt = q_real * inverse(q_lookAt) - residual rotation beyond pure look-at
    glm::quat tiltOffset(const glm::vec3& pos, const glm::vec3& ori)
    {
        const glm::quat qLookAt = glm::quat_cast(glm::mat3(lookAtOrigin(pos)));
        const glm::quat qReal   = glm::quat_cast(glm::mat3(VM4x4(pos, ori)));
        return qReal * glm::inverse(qLookAt);
    }

    glm::mat4 buildVM(const glm::quat& q, const glm::vec3& pos)
    {
        const glm::mat3 R = glm::mat3_cast(q);
        glm::mat4 vm(0.0f);
        vm[0] = glm::vec4(R[0], 0.0f);
        vm[1] = glm::vec4(R[1], 0.0f);
        vm[2] = glm::vec4(R[2], 0.0f);
        vm[3] = glm::vec4(-(R * pos), 1.0f);
        return vm;
    }

    glm::vec3 safeNormalize(const glm::vec3& v)
    {
        return (glm::length(v) > 1e-6f) ? glm::normalize(v) : glm::vec3(0, 0, 1);
    }

    glm::quat shortArc(const glm::quat& from, const glm::quat& to)
    {
        return (glm::dot(from, to) < 0.0f) ? -to : to;
    }
} // namespace

// ── sanSceneAnimator ──────────────────────────────────────────────────────────

sanSceneAnimator::sanSceneAnimator(sanXML* pxml) : m_pxml(pxml) {}
sanSceneAnimator::~sanSceneAnimator() {}

void sanSceneAnimator::initialize() {}

// ── ISvmAnimator ──────────────────────────────────────────────────────────────

void sanSceneAnimator::setInteractive(bool enable)
{
    if (m_interactive == enable) return;
    m_interactive    = enable;
    m_resetAnimating = false;
    if (m_interactive) snapToDefaultPose();
}

void sanSceneAnimator::setActiveViewMode(int viewModeInt)
{
    const VIEWMODE mode = (VIEWMODE)viewModeInt;
    if (m_activeViewMode == mode) return;
    m_activeViewMode = mode;
    m_resetAnimating = false;
    if (m_interactive) snapToDefaultPose();
}

void sanSceneAnimator::pushCameraInput(const CameraInput& input)
{
    if (!m_interactive || m_resetAnimating) return;
    if (input.resetCamera) { resetToDefaultPose(); return; }
    if (std::fabs(input.orbitAzimuth) > 1e-6f || std::fabs(input.orbitElevation) > 1e-6f)
        applyOrbit(input.orbitAzimuth, input.orbitElevation);
    if (std::fabs(input.zoomDelta) > 1e-6f)
        applyZoom(input.zoomDelta);
}

void sanSceneAnimator::getCurrentPose(glm::vec3& outPos, glm::vec3& outOri) const
{
    const float     azRad = glm::radians(m_azimuth);
    const float     elRad = glm::radians(m_elevation);
    const glm::vec3 pos(m_radius * std::cos(elRad) * std::cos(azRad),
                        m_radius * std::cos(elRad) * std::sin(azRad),
                        m_radius * std::sin(elRad));
    VM4x4toEP(buildVM(m_tilt * glm::quat_cast(glm::mat3(lookAtOrigin(pos))), pos), outPos, outOri);
}

void sanSceneAnimator::resetToDefaultPose()
{
    if (m_pxml == nullptr) return;

    const int idx          = defaultVcamIdx();
    m_resetTargetPos       = m_pxml->m_vcam[idx].pos;
    m_resetTargetOri       = m_pxml->m_vcam[idx].ori;

    if (!m_pxml->m_activation.scene_animation)
    {
        snapToDefaultPose();
        return;
    }

    m_resetQFrom     = glm::quat_cast(glm::mat3(buildInteractiveVM()));
    getCurrentPose(m_prevPos, m_prevOri);
    m_animT          = 0.0f;
    m_resetAnimating = true;
}

void sanSceneAnimator::nudgeRadius(float metres)
{
    m_radius = std::clamp(m_radius + metres, m_settings.minRadius, m_settings.maxRadius);
}

bool sanSceneAnimator::isInteractive() const { return m_interactive; }

// ── View matrix ───────────────────────────────────────────────────────────────

glm::mat4 sanSceneAnimator::get_view_matrix_of_virual_camera(VIEWMODE viewMode)
{
    if (m_interactive && viewMode != TOPVIEW3D)
    {
        if (m_resetAnimating) return animateReset();
        return buildInteractiveVM();
    }

    switch (viewMode)
    {
        case TOPVIEW3D:
            return VM4x4(m_pxml->m_vcam[10].pos, m_pxml->m_vcam[10].ori);

        case CAMVIEW3D_FRONT:
        case CAMVIEW3D_RIGHT:
        case CAMVIEW3D_REAR:
        case CAMVIEW3D_LEFT:
        {
            if (m_prevViewMode != viewMode)
            {
                if (m_isAnimating)
                {
                    const glm::vec3 currPos = samplePos(m_prevViewMode);
                    VM4x4toEP(buildVM(sampleTilt(m_prevViewMode) *
                                      glm::quat_cast(glm::mat3(lookAtOrigin(currPos))), currPos),
                              m_prevPos, m_prevOri);
                }
                else
                {
                    m_prevPos = m_pxml->m_vcam[camIdx(m_prevViewMode)].pos;
                    m_prevOri = m_pxml->m_vcam[camIdx(m_prevViewMode)].ori;
                }

                const glm::vec3& newTarget = m_pxml->m_vcam[camIdx(viewMode)].pos;
                m_animT        = arcAnimT(m_prevPos, newTarget, m_prevViewMode);
                m_isAnimating  = (m_animT < 1.0f);
                m_prevViewMode = viewMode;
            }

            if (m_pxml->m_activation.scene_animation == true &&
                m_pxml->m_scene_animation_parameter.scene_animation_type == 2 &&
                m_isAnimating == true)
                return animateTransition(viewMode);

            return VM4x4(m_pxml->m_vcam[camIdx(viewMode)].pos,
                         m_pxml->m_vcam[camIdx(viewMode)].ori);
        }

        case CAMVIEW2D_FRONT:
        case CAMVIEW2D_RIGHT:
        case CAMVIEW2D_REAR:
        case CAMVIEW2D_LEFT:
            return VM4x4(m_pxml->m_vcam[(int)viewMode - CAMVIEW2D_FRONT].pos,
                         m_pxml->m_vcam[(int)viewMode - CAMVIEW2D_FRONT].ori);

        default:
            return glm::mat4(1.0f);
    }
}

// ── Private helpers ───────────────────────────────────────────────────────────

int sanSceneAnimator::camIdx(VIEWMODE mode)
{
    return std::clamp((int)mode - CAMVIEW3D_FRONT, 0, SVM_CAMERAS_NUM - 1);
}

int sanSceneAnimator::defaultVcamIdx() const
{
    switch (m_activeViewMode)
    {
        case CAMVIEW3D_FRONT: case CAMVIEW2D_FRONT: return  0;
        case CAMVIEW3D_RIGHT: case CAMVIEW2D_RIGHT: return  1;
        case CAMVIEW3D_REAR:  case CAMVIEW2D_REAR:  return  2;
        case CAMVIEW3D_LEFT:  case CAMVIEW2D_LEFT:  return  3;
        case TOPVIEW3D:                             return 10;
        default:                                    return  0;
    }
}

void sanSceneAnimator::snapToDefaultPose()
{
    if (m_pxml == nullptr) return;
    const glm::vec3& pos    = m_pxml->m_vcam[defaultVcamIdx()].pos;
    const float      xyDist = std::sqrt(pos.x * pos.x + pos.y * pos.y);
    m_radius    = std::max(m_settings.minRadius, glm::length(pos));
    m_azimuth   = glm::degrees(std::atan2(pos.y, pos.x));
    m_elevation = (m_radius > 1e-6f)
                ? std::clamp(glm::degrees(std::atan2(pos.z, xyDist)),
                             m_settings.minElevation, m_settings.maxElevation)
                : m_settings.minElevation;
    m_tilt      = tiltOffset(pos, m_pxml->m_vcam[defaultVcamIdx()].ori);
}

glm::vec3 sanSceneAnimator::samplePos(VIEWMODE targetMode) const
{
    const SphericalPos sp = toSpherical(m_prevPos);
    const SphericalPos st = toSpherical(m_pxml->m_vcam[camIdx(targetMode)].pos);
    const float        ts = m_animT * m_animT * (3.0f - 2.0f * m_animT); // smoothstep
    return fromSpherical(sp.r         + ts * (st.r         - sp.r),
                         sp.azimuth   + ts * angleDiff(sp.azimuth, st.azimuth),
                         sp.elevation + ts * (st.elevation  - sp.elevation));
}

glm::quat sanSceneAnimator::sampleTilt(VIEWMODE targetMode) const
{
    const float ts = m_animT * m_animT * (3.0f - 2.0f * m_animT); // smoothstep
    return glm::slerp(tiltOffset(m_prevPos, m_prevOri),
                      tiltOffset(m_pxml->m_vcam[camIdx(targetMode)].pos,
                                 m_pxml->m_vcam[camIdx(targetMode)].ori), ts);
}

float sanSceneAnimator::arcAnimT(const glm::vec3& fromPos,
                                 const glm::vec3& toPos,
                                 VIEWMODE         xmlFromMode) const
{
    const glm::vec3 dirFrom    = safeNormalize(fromPos);
    const glm::vec3 dirTo      = safeNormalize(toPos);
    const glm::vec3 dirXmlFrom = safeNormalize(m_pxml->m_vcam[camIdx(xmlFromMode)].pos);

    const float fullArc = std::acos(glm::clamp(glm::dot(dirFrom,    dirTo), -1.0f, 1.0f));
    const float refArc  = std::acos(glm::clamp(glm::dot(dirXmlFrom, dirTo), -1.0f, 1.0f));

    if (fullArc < 1e-4f) return 1.0f; // already at target - snap
    const float remainingRatio = (refArc > 1e-4f) ? glm::clamp(fullArc / refArc, 0.0f, 1.0f) : 1.0f;
    // Invert smoothstep: smoothstep(t) = t²(3-2t).
    // Closed-form inverse: t = 0.5 - sin(asin(1 - 2s) / 3), where s = 1 - remainingRatio.
    const float s = 1.0f - remainingRatio;
    return glm::clamp(0.5f - std::sin(std::asin(1.0f - 2.0f * s) / 3.0f), 0.0f, 1.0f);
}

glm::mat4 sanSceneAnimator::animateTransition(VIEWMODE viewMode)
{
    const glm::vec3& targetPos = m_pxml->m_vcam[camIdx(viewMode)].pos;
    const glm::vec3& targetOri = m_pxml->m_vcam[camIdx(viewMode)].ori;

    m_animT += 1.0f / static_cast<float>(m_animationSteps);
    if (m_animT >= 1.0f)
    {
        m_animT = 1.0f; m_isAnimating = false;
        return VM4x4(targetPos, targetOri);
    }

    return buildVM(sampleTilt(viewMode) * glm::quat_cast(glm::mat3(lookAtOrigin(samplePos(viewMode)))),
                   samplePos(viewMode));
}

glm::mat4 sanSceneAnimator::animateReset()
{
    m_animT += 1.0f / static_cast<float>(m_animationSteps);
    if (m_animT >= 1.0f)
    {
        m_animT = 1.0f; m_resetAnimating = false;
        snapToDefaultPose();
        return buildInteractiveVM();
    }

    const float ts = m_animT * m_animT * (3.0f - 2.0f * m_animT);

    const SphericalPos sp = toSpherical(m_prevPos);
    const SphericalPos st = toSpherical(m_resetTargetPos);
    const glm::vec3 currPos = fromSpherical(sp.r         + ts * (st.r         - sp.r),
                                            sp.azimuth   + ts * angleDiff(sp.azimuth, st.azimuth),
                                            sp.elevation + ts * (st.elevation  - sp.elevation));

    const glm::quat qTo   = shortArc(m_resetQFrom, glm::quat_cast(glm::mat3(VM4x4(m_resetTargetPos, m_resetTargetOri))));
    return buildVM(glm::slerp(m_resetQFrom, qTo, ts), currPos);
}

glm::mat4 sanSceneAnimator::buildInteractiveVM() const
{
    const float     azRad = glm::radians(m_azimuth);
    const float     elRad = glm::radians(m_elevation);
    const glm::vec3 pos(m_radius * std::cos(elRad) * std::cos(azRad),
                        m_radius * std::cos(elRad) * std::sin(azRad),
                        m_radius * std::sin(elRad));
    return buildVM(m_tilt * glm::quat_cast(glm::mat3(lookAtOrigin(pos))), pos);
}

void sanSceneAnimator::applyOrbit(float dAzimDeg, float dElevDeg)
{
    m_azimuth  -= dAzimDeg * m_settings.orbitAzimuthSensitivity;
    m_elevation = std::clamp(m_elevation + dElevDeg * m_settings.orbitElevationSensitivity,
                             m_settings.minElevation, m_settings.maxElevation);
}

void sanSceneAnimator::applyZoom(float spanDeltaPx)
{
    m_radius = std::clamp(m_radius - spanDeltaPx * m_settings.zoomSensitivity,
                          m_settings.minRadius, m_settings.maxRadius);
}
