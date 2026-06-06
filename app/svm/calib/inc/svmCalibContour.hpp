#ifndef _SVM_CALIB_CONTOUR_HPP_
#define _SVM_CALIB_CONTOUR_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmVABT.hpp"
#include "svmCalibDefisheye.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmLogger.hpp"
#include "svmTextRenderer.hpp"

#define DEBUGGING_CONTOUR			(0)

class sanCalibContour: public sanVABT
{
public:
	sanXML* m_pxml;
	sanCamera* m_pcameras;
	sanCalibDefisheye* m_pdefisheyes;
	sanTextRenderer* m_ptextRenderer;

public:
	XY m_feature_pts[SVM_CAMERAS_NUM][CONTROL_POINTS_NUM];
	sanShader m_contourShader = sanShader(vs_calib_contour, NULL, fs_calib_contour);
	float* m_contourLUT[SVM_CAMERAS_NUM] = {nullptr};
	int m_vertices_num[SVM_CAMERAS_NUM] = {0};
	bool m_is_vabt_generated[SVM_CAMERAS_NUM] = {false};
	bool m_is_feature_pts_edited[SVM_CAMERAS_NUM] = {false};
	
	float* m_magnifyingLUT = nullptr;
	float* m_magnifyingTexLUT = nullptr;
	bool m_is_vabt_manifying_generated = false;
	int m_selected_point_idx = 0;

public:
	sanCalibContour(sanXML*pxml, sanCamera* pcameras, sanCalibDefisheye* pdefisheyes, sanTextRenderer* ptextRenderer);
	~sanCalibContour();
	void initialize();
	void renderContours(int camID, bool render_magnify = false, GLuint glDrawingType = GL_LINES);
	void createContourLUT(int camID);
	void createMagnifyingLUT(int camID, XY center, float width, float height);
	void updateContours(int camID);

public:
	bool getFeaturePoints(int camID);
	void saveFeaturePoints(int camID);
	bool getFeaturePointsFromSetting(int camID);
	bool extractFeaturePointsFromDefisheye(int camID);

	std::vector<cv::Point> getSquareCenters(const Mat& img);
	bool postProcessSquareCenters(std::vector<cv::Point> centers /*in*/, std::vector<cv::Point>& sorted_centers /*out*/);
	std::vector<cv::Point> loadDefaultCenters();
};


#endif 
