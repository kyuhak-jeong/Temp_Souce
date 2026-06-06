#ifndef SVMCONTEXT_HPP_
#define SVMCONTEXT_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmXML.hpp"
#include "svmTextRenderer.hpp"
#include "svmFromFile.hpp"
#include "svmVABT.hpp"
#include "svmCamera.hpp"
#include "svmBowl.hpp"
#include "svmCamView.hpp"
#include "svmModel.hpp"
#include "svmPGS.hpp"
#include "svmOD.hpp"
#include "svmDGS.hpp"
#include "svmMOBS.hpp"
#include "svmSceneAnimator.hpp"
#include "svmCamvec.hpp"
#include "svmVBC.hpp"
#include "svmMRT.hpp"
#include "svmModelAnimator.hpp"
#include "svmOsd.hpp"

CLASS_PTR(sanContext)

class sanContext : public sanVABT
{
public:
    sanContext();
    ~sanContext();

    static sanContextUPtr Create(GLuint* front_txID, GLuint* right_txID,
                                 GLuint* rear_txID,  GLuint* left_txID,
                                 GLuint* add0_txID);
    void initialize(GLuint* front_txID, GLuint* right_txID,
                    GLuint* rear_txID,  GLuint* left_txID,
                    GLuint* add0_txID);

    // ── Rendering ─────────────────────────────────────────────────────────
    void setPM();
    void setViewOption();
    void clearView();

    // ── Vehicle state ─────────────────────────────────────────────────────
    void updateVehicleStateFromSignal();
    void updateViewLayout();

    // ── Animator access ───────────────────────────────────────────────────
    ISvmAnimator* getAnimator() { return m_pscene_animator; }

    // ── Input state ───────────────────────────────────────────────────────
    int       m_mousePressed = 0;
    glm::vec2 m_mousePrepos  = glm::vec2(0.0f);

    // ── Vehicle signal & state ────────────────────────────────────────────
    VEHICLESIGNAL m_vehicleSignal = { { GEAR_PARKING, TURN_SIGNAL_OFF }, 0.0f, 0.0f, 0.0f };
    VEHICLESTATE  m_vehicleState  = VEHICLESTATE::FORWARD;

    // ── Texture resources ─────────────────────────────────────────────────
    GLuint  m_itxID  [SVM_CAMERAS_NUM];     // source image texture IDs
    GLuint* m_pitxID [SVM_CAMERAS_NUM];     // pointers to source image texture IDs
    GLuint  m_mtxID  [SVM_CAMERAS_NUM];     // mask image texture IDs
    GLuint* m_pmtxID [SVM_CAMERAS_NUM];     // pointers to mask image texture IDs
    GLuint  m_aitxID [ADD_CAMERAS_NUM];     // additional image texture IDs
    GLuint* m_paitxID[ADD_CAMERAS_NUM];     // pointers to additional image texture IDs

    vector<cv::Mat> m_source_images;
    vector<cv::Mat> m_additional_images;

    // ── Matrices & mappings ───────────────────────────────────────────────
    PM                m_pm;
    map<float, float> m_leftAngleMap;
    map<float, float> m_rightAngleMap;

    // ── Sub-systems ───────────────────────────────────────────────────────
    sanXML*           m_pxml            = nullptr;
    sanTextRenderer*  m_ptextRenderer   = nullptr;
    sanCamera*        m_pcameras        = nullptr;
    sanMRT*           m_pmrt            = nullptr;
    sanBowl*          m_pbowl           = nullptr;
    sanCamView*       m_pcamView        = nullptr;
    sanVBC*           m_pvbc            = nullptr;
    sanModelAnimator* m_pmodel_animator = nullptr;
    sanSceneAnimator* m_pscene_animator = nullptr;
    sanDGS*           m_pdgs            = nullptr;
    sanPGS*           m_ppgs            = nullptr;
    sanMOBS*          m_pmobs           = nullptr;
    sanOD*            m_pod             = nullptr;
    sanOsd*           m_posd            = nullptr;

private:
    void releaseResource();
    void getResources            (GLuint* front_txID, GLuint* right_txID,
                                  GLuint* rear_txID,  GLuint* left_txID,
                                  GLuint* add0_txID);
    void load_textures_for_images(GLuint* front_txID, GLuint* right_txID,
                                  GLuint* rear_txID,  GLuint* left_txID,
                                  GLuint* add0_txID);
    void load_textures_for_masks();
    void load_steering2wheel();

    #if defined(_WIN32) || defined(_WIN64)
        void load_image_from_file(int camID, const string& prefix,
                                  vector<cv::Mat>& imgVec, GLuint* txID[], GLuint txArr[]);
    #endif
};

#endif // SVMCONTEXT_HPP_
