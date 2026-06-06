#ifndef _SVM_CALIB_FISHEYE_HPP_
#define _SVM_CALIB_FISHEYE_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmVABT.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmLogger.hpp"
#include "svmCamera.hpp"

class sanCalibFisheye: public sanVABT
{
public:
	sanXML* m_pxml;                       
	vector<cv::Mat> m_fisheye_images;
	vector<cv::Mat> m_additional_fisheye_images;
	sanShader m_fisheyeShader = sanShader(vs_calib_image, NULL, fs_calib_image); 

public:
	sanCalibFisheye(sanXML* pxml);
	~sanCalibFisheye();
	void initialize(unsigned char* img_front, unsigned char* img_right, unsigned char* img_rear, unsigned char* img_left, unsigned char* add_0);
	void renderFisheyes(int camID);

public:
	// for drawing four images with GL
	float m_fisheyeLUT[30] = {	-1.0f, 1.0f, 0.0f, 0.0f, 0.0f,  // GL vertex(x, y, z),  normalized image(u, v)
								-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
								1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
								1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
								-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
								1.0f, -1.0f, 0.0f, 1.0f, 1.0f };

};

#endif

