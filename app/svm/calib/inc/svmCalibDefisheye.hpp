#ifndef _SVM_CALIB_DEFISHEYE_HPP_
#define _SVM_CALIB_DEFISHEYE_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmVABT.hpp"
#include "svmLogger.hpp"
#include "svmCamera.hpp"
#include "svmCalibFisheye.hpp"
#include "svmShader.hpp"
#include "svmShaderString.hpp"
#include "svmFromFile.hpp"

class sanCalibDefisheye: public sanVABT
{
public:
	sanXML* m_pxml;                        // Setting parameters
	sanCamera* m_pcameras;
	sanCalibFisheye* m_pfisheyes;
	vector<Mat> m_defisheye_images;
	sanShader m_defisheyeShader = sanShader(vs_calib_image, NULL, fs_calib_image);
	float* m_defisheyeLUT[SVM_CAMERAS_NUM] = { nullptr };
	int m_vertices_num[SVM_CAMERAS_NUM] = { 0 };

public:
	sanCalibDefisheye(sanXML* pxml, sanCamera* pcameras, sanCalibFisheye* pfisheyes);
	~sanCalibDefisheye();

	void initialize();
	void createDefisheyeLUT(int density = 2);
	void renderDefisheyes(int camID);
};

#endif

