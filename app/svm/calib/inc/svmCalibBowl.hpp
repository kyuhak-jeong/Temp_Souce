#ifndef _SVM_CALIB_BOWL_HPP_
#define _SVM_CALIB_BOWL_HPP_

#include "svmCore.hpp"
#include "svmCamera.hpp"
#include "svmXML.hpp"
#include "svmVABT.hpp"
#include "svmCalibMask.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"

class sanCalibBowl: public sanVABT
{
public:
    sanXML* m_pxml;
    sanCalibFisheye* m_pfisheye;
    sanCamera* m_pcameras;
    sanCalibGrid* m_pgrids;
    sanCalibMask* m_pmasks;

public:
    vector<vector<array<float, 5>>> m_all_luts;
    vector<vector<array<float, 5>>> m_overlap_luts;
    vector<vector<array<float, 5>>> m_nonoverlap_luts;

    sanShader m_bowlShader = sanShader(vs_calib_bowl, NULL, fs_calib_bowl);

public:
    sanCalibBowl(sanXML* pxml, sanCalibFisheye* pfisheye, sanCamera* pcameras, sanCalibGrid* pgrids, sanCalibMask* pmasks);
    ~sanCalibBowl();
    void initialize();

    void createLUT(int camID);
    void splitLUT(int camID); // overlapped and nonoverlapped
    int saveLUT(int camID);

    void getLUTs();
    void updateLUTs();

    void renderBowl();
};

#endif 