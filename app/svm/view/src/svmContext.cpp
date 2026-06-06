#include "svmContext.hpp"
#include "svmError.hpp"

// ── sanContext ────────────────────────────────────────────────────────────────

sanContext::sanContext()
{
    memset(m_itxID,   0x00, sizeof(m_itxID));
    memset(m_pitxID,  0x00, sizeof(m_pitxID));
    memset(m_mtxID,   0x00, sizeof(m_mtxID));
    memset(m_pmtxID,  0x00, sizeof(m_pmtxID));
    memset(m_aitxID,  0x00, sizeof(m_aitxID));
    memset(m_paitxID, 0x00, sizeof(m_paitxID));

    load_steering2wheel();

    m_pxml            = new sanXML();
    m_ptextRenderer   = new sanTextRenderer(m_pxml);
    m_pcameras        = new sanCamera(m_pxml);
    m_pmrt            = new sanMRT(m_pxml);
    m_pbowl           = new sanBowl(m_pxml, &m_pm);
    m_pcamView        = new sanCamView(m_pxml, m_pcameras, &m_pm);
    m_pvbc            = new sanVBC(m_pxml, &m_pm);
    m_pmodel_animator = new sanModelAnimator(m_pxml, &m_pm, &m_vehicleSignal);
    m_pscene_animator = new sanSceneAnimator(m_pxml);
    m_pdgs            = new sanDGS(m_pxml, &m_pm, &m_vehicleSignal.m_trigger);
    m_ppgs            = new sanPGS(m_pxml, &m_pm, m_pcameras, &m_vehicleSignal);
    m_pmobs           = new sanMOBS(m_pxml, &m_pm, m_pcameras, m_ppgs, &m_vehicleSignal);
    m_pod             = new sanOD(m_pxml, &m_pm, m_pcameras, m_pmobs, m_ptextRenderer, &m_vehicleSignal);
    m_posd            = new sanOsd(m_pxml);
}

sanContext::~sanContext()
{
    delete m_posd;            m_posd            = nullptr;
    delete m_pod;             m_pod             = nullptr;
    delete m_pmobs;           m_pmobs           = nullptr;
    delete m_ppgs;            m_ppgs            = nullptr;
    delete m_pdgs;            m_pdgs            = nullptr;
    delete m_pscene_animator; m_pscene_animator = nullptr;
    delete m_pmodel_animator; m_pmodel_animator = nullptr;
    delete m_pvbc;            m_pvbc            = nullptr;
    delete m_pcamView;        m_pcamView        = nullptr;
    delete m_pbowl;           m_pbowl           = nullptr;
    delete m_pmrt;            m_pmrt            = nullptr;
    delete m_pcameras;        m_pcameras        = nullptr;
    delete m_ptextRenderer;   m_ptextRenderer   = nullptr;
    delete m_pxml;            m_pxml            = nullptr;

    m_leftAngleMap.clear();
    m_rightAngleMap.clear();
    releaseResource();
}

// ── Factory / initialization ──────────────────────────────────────────────────

sanContextUPtr sanContext::Create(GLuint* front_txID, GLuint* right_txID,
                                  GLuint* rear_txID,  GLuint* left_txID,
                                  GLuint* add0_txID)
{
    auto ctx = sanContextUPtr(new sanContext());
    ctx->initialize(front_txID, right_txID, rear_txID, left_txID, add0_txID);
    return ctx;
}

void sanContext::initialize(GLuint* front_txID, GLuint* right_txID,
                            GLuint* rear_txID,  GLuint* left_txID,
                            GLuint* add0_txID)
{
    #if (HAVE_TO_REMOVE_IN_RELEASE_VERSION)
        logger.record_message(logger.svm_inform("********", "<<viewer started>>").what());
    #endif

    #if !(defined(_WIN32) || defined(_WIN64))
        if (front_txID == nullptr || right_txID == nullptr ||
            rear_txID  == nullptr || left_txID  == nullptr)
            throw logger.svm_fatal("C21xx001", __FUNCTION__ + string(" $argument wrong"));
    #endif

    try
    {
        // Order is significant - do not reorder.
        m_pxml->initialize();
        getResources(front_txID, right_txID, rear_txID, left_txID, add0_txID);
        setPM();
        setViewOption();

        m_ptextRenderer->initialize(
            string(_FONTS_PATH_) + "/" + string(m_pxml->m_od_parameter.info_font_name),
            m_pxml->m_od_parameter.info_font_size);

        m_pcameras->initialize();
        m_pbowl->initialize(m_pitxID, m_pmtxID);
        m_pcamView->initialize(m_pitxID, m_pmtxID, m_paitxID);
        m_pvbc->initialize();
        m_ppgs->initialize();
        m_pdgs->initialize();
        m_pod->initialize();
        m_pmobs->initialize();
        m_pmodel_animator->initialize();
        m_pscene_animator->initialize();
        m_posd->initialize();
        m_pmrt->initialize();
    }
    catch (exception& e)
    {
        string msg = e.what();
        throw runtime_error(msg.insert(msg.find('@') + 1, __FUNCTION__ + string(" -> ")));
    }
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void sanContext::setPM()
{
    try
    {
        const float w     = m_pxml->m_resolution.image.width;
        const float h     = m_pxml->m_resolution.image.height;
        const float ratio = (h > 0.0f) ? (w / h) : 1.0f;

        m_pm.opm = glm::ortho(m_pxml->m_orthogrphic.left,   m_pxml->m_orthogrphic.right,
                              m_pxml->m_orthogrphic.bottom, m_pxml->m_orthogrphic.top,
                              m_pxml->m_orthogrphic.znear,  m_pxml->m_orthogrphic.zfar);
        m_pm.ppm = glm::perspective<float>(
            glm::radians(m_pxml->m_perspective.fov), ratio,
            m_pxml->m_perspective.znear, m_pxml->m_perspective.zfar);
    }
    catch (exception& e)
    {
        throw logger.svm_fatal("C2101301", __FUNCTION__ + delimiter(string(e.what())));
    }
}

void sanContext::setViewOption()
{
    try
    {
        sanError::glClearError();
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);
        sanError::glCheckError();
    }
    catch (exception& e)
    {
        throw logger.svm_fatal("C2101401", __FUNCTION__ + delimiter(string(e.what())));
    }
}

void sanContext::clearView()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// ── Vehicle state ─────────────────────────────────────────────────────────────

void sanContext::updateVehicleStateFromSignal()
{
    if (m_vehicleSignal.m_trigger.gear == GEAR::GEAR_REVERSE)
        { m_vehicleState = VEHICLESTATE::REVERSE; return; }

    switch (m_vehicleSignal.m_trigger.turn_signal)
    {
        case TURN_SIGNAL::TURN_SIGNAL_RIGHT:     m_vehicleState = VEHICLESTATE::RIGHT_TURN; break;
        case TURN_SIGNAL::TURN_SIGNAL_LEFT:      m_vehicleState = VEHICLESTATE::LEFT_TURN;  break;
        case TURN_SIGNAL::TURN_SIGNAL_EMERGENCY: m_vehicleState = VEHICLESTATE::EMERGENCY;  break;
        default:                                 m_vehicleState = VEHICLESTATE::FORWARD;    break;
    }
}

void sanContext::updateViewLayout()
{
    switch (m_vehicleState)
    {
        case VEHICLESTATE::FORWARD:    m_pxml->m_layout[1].view_mode = m_pxml->m_view_setting.default_view; break;
        case VEHICLESTATE::RIGHT_TURN: m_pxml->m_layout[1].view_mode = m_pxml->m_view_setting.right_turn;   break;
        case VEHICLESTATE::REVERSE:    m_pxml->m_layout[1].view_mode = m_pxml->m_view_setting.rear_gear;    break;
        case VEHICLESTATE::LEFT_TURN:  m_pxml->m_layout[1].view_mode = m_pxml->m_view_setting.left_turn;    break;
        case VEHICLESTATE::EMERGENCY:  m_pxml->m_layout[1].view_mode = m_pxml->m_view_setting.emergency;    break;
        default: break;
    }
}

// ── Resource management ───────────────────────────────────────────────────────

void sanContext::releaseResource()
{
    for (int i = 0; i < SVM_CAMERAS_NUM; i++)
    {
        if (m_mtxID[i] != 0) { glDeleteTextures(1, &m_mtxID[i]); m_mtxID[i] = 0; }
        m_pmtxID[i] = nullptr;
        if (m_itxID[i] != 0) { glDeleteTextures(1, &m_itxID[i]); m_itxID[i] = 0; }
        m_pitxID[i] = nullptr;
    }
    for (int i = 0; i < ADD_CAMERAS_NUM; i++)
    {
        if (m_aitxID[i] != 0) { glDeleteTextures(1, &m_aitxID[i]); m_aitxID[i] = 0; }
        m_paitxID[i] = nullptr;
    }

    for (auto& img : m_source_images)     img.release();
    for (auto& img : m_additional_images) img.release();
    vector<cv::Mat>().swap(m_source_images);
    vector<cv::Mat>().swap(m_additional_images);
}

void sanContext::getResources(GLuint* front_txID, GLuint* right_txID,
                              GLuint* rear_txID,  GLuint* left_txID,
                              GLuint* add0_txID)
{
    try
    {
        load_textures_for_images(front_txID, right_txID, rear_txID, left_txID, add0_txID);
        load_textures_for_masks();
    }
    catch (exception& e)
    {
        throw logger.svm_fatal("C2101201", __FUNCTION__ + delimiter(string(e.what())));
    }
}

void sanContext::load_textures_for_images(GLuint* front_txID, GLuint* right_txID,
                                          GLuint* rear_txID,  GLuint* left_txID,
                                          GLuint* add0_txID)
{
    try
    {
        #if defined(_WIN32) || defined(_WIN64)
            for (int i = 0; i < SVM_CAMERAS_NUM; i++)
                load_image_from_file(i, "src", m_source_images,     m_pitxID,  m_itxID);
            for (int i = 0; i < ADD_CAMERAS_NUM; i++)
                load_image_from_file(i, "add", m_additional_images, m_paitxID, m_aitxID);
        #else
            m_pitxID[0]  = front_txID;
            m_pitxID[1]  = right_txID;
            m_pitxID[2]  = rear_txID;
            m_pitxID[3]  = left_txID;
            m_paitxID[0] = add0_txID;
        #endif
    }
    catch (exception& e)
    {
        throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
    }
}

void sanContext::load_textures_for_masks()
{
    try
    {
        for (int i = 0; i < SVM_CAMERAS_NUM; i++)
        {
            const string tag      = "(camID[" + to_string(i) + "])";
            const string filepath = string(_MASK_IMAGES_PATH_) + "/mask" + to_string(i) + ".jpg";
            try
            {
                cv::Mat mask = sanFromFile::read_image(filepath, IMREAD_GRAYSCALE);
                if (mask.cols != (int)m_pxml->m_resolution.image.width ||
                    mask.rows != (int)m_pxml->m_resolution.image.height)
                    throw runtime_error(string("$mask size wrong - ") + filepath);

                m_pmtxID[i] = &m_mtxID[i];
                sanVABT::generateTexture(GL_TEXTURE1, &m_mtxID[i]);
                sanVABT::updateTexture(GL_TEXTURE1, m_mtxID[i],
                                       mask.ptr(), mask.cols, mask.rows,
                                       mask.channels(), BINDING);
            }
            catch (exception& e)
            {
                throw runtime_error(tag + delimiter(string(e.what())));
            }
        }
    }
    catch (exception& e)
    {
        throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
    }
}

void sanContext::load_steering2wheel()
{
    try
    {
        m_leftAngleMap  = sanFromFile::read_map(string(_MAPPINGS_PATH_) + "/steering2wheel_left.txt");
        m_rightAngleMap = sanFromFile::read_map(string(_MAPPINGS_PATH_) + "/steering2wheel_right.txt");
    }
    catch (exception& e)
    {
        throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
    }
}

#if defined(_WIN32) || defined(_WIN64)
    void sanContext::load_image_from_file(int camID, const string& prefix,
                                          vector<cv::Mat>& imgVec,
                                          GLuint* txID[], GLuint txArr[])
    {
        const string tag      = "(" + prefix + " camID[" + to_string(camID) + "])";
        const string filepath = string(_SOURCE_IMAGES_PATH_) + "/" + prefix + to_string(camID) + ".jpg";
        try
        {
            cv::Mat img = sanFromFile::read_image(filepath, IMREAD_COLOR);
            if (img.cols != (int)m_pxml->m_resolution.image.width ||
                img.rows != (int)m_pxml->m_resolution.image.height)
                throw runtime_error(string("$image size wrong - ") + filepath);

            imgVec.push_back(img);
            txID[camID] = &txArr[camID];
            sanVABT::generateTexture(GL_TEXTURE0, &txArr[camID]);
        }
        catch (exception& e)
        {
            throw runtime_error(tag + delimiter(string(e.what())));
        }
    }
#endif
