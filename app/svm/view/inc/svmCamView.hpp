#ifndef SVMCAMVIEW_HPP_
#define SVMCAMVIEW_HPP_

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmCamera.hpp"
#include "svmWorld.hpp"
#include "svmError.hpp"

class sanCamView: public sanVABT    // Canview 2D
{
public:
	sanXML* m_pxml;
	sanCamera* m_pcameras;
	PM* m_ppm;
	int m_square_size;
	sanShader m_camviewShader = sanShader(vs_view_c2d, NULL, fs_view_c2d);
	sanShader m_camviewSeamShader = sanShader(vs_view_primitive2d, NULL, fs_view_primitive2d);

public:
	sanCamView(sanXML* pxml, sanCamera* pcameras, PM* ppm);
	~sanCamView();

	void initialize(GLuint* ppitxID[], GLuint* ppmtxID[], GLuint* ppaitxID[]);
	void renderCamView(GLuint view_camID);
	void renderAdditionalCamView(GLuint acamID);
	void drawLayout1(bool updateLUT = false);

	GLfloat* createSeamLUT(int camID, bool flipx, vector<Point2f>& pt2d);
	void createDefisheyeLUT(RCAM_PARAMETERS& camparam, Mat& xmap, Mat& ymap, int square_size, GLfloat** vertices_/*[out]*/, int* vertices_num_/*[out]*/);

	void update_RCamViewLUT(int rcamID);
	void update_ACamViewLUT(int acamID);
};

#endif



