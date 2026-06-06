#ifndef _SVM_CALIB_CONTEXT_HPP_
#define _SVM_CALIB_CONTEXT_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmXML.hpp"
#include "svmTextRenderer.hpp"
#include "svmFromFile.hpp"
#include "svmVABT.hpp"
#include "svmShaderString.hpp"
#include "svmCalibGrid.hpp"
#include "svmCamera.hpp"
#include "svmCalibMask.hpp"
#include "svmCalibFisheye.hpp"
#include "svmCalibDefisheye.hpp"
#include "svmCalibContour.hpp"
#include "svmCalibCal.hpp"
#include "svmCalibBowl.hpp"
#include "svmMRT.hpp"
#include <array>
#include <functional>


enum VIEW_MODE { FISHEYE_VIEW = 0, DEFISHEYE_VIEW = 1, FEATURES_VIEW = 2, CONTOURS_VIEW = 3, GRIDS_VIEW = 4, MASKS_VIEW = 5, LUTS_VIEW = 6 };

CLASS_PTR(sanCalibContext)

class sanCalibContext
{
public:
    sanCalibContext();
    ~sanCalibContext();

    static sanCalibContextUPtr Create(unsigned char* img_front, unsigned char* img_right,
                                      unsigned char* img_rear,  unsigned char* img_left,
                                      unsigned char* add_0);

    void initialize(unsigned char* img_front, unsigned char* img_right,
                    unsigned char* img_rear,  unsigned char* img_left,
                    unsigned char* add_0);
    void run();
    void clearView();
    void saveSettings(string filePath);
    void captureResult(string filePath);

    const CalibState* getState() const { return &m_state; }

    bool requestViewMode(int mode);
    bool requestCamIndex(int idx);
    bool requestAdjustMode(bool enter);

    void advanceSelectedPoint();
    void resetSelectedPoint();
    void setPointPosition(XY pos);

    void notifyStepComplete(int stepIdx, int camIdx = -1);
    bool canAccessStep(int stepIdx) const;
    void resetStepStates();
    void clearStepDenied() { m_state.stepDenied = false; }

    sanMRT*           getMRT()       const { return m_pmrt;       }
    sanCalibContour*  getContours()  const { return m_pcontours;  }

public:
    // Mouse / raw input state (set by calib_ui_process before run())
    int       m_mousePressed = 0;
    glm::vec2 m_mousePrepos  = glm::vec2(0, 0);

private:
    void unlockNextStep(int stepIdx);

    sanXML*            m_pxml            = nullptr;
    sanTextRenderer*   m_ptextRenderer   = nullptr;
    sanCamera*         m_pcameras        = nullptr;
    sanCalibFisheye*   m_pfisheyes       = nullptr;
    sanCalibDefisheye* m_pdefisheyes     = nullptr;
    sanCalibContour*   m_pcontours       = nullptr;
    sanCalibCal*       m_pcalibration    = nullptr;
    sanCalibGrid*      m_pgrids          = nullptr;
    sanCalibMask*      m_pmasks          = nullptr;
    sanCalibBowl*      m_pLUTs           = nullptr;
    sanMRT*            m_pmrt            = nullptr;

    CalibState m_state;
};

#endif
