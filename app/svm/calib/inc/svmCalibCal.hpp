#ifndef _SVM_CALIB_CAL_HPP_
#define _SVM_CALIB_CAL_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmXML.hpp"
#include "svmCamera.hpp"
#include "svmCalibContour.hpp"


class sanCalibCal
{
public:
	sanXML* m_pxml;
	sanCamera* m_pcameras;
	sanCalibDefisheye* m_pdefisheyes;
	sanCalibContour* m_pcontours;

	vector<vector<Point3f>> m_local_pts;
	vector<vector<Point3f>> m_local_npts; // normalized local points

public:
	sanCalibCal(sanXML* pxml, sanCamera* pcameras, sanCalibDefisheye* pdefisheyes, sanCalibContour *pcontours);
	~sanCalibCal();
	void initialize();
	void doCalibration(int camID);
	void calcExtrinsicParameters(int camID);
	void updateCalibratedParameters(int camID);

	void getLocalPointsFromSetting();
	void normalizeLocalPoints();
	void showCalInfo(); // for debugging
};


#endif 
