#include "svmCalibCal.hpp"
#include "svmWorld.hpp"

sanCalibCal::sanCalibCal(sanXML* pxml, sanCamera* pcameras, sanCalibDefisheye* pdefisheyes, sanCalibContour* pcontours)
{
	m_pxml = pxml;
	m_pcameras = pcameras;
	m_pdefisheyes = pdefisheyes;
	m_pcontours = pcontours;
}

sanCalibCal::~sanCalibCal()
{
	if(!m_local_pts.empty()) m_local_pts.clear(); else noop;
	if(!m_local_npts.empty()) m_local_npts.clear(); else noop;
}

void sanCalibCal::initialize()
{
	try
	{
		getLocalPointsFromSetting();

		normalizeLocalPoints();
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1105101", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibCal::getLocalPointsFromSetting()
{
	if (!m_local_pts.empty()) m_local_pts.clear(); else noop;

	try
	{
		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			string str_camID = "(camID[" + to_string(camID) + "])";

			vector<Point3f> local_pts;

			if ((int)(sizeof(m_pxml->m_local_pts) / sizeof(*m_pxml->m_local_pts)) > camID)
			{
				XYZ* plocal = m_pxml->m_local_pts[camID];
				for (int i = 0; i < CONTROL_POINTS_NUM; i++)
					local_pts.push_back(Point3f(plocal[i].x, plocal[i].y, 0.0f));
				m_local_pts.push_back(local_pts);
			}
			else
				throw runtime_error(str_camID + delimiter(string("$camID out of range")));
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibCal::normalizeLocalPoints() // Normalization into [-1, 1] according template size
{
	if(!m_local_npts.empty()) m_local_npts.clear(); else noop;

	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		//int max_x = 0;
		//for (int i = 0; i < (int)m_local_pts[camID].size(); i++)
		//	max_x = max(max_x, (int)m_local_pts[camID][i].x);

		vector<Point3f> npts = sanWorld::get_full_Logic_InLOCAL(camID, m_pxml, m_local_pts[camID]);
		m_local_npts.push_back(npts);
	}
}

void sanCalibCal::calcExtrinsicParameters(int camID)
{
	vector<Point2f> feature_pts;
	for (int i = 0; i < CONTROL_POINTS_NUM; i++)
		feature_pts.push_back(Point2f(m_pcontours->m_feature_pts[camID][i].x, m_pcontours->m_feature_pts[camID][i].y));

	Mat matImgPoints(feature_pts);
	Mat matObjPoints(m_local_npts[camID]);

	Mat rvec, tvec;
	bool errcode = solvePnP(matObjPoints, matImgPoints, m_pcameras->getK(camID), m_pcameras->getDistCoeffs(camID), rvec, tvec);
	if (!errcode)
		throw runtime_error(__FUNCTION__ + string(" -> solvePnP() $failed to estimate an extrisic parameter"));
	else
	{
#if(0)
		if (camID == 0)
		{
			// convert current rvec into rotation matrix

			float r[3]; // rotation vector
			float R[9]; // rotation matrix

			r[0] = (float)rvec.at<double>(0, 0);
			r[1] = (float)rvec.at<double>(1, 0);
			r[2] = (float)rvec.at<double>(2, 0);

			CvMat _r = cvMat(3, 1, CV_32F, r);
			CvMat _R = cvMat(3, 3, CV_32F, R);

			sanProject::Rodrigues2(&_r, &_R, NULL);

			glm::mat4 Rot; // column major matrices mat[col][row]
			Rot[0][0] = R[0]; Rot[1][0] = R[1]; Rot[2][0] = R[2]; Rot[3][0] = 0.0f;
			Rot[0][1] = R[3]; Rot[1][1] = R[4]; Rot[2][1] = R[5]; Rot[3][1] = 0.0f;
			Rot[0][2] = R[6]; Rot[1][2] = R[7]; Rot[2][2] = R[8]; Rot[3][2] = 0.0f;
			Rot[0][3] = 0.0f; Rot[1][3] = 0.0f; Rot[2][3] = 0.0f; Rot[3][3] = 1.0f;

			// rotation
			cout << "front cam pos: [" << m_pcameras->m_camera_pos.x << " " << m_pcameras->m_camera_pos.y << " " << m_pcameras->m_camera_pos.z << "]"
				<< "; ori: [" << m_pcameras->m_camera_ori.x << " " << m_pcameras->m_camera_ori.y << " " << m_pcameras->m_camera_ori.z << "]" << endl;

			glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(m_pcameras->m_camera_ori.x), glm::vec3(1.0f, 0.0f, 0.0f));
			glm::mat4 RxRz = glm::rotate(Rx, glm::radians(m_pcameras->m_camera_ori.z), glm::vec3(0.0f, 0.0f, 1.0f));
			glm::mat4 RxRzRy = glm::rotate(RxRz, glm::radians(m_pcameras->m_camera_ori.y), glm::vec3(0.0f, 1.0f, 0.0f));

			// transform
			glm::mat4 newRot = RxRzRy * Rot;
			float r2[3];
			float R2[9];

			R2[0] = newRot[0][0]; R2[1] = newRot[1][0]; R2[2] = newRot[2][0];
			R2[3] = newRot[0][1]; R2[4] = newRot[1][1]; R2[5] = newRot[2][1];
			R2[6] = newRot[0][2]; R2[7] = newRot[1][2]; R2[8] = newRot[2][2];

			CvMat _r2 = cvMat(3, 1, CV_32F, r2);
			CvMat _R2 = cvMat(3, 3, CV_32F, R2);

			sanProject::Rodrigues2(&_R2, &_r2, NULL);

			rvec.at<double>(0, 0) = (double)r2[0];
			rvec.at<double>(1, 0) = (double)r2[1];
			rvec.at<double>(2, 0) = (double)r2[2];

			tvec.at<double>(0, 0) += (double)m_pcameras->m_camera_pos.x;
			tvec.at<double>(1, 0) += (double)m_pcameras->m_camera_pos.y;
			tvec.at<double>(2, 0) += (double)m_pcameras->m_camera_pos.z;
		}
		else noop;
#endif

		m_pcameras->setRvec(camID, rvec);
		m_pcameras->setTvec(camID, tvec);
	}
}

void sanCalibCal::updateCalibratedParameters(int camID)
{
	for (int i = 0; i < 9; i++)
	{
		m_pxml->m_calibrated_parameters[camID].K[i] = m_pcameras->m_camparams[camID].K.at<double>(i/3, i%3);
	}

	for (int i = 0; i < 3; i++)
	{
		m_pxml->m_calibrated_parameters[camID].ext[i] = m_pcameras->m_camparams[camID].tvec.at<double>(i, 0);
	}
	
	for (int i = 0; i < 3; i++)
	{
		m_pxml->m_calibrated_parameters[camID].ext[i+3] = m_pcameras->m_camparams[camID].rvec.at<double>(i, 0);
	}
}

void sanCalibCal::doCalibration(int camID)
{
	string str_camID = "(camID[" + to_string(camID) + "])";
	try
	{
		m_pcameras->calcIntrinsicParameters(camID);

		this->calcExtrinsicParameters(camID);

		this->updateCalibratedParameters(camID);
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1205101", __FUNCTION__ + str_camID + delimiter(string(e.what())));
	}

	logger.record_message(logger.svm_inform("--------", str_camID + "calibration step successful").what());
}

void sanCalibCal::showCalInfo()
{
	Scalar text_color(255, 0, 0);

	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		Mat defisheye_img;
		m_pdefisheyes->m_defisheye_images[camID].copyTo(defisheye_img);
		
		for (int i = 0; i < CONTROL_POINTS_NUM; i++)
		{
		    string index_str = to_mystring(i, 2);
		    if (1)
		    {
		        int x = (int)m_pcontours->m_feature_pts[camID][i].x;
		        int y = (int)m_pcontours->m_feature_pts[camID][i].y;
		        line(defisheye_img, Point(x - 1, y), Point(x + 1, y), Scalar(0, 0, 255), 1, LINE_4);
		        line(defisheye_img, Point(x, y - 1), Point(x, y + 1), Scalar(0, 0, 255), 1, LINE_4);
		
		        Point point((int)m_pcontours->m_feature_pts[camID][i].x, (int)m_pcontours->m_feature_pts[camID][i].y);
		        putText(defisheye_img, index_str, point, 2, 0.3, Scalar(255, 255, 255));
		    }
		
		    string feature_x_str = to_mystring(m_pcontours->m_feature_pts[camID][i].x, 1);
		    string feature_y_str = to_mystring(m_pcontours->m_feature_pts[camID][i].y, 1);
		    string feature_pts_string = "[" + index_str + "] = (" + feature_x_str + ", " + feature_y_str + ")";
		    Point feature_position(1400, 100 + 20 * i);
		    putText(defisheye_img, feature_pts_string, feature_position, 2, 0.5, text_color);
		
		    string local_x_str = to_mystring(m_local_pts[camID][i].x, 1);
		    string local_y_str = to_mystring(m_local_pts[camID][i].y, 1);
		    string local_z_str = to_mystring(m_local_pts[camID][i].z, 1);
		    string local_pts_string = "[" + index_str + "] = (" + local_x_str + ", " + local_y_str + ", " + local_z_str + ")";
		    Point local_position(1600, 100 + 20 * i);
		    putText(defisheye_img, local_pts_string, local_position, 2, 0.5, text_color);
		}
		string filepath = _PERSPECTIVE_PATH_ + "/defisheye" + to_string(camID) + ".bmp";
		imwrite(filepath, defisheye_img);
		
		imshow("Information about Calibration", defisheye_img);
		waitKey(0);
		destroyAllWindows();
		defisheye_img.release();
	}
}
