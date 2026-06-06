#ifndef _SVM_CALIB_MASK_HPP_
#define _SVM_CALIB_MASK_HPP_

#include "svmCore.hpp"
#include "svmCamera.hpp"
#include "svmCalibGrid.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmLogger.hpp"


#define LEFT_SIDE			(-1)
#define RIGHT_SIDE			(1)
#define SEAM_STEP	  		(0.01f)


struct Line 		// Line description y = alpha * x + beta
{
	double alpha;
	double beta;
};

struct SEAM_LINE
{
	Line line;		// a straint line through ip1 and ip2
	Point2f ip1;	// bottom intersection point
	Point2f ip2;	// top intersection point
};


class sanCalibMask: public sanVABT
{
public:
	sanXML* m_pxml;
	sanCamera* m_pcameras;
	sanCalibGrid* m_pgrids;
	sanShader m_maskShader = sanShader(vs_calib_mask, NULL, fs_calib_mask);

public:
	sanCalibMask(sanXML* pxml, sanCamera* pcameras, sanCalibGrid* pgrids);
	~sanCalibMask();
	void initialize();
	void createMasks();
	void saveMasks();
	void updateMasks();
	void renderMasks(int camID);

public:		
	vector<Mat> m_masks;
	vector<SEAM_LINE> m_left_seam_lines; 
	vector<SEAM_LINE> m_right_seam_lines; 
	
	Point2f getIntersection(Line line1, Line line2);
	Line getAlphaBeta(Point2f p1, Point2f p2);
	void smoothMaskEdge(Mat &img, Vec2b colors, Vec2d angles, double angle_step, vector<Point3f> edge_points, int camID);
	void getSeamLine(vector<Point3f> &polygon1, vector<Point3f> &polygon2, int left_or_right, SEAM_LINE& seam_line /*[out]*/);

	float m_maskLUT[30] = 
		{ -1.0f, 1.0f, 0.0f, 0.0f, 0.0f,  // GL vertex(x, y, z),  normalized image(u, v)
		  -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
		   1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
		   1.0f, 1.0f, 0.0f, 1.0f, 0.0f,
		  -1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
		   1.0f, -1.0f, 0.0f, 1.0f, 1.0f };
};


#endif 
