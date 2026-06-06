#include "svmCalibContext.hpp"
#include "svmCalibMask.hpp"
#include "svmWorld.hpp"
#include "svmError.hpp"
#include "svmTime.hpp"

// ── Factory ───────────────────────────────────────────────────────────────────

sanCalibContextUPtr sanCalibContext::Create(unsigned char* img_front, unsigned char* img_right,
                                            unsigned char* img_rear,  unsigned char* img_left,
                                            unsigned char* add_0)
{
    auto context = sanCalibContextUPtr(new sanCalibContext());
    context->initialize(img_front, img_right, img_rear, img_left, add_0);
    return (sanCalibContextUPtr)std::move(context);
}

// ── Constructor / Destructor ──────────────────────────────────────────────────

sanCalibContext::sanCalibContext()
{
    m_pxml          = new sanXML();
    m_ptextRenderer = new sanTextRenderer(m_pxml);
    m_pcameras      = new sanCamera(m_pxml);
    m_pmrt          = new sanMRT(m_pxml);
    m_pfisheyes     = new sanCalibFisheye(m_pxml);
    m_pdefisheyes   = new sanCalibDefisheye(m_pxml, m_pcameras, m_pfisheyes);
    m_pcontours     = new sanCalibContour(m_pxml, m_pcameras, m_pdefisheyes, m_ptextRenderer);
    m_pcalibration  = new sanCalibCal(m_pxml, m_pcameras, m_pdefisheyes, m_pcontours);
    m_pgrids        = new sanCalibGrid(m_pxml, m_pcameras, m_pcalibration);
    m_pmasks        = new sanCalibMask(m_pxml, m_pcameras, m_pgrids);
    m_pLUTs         = new sanCalibBowl(m_pxml, m_pfisheyes, m_pcameras, m_pgrids, m_pmasks);

    resetStepStates();
    m_state.keyPressed = true;
}

sanCalibContext::~sanCalibContext()
{
    if (m_pLUTs         != nullptr) { delete m_pLUTs;         m_pLUTs         = nullptr; }
    if (m_pmasks        != nullptr) { delete m_pmasks;        m_pmasks        = nullptr; }
    if (m_pgrids        != nullptr) { delete m_pgrids;        m_pgrids        = nullptr; }
    if (m_pcalibration  != nullptr) { delete m_pcalibration;  m_pcalibration  = nullptr; }
    if (m_pcontours     != nullptr) { delete m_pcontours;     m_pcontours     = nullptr; }
    if (m_pdefisheyes   != nullptr) { delete m_pdefisheyes;   m_pdefisheyes   = nullptr; }
    if (m_pfisheyes     != nullptr) { delete m_pfisheyes;     m_pfisheyes     = nullptr; }
    if (m_pmrt          != nullptr) { delete m_pmrt;          m_pmrt          = nullptr; }
    if (m_pcameras      != nullptr) { delete m_pcameras;      m_pcameras      = nullptr; }
    if (m_ptextRenderer != nullptr) { delete m_ptextRenderer; m_ptextRenderer = nullptr; }
    if (m_pxml          != nullptr) { delete m_pxml;          m_pxml          = nullptr; }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void sanCalibContext::initialize(unsigned char* img_front, unsigned char* img_right,
                                 unsigned char* img_rear,  unsigned char* img_left,
                                 unsigned char* add_0)
{
    logger.record_message(logger.svm_inform("********", "<<calibration started>>").what());

    if (img_front == nullptr || img_right == nullptr || img_rear == nullptr || img_left == nullptr)
        throw logger.svm_fatal("C11xx001", __FUNCTION__ + string("$arguement wrong"));

    try
    {
        // Order of calls is significant - do not reorder.
        m_pxml->initialize();
        m_ptextRenderer->initialize(string(_FONTS_PATH_) + "/" + "NotoSans-Bold.ttf", 100);
        m_pcameras->initialize();
        m_pfisheyes->initialize(img_front, img_right, img_rear, img_left, add_0);
        m_pdefisheyes->initialize();
        m_pcontours->initialize();
        m_pcalibration->initialize();
        m_pgrids->initialize();
        m_pmasks->initialize();
        m_pLUTs->initialize();
        m_pmrt->initialize();

        logger.record_message(logger.svm_inform("--------", "initialization step successful").what());
    }
    catch (exception& e)
    {
        string msg = e.what();
        throw runtime_error(msg.insert(msg.find('@') + 1, __FUNCTION__ + string(" -> ")));
    }
}

void sanCalibContext::run()
{
    try
    {
        const int  vm         = m_state.viewMode;
        const int  cam        = m_state.camIndex;
        const bool keyPressed = m_state.keyPressed;

        // Layout helpers - reused by all multi-camera views
        const int   rowNum  = 2;
        const int   colNum  = 2;
        const float rowNorm = (float)(m_pxml->m_resolution.display.height / rowNum);
        const float colNorm = (float)(m_pxml->m_resolution.display.width  / colNum);

        auto tileViewport = [&](int camID)
        {
            int rowIdx = (int)(camID % 2);
            int colIdx = (int)((bool)(camID % 3));
            Point2f tl(colIdx * colNorm, rowIdx * rowNorm);
            glViewport((int)tl.x, (int)tl.y, (int)colNorm, (int)rowNorm);
        };

        switch (vm)
        {
        case FISHEYE_VIEW:
            for (int i = 0; i < m_pxml->m_svm_cameras_num; i++)
            {
                tileViewport(i);
                m_pfisheyes->renderFisheyes(i);
            }
            break;

        case DEFISHEYE_VIEW:
            for (int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                tileViewport(i);
                m_pdefisheyes->renderDefisheyes(i);
            }
            break;

        case FEATURES_VIEW:
            if (keyPressed == true && m_pcontours->m_is_feature_pts_edited[cam] == false)
            {
                m_pcontours->getFeaturePoints(cam);
                m_pcontours->m_is_feature_pts_edited[cam] = true;
                m_state.pointUpdated = true;
            }

            if (m_pcontours != nullptr && m_state.enterAdjustMode)
            {
                m_state.selectedPtPos = m_pcontours->m_feature_pts[cam][m_state.selectedPtIdx];
            }

            if (m_state.pointUpdated == true)
            {
                m_pcontours->m_selected_point_idx = m_state.selectedPtIdx;
                m_pcontours->updateContours(cam);
                m_state.pointUpdated   = false;
                m_state.contourUpdated = true;
            }

            glViewport(0, 0,
                       (int)m_pxml->m_resolution.display.width,
                       (int)m_pxml->m_resolution.display.height);
            m_pdefisheyes->renderDefisheyes(cam);
            m_pcontours->renderContours(cam, m_state.enterAdjustMode);
            break;

        case CONTOURS_VIEW:
            for (int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                tileViewport(i);
                m_pdefisheyes->renderDefisheyes(i);
                m_pcontours->renderContours(i);
            }
            break;

        case GRIDS_VIEW:
            for (int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                if (keyPressed == true &&
                    (m_pgrids->m_is_grids_generated[i] == false || m_state.contourUpdated == true))
                {
                    m_pcalibration->doCalibration(i);
                    m_pgrids->createGrids(i);
                    m_pgrids->updateGrids(i);
                    m_pgrids->m_is_grids_generated[i] = true;
                    m_state.contourUpdated = false;
                }
                tileViewport(i);
                m_pdefisheyes->renderDefisheyes(i);
                m_pgrids->renderGrids(i);
            }
            break;

        case MASKS_VIEW:
            if (keyPressed == true)
            {
                m_pmasks->createMasks();
                m_pmasks->saveMasks();
                m_pmasks->updateMasks();
            }
            for (int i = 0; i < SVM_CAMERAS_NUM; i++)
            {
                tileViewport(i);
                m_pmasks->renderMasks(i);
            }
            break;

        case LUTS_VIEW:
            if (keyPressed == true)
            {
                m_pLUTs->getLUTs();
                m_pLUTs->updateLUTs();
                this->saveSettings(m_pxml->m_settings_calibration_filePath);
                this->saveSettings(m_pxml->m_output_calibration_filePath);
            }
            glViewport(0, 0,
                       (int)m_pxml->m_resolution.display.width,
                       (int)m_pxml->m_resolution.display.height);
            m_pLUTs->renderBowl();
            if (keyPressed == true) captureResult(m_pxml->m_output_calibration_result_image_filePath);
            break;

        default:
            throw logger.svm_fatal("C12xx001",
                string("view mode[") + to_string(vm) + string("] wrong"));
        }

        m_state.keyPressed = false;
    }
    catch (exception& e)
    {
        string msg = e.what();
        throw runtime_error(msg.insert(msg.find('@') + 1, __FUNCTION__ + string(" -> ")));
    }
}

void sanCalibContext::clearView()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void sanCalibContext::saveSettings(string filePath)
{
    try
    {
        xmlResetLastError();

        char*      full_path    = (char*)&filePath.c_str()[0];
        xmlDocPtr  xml_doc_ptr  = xmlReadFile(full_path, nullptr, 0);
        if (xml_doc_ptr == nullptr)
        {
            xmlFreeDoc(xml_doc_ptr);
            xmlCleanupParser();
            throw runtime_error(string(xmlGetLastError()->message));
        }

        WRITING_PARAMETERS wparameters;
        m_pxml->m_calibrated_status.calibration_done = 1;
        m_pxml->m_calibrated_status.completion_time  = sanTime::get_string_time(0, true);
        wparameters.calibrated_status.push_back(to_string(m_pxml->m_calibrated_status.calibration_done));
        wparameters.calibrated_status.push_back(m_pxml->m_calibrated_status.completion_time);
        wparameters.poster_size.push_back(m_pxml->m_arrangement.poster.width);
        wparameters.poster_size.push_back(m_pxml->m_arrangement.poster.height);
        wparameters.poster_size.push_back(m_pxml->m_arrangement.poster.radius_scale);

        for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
        {
            string real_camera_basename = "rcam" + to_string(camID);

            memset(wparameters.feature_points, 0x00, sizeof(wparameters.feature_points));
            m_pxml->format_feature_points_for_writing(m_pcontours->m_feature_pts[camID], wparameters.feature_points);

            memset(wparameters.local_points, 0x00, sizeof(wparameters.local_points));
            m_pxml->format_local_points_for_writing(m_pxml->m_local_pts[camID], wparameters.local_points);

            memset(wparameters.calibrated_parameter, 0x00, sizeof(wparameters.calibrated_parameter));
            m_pxml->format_calibrated_parameters_for_writing(m_pxml->m_calibrated_parameters[camID], wparameters.calibrated_parameter);

            m_pxml->xmlUpdateCache(xml_doc_ptr, wparameters, real_camera_basename);
        }

        xmlSaveFormatFileEnc(full_path, xml_doc_ptr, "UTF-8", 1);
        xmlFreeDoc(xml_doc_ptr);
        xmlCleanupParser();
    }
    catch (exception& e)
    {
        throw logger.svm_fatal("C12xx101", __FUNCTION__ + delimiter(string(e.what())));
    }
}

void sanCalibContext::captureResult(string filePath)
{
    try
    {
        const int w = (int)m_pxml->m_resolution.display.width;
        const int h = (int)m_pxml->m_resolution.display.height;

        unsigned char* image = (unsigned char*)calloc(4 * (size_t)w * (size_t)h, sizeof(unsigned char));
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, image);

        cv::Mat result = cv::Mat(h, w, CV_8UC4, image);
        cv::flip(result, result, 0);
        cv::cvtColor(result, result, cv::COLOR_BGR2RGB);
        sanFromFile::write_image(filePath, result);

        if (image != nullptr) free(image);
    }
    catch (exception& e)
    {
        throw logger.svm_fatal("C12xx201", __FUNCTION__ + delimiter(string(e.what())));
    }
}

// ── State API ─────────────────────────────────────────────────────────────────

bool sanCalibContext::requestViewMode(int mode)
{
    const int stepIdx = mode; // VIEW_MODE values map 1:1 to step indices
    if (stepIdx >= 0 && stepIdx < CALIB_STEP_COUNT &&
        m_state.stepStates[stepIdx] == CalibStepStatus::LOCKED)
    {
        m_state.stepDenied = true;
        return false;
    }
    if (m_state.viewMode == mode) return true;
    m_state.viewMode   = mode;
    m_state.keyPressed = true;
    return true;
}

bool sanCalibContext::requestCamIndex(int idx)
{
    if (idx < 0 || idx >= SVM_CAMERAS_NUM) return false;
    if (m_state.camStepStates[idx] == CalibStepStatus::LOCKED)
    {
        m_state.stepDenied = true;
        return false;
    }
    if (m_state.camIndex == idx) return true;
    m_state.camIndex   = idx;
    m_state.keyPressed = true;
    return true;
}

bool sanCalibContext::requestAdjustMode(bool enter)
{
    if (m_state.viewMode != FEATURES_VIEW) return false;
    m_state.enterAdjustMode = enter;
    if (enter == true && m_pcontours != nullptr) m_state.pointUpdated = true;
    return true;
}

void sanCalibContext::advanceSelectedPoint()
{
    if (m_pcontours == nullptr) return;
    if (m_pgrids != nullptr)
    {
        for (int i = 0; i < SVM_CAMERAS_NUM; i++)
            m_pgrids->m_is_grids_generated[i] = false;
    }

    if (m_state.selectedPtIdx < CONTROL_POINTS_NUM - 1)
    {
        m_state.selectedPtIdx++;
        m_state.pointUpdated = true;
    }
    else
    {
        resetSelectedPoint();
        requestAdjustMode(false);
    }
}

void sanCalibContext::resetSelectedPoint()
{
    m_state.selectedPtIdx = 0;
    m_state.pointUpdated  = true;
}

void sanCalibContext::setPointPosition(XY pos)
{
    if (m_pcontours == nullptr) return;
    const int cam   = m_state.camIndex;
    const int ptIdx = m_state.selectedPtIdx;
    m_pcontours->m_feature_pts[cam][ptIdx] = pos;
    m_state.selectedPtPos  = pos;
    m_state.pointUpdated   = true;
}

void sanCalibContext::notifyStepComplete(int stepIdx, int camIdx)
{
    if (stepIdx < 0 || stepIdx >= CALIB_STEP_COUNT) return;

    if (stepIdx == (int)FEATURES_VIEW)
    {
        if (camIdx >= 0 && camIdx < SVM_CAMERAS_NUM)
        {
            m_state.featureCamsComplete |= static_cast<uint8_t>(1u << camIdx);
            m_state.camStepStates[camIdx] = CalibStepStatus::COMPLETE;
            const int nextCam = camIdx + 1;
            if (nextCam < SVM_CAMERAS_NUM && m_state.camStepStates[nextCam] == CalibStepStatus::LOCKED)
                m_state.camStepStates[nextCam] = CalibStepStatus::AVAILABLE;
        }
        if (m_state.featureCamsComplete != 0x0F) return;
    }

    if (m_state.stepStates[stepIdx] == CalibStepStatus::COMPLETE) return;
    m_state.stepStates[stepIdx] = CalibStepStatus::COMPLETE;
    unlockNextStep(stepIdx);
}

bool sanCalibContext::canAccessStep(int stepIdx) const
{
    if (stepIdx < 0 || stepIdx >= CALIB_STEP_COUNT) return false;
    return m_state.stepStates[stepIdx] != CalibStepStatus::LOCKED;
}

void sanCalibContext::resetStepStates()
{
    m_state.featureCamsComplete = 0;
    for (int i = 0; i < CALIB_STEP_COUNT; i++)
        m_state.stepStates[i] = (i == 0) ? CalibStepStatus::AVAILABLE : CalibStepStatus::LOCKED;
    for (int c = 0; c < SVM_CAMERAS_NUM; c++)
        m_state.camStepStates[c] = (c == 0) ? CalibStepStatus::AVAILABLE : CalibStepStatus::LOCKED;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void sanCalibContext::unlockNextStep(int stepIdx)
{
    const int next = stepIdx + 1;
    if (next < CALIB_STEP_COUNT && m_state.stepStates[next] == CalibStepStatus::LOCKED)
        m_state.stepStates[next] = CalibStepStatus::AVAILABLE;
}
