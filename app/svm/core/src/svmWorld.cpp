#include "svmWorld.hpp"
#include "svmProject.hpp"


sanWorld::sanWorld()
{
}

sanWorld::~sanWorld()
{
}

void sanWorld::initialize()
{
}

Point3f sanWorld::LOCAL_to_GLOBAL(int camID, Point3f local_pt3d)
{
	// Grids are on the half circle in the Y-Z- space based on OpenCV coordinates
	Point3f global_pt3d(0.0f, 0.0f, 0.0f);

	switch (camID) //openCV, Z-axis rotation, object rotation
	{
	case CAMID_FRONT: // no rotation
		global_pt3d.x = local_pt3d.x;   // [ 1   0   0  ]   [ cos(0)    -sin(0)      0 ]
		global_pt3d.y = local_pt3d.y;   // [ 0   1   0  ] = [ sin(0)     cos(0)      0 ] 
		global_pt3d.z = local_pt3d.z;   // [ 0   0   1  ]   [   0          0         1 ] 
		break;
	case CAMID_RIGHT:  // Z-axis based 90 rotation(point rotation)
		global_pt3d.x = -local_pt3d.y;  // [ 0  -1   0  ]   [ cos(90)   -sin(90)     0 ]
		global_pt3d.y =  local_pt3d.x;  // [ 1   0   0  ] = [ sin(90)    cos(90)     0 ] 
		global_pt3d.z =  local_pt3d.z;  // [ 0   0   1  ]   [   0          0         1 ]   
		break;
	case CAMID_REAR:  // Z-axis based 180 rotation(point rotation)
		global_pt3d.x = -local_pt3d.x;  // [ -1  0   0  ]   [ cos(180)   -sin(180)   0 ]
		global_pt3d.y = -local_pt3d.y;  // [ 0  -1   0  ] = [ sin(180)    cos(180)   0 ] 
		global_pt3d.z =  local_pt3d.z;  // [ 0   0   1  ]   [     0           0      1 ] 
		break;
	case CAMID_LEFT: // Z-axis based -90 rotation(point rotation)
		global_pt3d.x =  local_pt3d.y;  // [ 0   1   0  ]   [ cos(-90)   -sin(-90)   0 ]
		global_pt3d.y = -local_pt3d.x;  // [-1   0   0  ] = [ sin(-90)    cos(-90)   0 ] 
		global_pt3d.z =  local_pt3d.z;  // [ 0   0   1  ]   [   0             0      1 ] 
		break;
	default:
		string str_camID = "(camID[" + to_string(camID) + "])";
		throw runtime_error(str_camID + delimiter(string("$camID out of range")));
		break;
	}

	return global_pt3d;
}

Point3f sanWorld::GLOBAL_to_LOCAL(int camID, Point3f global_pt3d)
{
	Point3f local_pt3d(0.0f, 0.0f, 0.0f);
	

	switch (camID) //openCV, Z-axis rotation, object rotation
	{
	case CAMID_FRONT: // no rotation
		local_pt3d.x = global_pt3d.x;  // [ 1   0   0  ]   [ cos(0)    -sin(0)      0 ]
		local_pt3d.y = global_pt3d.y;  // [ 0   1   0  ] = [ sin(0)     cos(0)      0 ]
		local_pt3d.z = global_pt3d.z;  // [ 0   0   1  ]   [   0          0         1 ]
		break;
	case CAMID_RIGHT: // Z-axis based -90 rotation(point rotation)
		local_pt3d.x = global_pt3d.y;  // [ 0   1   0  ]   [ cos(-90)   -sin(-90)   0 ]
		local_pt3d.y = -global_pt3d.x; // [-1   0   0  ] = [ sin(-90)    cos(-90)   0 ] 
		local_pt3d.z = global_pt3d.z;  // [ 0   0   1  ]   [    0           0       1 ]   
		break;
	case CAMID_REAR:  // Z-axis based -180 rotation(point rotation)
		local_pt3d.x = -global_pt3d.x; // [ -1  0   0  ]   [ cos(180)   -sin(180)   0 ]
		local_pt3d.y = -global_pt3d.y; // [ 0  -1   0  ] = [ sin(180)    cos(180)   0 ]
		local_pt3d.z = global_pt3d.z;  // [ 0   0   1  ]   [     0           0      1 ]
		break;
	case CAMID_LEFT: // Z-axis based 90 rotation(point rotation)
		local_pt3d.x = -global_pt3d.y; // [ 0  -1   0  ]   [ cos(90)    -sin(90)    0 ]
		local_pt3d.y = global_pt3d.x;  // [ 1   0   0  ] = [ sin(90)     cos(90)    0 ]
		local_pt3d.z = global_pt3d.z;  // [ 0   0   1  ]   [   0             0      1 ]
		break;
	default:
		string str_camID = "(camID[" + to_string(camID) + "])";
		throw runtime_error(str_camID + delimiter(string("$camID out of range")));
		break;
	}

	return local_pt3d;
}


Point3f sanWorld::GLOBAL_to_GL(Point3f pt3d)
{
	return Point3f(pt3d.x, -pt3d.y, -pt3d.z);
}


Point3f sanWorld::GL_to_GLOBAL(Point3f pt3d)
{
	return Point3f(pt3d.x, -pt3d.y, -pt3d.z);
}

vector<Point3f> sanWorld::LOCAL_to_GL(int camID, vector<Point3f>& local_pt3d /* logic or mm */)
{
	try
	{
		vector<Point3f> global_pt3d;

		for (int i = 0; i < (int)local_pt3d.size(); i++)
			global_pt3d.push_back(GLOBAL_to_GL(LOCAL_to_GLOBAL(camID, local_pt3d[i])));

		return global_pt3d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

vector<Point3f> sanWorld::GL_to_LOCAL(int camID, vector<Point3f>& global_pt3d /* logic or mm*/)
{
	try
	{
		vector<Point3f> local_pt3d;

		for (int i = 0; i < (int)global_pt3d.size(); i++)
			local_pt3d.push_back(GLOBAL_to_LOCAL(camID, GL_to_GLOBAL(global_pt3d[i])));

		return local_pt3d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


vector<Point3f> sanWorld::CV2GL(vector<Point3f>& cv_points)
{
	vector<Point3f> gl_points;

	for (auto& iter : cv_points)
		gl_points.push_back(Point3f(iter.x, -iter.y, -iter.z));

	return gl_points;
}


vector<Point2f> sanWorld::CV2GL(vector<Point2f>& cv_points)
{
	vector<Point2f> gl_points;

	for (auto& iter : cv_points)
		gl_points.push_back(Point2f(iter.x, -iter.y));

	return gl_points;
}


vector<Point2f> sanWorld::GL_to_NDC(glm::mat4 vp /* view_matrix and projection_matrix */, vector<Point3f>& gl_p3d)
{
	vector<Point2f> NDC;

	for (int i = 0; i < (int)gl_p3d.size(); i++)
	{
		//GL space to clip space
		glm::vec4 clip_space = vp * glm::vec4(gl_p3d[i].x, gl_p3d[i].y, gl_p3d[i].z, 1.0f);

		// division projection
		float ndc_x = clip_space[0] / (-1.0f * clip_space[2]); // multiplied by -1.0 due to left-hand coordinate system
		float ndc_y = clip_space[1] / (-1.0f * clip_space[2]);

		NDC.push_back(Point2f(ndc_x, ndc_y));
	}

	return NDC;
}


vector<Point3f> sanWorld::get_Logic_InLOCAL(sanXML* pxml, vector<Point3f>& pt3d_mm)
{
	vector<Point3f> pt3d;
	for (int i = 0; i < (int)pt3d_mm.size(); i++)
	{
		float x = pt3d_mm[i].x / ((float)pxml->m_arrangement.poster.width / 2.0f);
		float y = pt3d_mm[i].y / ((float)pxml->m_arrangement.poster.width / 2.0f);
		float z = pt3d_mm[i].z / ((float)pxml->m_arrangement.poster.width / 2.0f);

		pt3d.push_back(Point3f(x, y, z));
	}
	return pt3d;
}


vector<Point3f> sanWorld::get_mm_InLOCAL(sanXML* pxml, vector<Point3f>& pt3d)
{
	vector<Point3f> pt3d_mm;
	for (int i = 0; i < (int)pt3d.size(); i++)
	{
		float x_mm = pt3d[i].x * ((float)pxml->m_arrangement.poster.width / 2.0f);
		float y_mm = pt3d[i].y * ((float)pxml->m_arrangement.poster.width / 2.0f);

		pt3d_mm.push_back(Point3f(x_mm, y_mm, 0.0f));
	}

	return pt3d_mm;
}


vector<Point3f> sanWorld::get_Logic_InGLOBAL(sanXML* pxml, vector<Point3f>& pt3d_mm)
{
	vector<Point3f> pt3d;
	for (int i = 0; i < (int)pt3d_mm.size(); i++)
	{
		float x = pt3d_mm[i].x / ((float)pxml->m_arrangement.poster.width / 2.0f);
		float y = pt3d_mm[i].y / ((float)pxml->m_arrangement.poster.width / 2.0f);
		float z = pt3d_mm[i].z / ((float)pxml->m_arrangement.poster.width / 2.0f);

		pt3d.push_back(Point3f(x, y, z));
	}
	return pt3d;
}


vector<Point3f> sanWorld::get_mm_InGLOBAL(sanXML* pxml, vector<Point3f>& pt3d)
{
	vector<Point3f> pt3d_mm;
	for (int i = 0; i < (int)pt3d.size(); i++)
	{
		float x = pt3d[i].x * ((float)pxml->m_arrangement.poster.width / 2.0f);
		float y = pt3d[i].y * ((float)pxml->m_arrangement.poster.width / 2.0f);
		float z = pt3d[i].z * ((float)pxml->m_arrangement.poster.width / 2.0f);

		pt3d_mm.push_back(Point3f(x, y, z));
	}
	return pt3d_mm;
}


vector<Point3f> sanWorld::get_full_Logic_InLOCAL(int camID, sanXML* pxml, vector<Point3f>& pt3d_mm) // MM: MilliMeter, Logic: no unit due to normaization  (-1 ~ 1)
{
	/* pt3d_mm is in Pattern space of current camID */

	try
	{
		Size pattern_space = get_pattern_arrangement_space(camID, pxml); // note XY axies.

		if (1.0f < pxml->m_arrangement.poster.width)
		{
			vector<Point3f> p3d;
			for (int i = 0; i < (int)pt3d_mm.size(); i++)  // real coordinate (0, pisitive number)  --> betwwen -1 and 1
			{
				float x = (2.0f * pt3d_mm[i].x - (float)pattern_space.width) / (float)pxml->m_arrangement.poster.width;
				float y = (2.0f * pt3d_mm[i].y - (float)pattern_space.height) / (float)pxml->m_arrangement.poster.width;

				p3d.push_back(Point3f(x, y, 0.0f));
			}

			return p3d;
		}
		else
		{
			string msg = string("$poster width(") + to_string(pxml->m_arrangement.poster.width) + string("] wrong");
			throw runtime_error(msg);
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


vector<Point3f> sanWorld::defisheye_to_LOCAL(int camID, sanXML* pxml, vector<Point2f>& defisheye_p2d)
{
	try
	{
		CALIBRATED_PARAMETER* calibed_param = &pxml->m_calibrated_parameters[camID];
		Mat invK = Mat(3, 3, CV_64F, calibed_param->K).inv(); // camera matrix estimated newly;

		//////////////////// Homography //////////////////////////////////////////
		 // 3x3 Rotation matrix from Rodriguess rotation vector
		CvMat rvec = cvMat(3, 1, CV_64F, &calibed_param->ext[3]);
		double Rwc_buf[9] = { 0.0, };
		CvMat Rwc = cvMat(3, 3, CV_64F, Rwc_buf);
		sanProject::Rodrigues2(&rvec, &Rwc, NULL);

		double Homogrpahy[9] = { Rwc.data.db[0], Rwc.data.db[1], calibed_param->ext[0],
								 Rwc.data.db[3], Rwc.data.db[4], calibed_param->ext[1],
								 Rwc.data.db[6], Rwc.data.db[7], calibed_param->ext[2] };

		Mat invH = Mat(3, 3, CV_64F, Homogrpahy).inv();

		double inv_homography_last_factor = invH.at<double>(2, 2);
		if (DBL_EPSILON < fabs(inv_homography_last_factor))
			invH = invH / inv_homography_last_factor;
		else
			invH.at<double>(2, 2) = 1.0;
		//////////////////////////////////////////////////////////////////////////

		vector<Point3f> local_pt3d;

		float circle_radius = calBaseRadius(pxml) * pxml->m_arrangement.poster.radius_scale;

		for (int i = 0; i < (int)defisheye_p2d.size(); i++)
		{
			Point3d defisheye_p3d(defisheye_p2d[i].x, defisheye_p2d[i].y, 1.0);

			// apply intrinsic and extrinsic parameters
			Mat localV = invH * invK * Mat(3, 1, CV_64F, &defisheye_p3d);

			//point on the ideal plane(z=0), front-based, OpenCV
			double nX = localV.at<double>(0, 0);
			double nY = localV.at<double>(1, 0);
			double nZ = localV.at<double>(2, 0);
			// if (0.0 < nZ) // when it's below the vanishing line
			{
				nX = nX / nZ;
				nY = nY / nZ;
				nZ = 0.0;
			}
			// else if (nZ < 0.0) //when it's above the vanishing line
			// {
			// 	nX = -1.0 * nX / nZ; // 180 degree rotation around axis-Z
			// 	nY = -1.0 * nY / nZ;
			// 	nZ = 0.0;
			// }
			// else noop;  //when it's on the vanishing line		

			float current_distance = (float)sqrt(nX * nX + nY * nY);
			float delta_distance = (current_distance - circle_radius);
			if (0 < delta_distance)  // on the parabolic curve
			{
				double theta = atan2(nY, nX);
				double new_delta_distance = sqrt(delta_distance);
				double new_distance = circle_radius + new_delta_distance;
				nX = new_distance * cos(theta);
				nY = new_distance * sin(theta);
				nZ = 0.0;
			}
			else noop;

			local_pt3d.push_back(Point3f((float)nX, (float)nY, (float)nZ));
		}

		return local_pt3d;  // based on OpenCV coordinate
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


Point2f sanWorld::fisheye_to_defisheye(int camID, sanXML* pxml, Point2f& fisheye_p2d)
{
	try
	{
		RCAM_PARAMETERS rcam = pxml->m_rcam[camID];
		Point3f defisheye;
		sanCamera::cam2world(rcam, fisheye_p2d, defisheye);
		defisheye = (defisheye / defisheye.z) * (-pxml->m_resolution.image.width / rcam.sf);

		return Point2f(defisheye.x + (float)rcam.cx, defisheye.y + (float)rcam.cy);
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


vector<Point2f> sanWorld::fisheye_to_defisheye(int camID, sanXML* pxml, vector<Point2f>& fisheye_p2d)
{
	vector<Point2f> defisheye_p2d;
	for (int i = 0; i < (int)fisheye_p2d.size(); i++)
	{
		defisheye_p2d.push_back(fisheye_to_defisheye(camID, pxml, fisheye_p2d[i]));
	}

	return defisheye_p2d;
}


vector<Point3f> sanWorld::fisheye_to_LOCAL(int camID, sanXML* pxml, vector<Point2f>& fisheye_p2d) // note it uses OpenCV coordinates
{
	try
	{
		vector<Point2f> defisheye_pt2d = fisheye_to_defisheye(camID, pxml, fisheye_p2d);
		vector<Point3f> local_pt3d = defisheye_to_LOCAL(camID, pxml, defisheye_pt2d);
		return local_pt3d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


#if(0)
vector<Point3f> sanWorld::fisheye_to_GL(int camID, sanXML* pxml, vector<Point2f>& fisheye_p2d)  // for 3D drawing
{	
	try
	{
		vector<Point3f> local_pt3d = fisheye_to_LOCAL(camID, pxml, fisheye_p2d);
		vector<Point3f> global_pt3d = LOCAL_to_GL(camID, local_pt3d);

		return global_pt3d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}
#endif

vector<Point3f> sanWorld::fisheye_to_GL_mm(int camID, sanXML* pxml, vector<Point2f>& fisheye_p2d) // for warning
{	
	try
	{
		vector<Point3f> local_pt3d = fisheye_to_LOCAL(camID, pxml, fisheye_p2d);		
		vector<Point3f> global_pt3d = LOCAL_to_GL(camID, local_pt3d);
		vector<Point3f> global_pt3d_mm = get_mm_InGLOBAL(pxml, global_pt3d);
		
		vector<Point3f> vehicle_box_mm = get_vehicle_box_InGLOBAL(pxml, VEHICLE_BOX_UNIT_MM);
		vector<Point3f> vehicle_zone = { vehicle_box_mm[3], vehicle_box_mm[2], vehicle_box_mm[0], vehicle_box_mm[1] };
		vector<Point3f> new_global_pt3d_mm;

		for (int i = 0; i < (int)global_pt3d_mm.size(); i++)
		{
			float new_x = global_pt3d_mm[i].x;
			float new_y = global_pt3d_mm[i].y;
			float new_z = global_pt3d_mm[i].z;

			if (sanWorld::isInsideZone(vehicle_zone, Point3f(new_x, new_y, new_z))) // for thresholding geater values than the dimension of a car
			{
				switch (camID)
				{
				case CAMID_FRONT:
					new_y = max(new_y, vehicle_zone[2].y);
					break;
				case CAMID_RIGHT:
					new_x = max(new_x, vehicle_zone[1].x);
					break;
				case CAMID_REAR:
					new_y = min(new_y, vehicle_zone[0].y);
					break;
				case CAMID_LEFT:
					new_x = min(new_x, vehicle_zone[3].x);
					break;
				default:
					throw runtime_error(" $camID wrong");
					break;
				}
			}
			else noop;

			new_global_pt3d_mm.push_back(Point3f(new_x, new_y, new_z));
		}

		return new_global_pt3d_mm;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


vector<Point2f> sanWorld::LOCAL_to_defisheye(int camID, sanXML* pxml, vector<Point3f>& local_p3d) // note it uses OpenCV coordinates
{
	try
	{
		CALIBRATED_PARAMETER* caled_param = &pxml->m_calibrated_parameters[camID];

		// local -> defisheye
		Mat tvec(3, 1, CV_64F, &caled_param->ext[0]); // Tc : translation
		Mat rvec = Mat(3, 1, CV_64F, &caled_param->ext[3]); // direction
		Mat K(3, 3, CV_64F, caled_param->K);
		Mat distCoeffs = Mat(4, 1, CV_64F, Scalar(0.0f));
		vector<Point2f> defisheye_p2d;
		sanProject::projectPoints(local_p3d, rvec, tvec, K, distCoeffs, defisheye_p2d); // return points based on the top-left (0,0)

		return defisheye_p2d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


Point2f sanWorld::defisheye_to_fisheye(int camID, sanXML* pxml, Point2f& defisheye_p2d) // note it uses OpenCV coordinates
{
	RCAM_PARAMETERS rcam = pxml->m_rcam[camID];
	Point3f defisheye = Point3f(defisheye_p2d.x - (float)rcam.cx, defisheye_p2d.y - (float)rcam.cy, (float)(-pxml->m_resolution.image.width / rcam.sf));
	Point2f fisheye;
	sanCamera::world2cam(rcam, defisheye, fisheye);
	return fisheye;
}


vector<Point2f> sanWorld::defisheye_to_fisheye(int camID, sanXML* pxml, vector<Point2f>& defisheye_p2d)
{
	vector<Point2f> fisheye_p2d;
	for (int i = 0; i < (int)defisheye_p2d.size(); i++)
	{
		fisheye_p2d.push_back(defisheye_to_fisheye(camID, pxml, defisheye_p2d[i]));
	}

	return fisheye_p2d;
}

vector<Point2f> sanWorld::LOCAL_to_fisheye(int camID, sanXML* pxml, vector<Point3f>& local_p3d) // note it uses OpenCV coordinates
{
	try
	{
		vector<Point2f> defisheye_p2d = LOCAL_to_defisheye(camID, pxml, local_p3d);
		vector<Point2f> fisheye_p2d = defisheye_to_fisheye(camID, pxml, defisheye_p2d);

		return fisheye_p2d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


vector<Point2f> sanWorld::GL_mm_to_fisheye(int camID, sanXML* pxml, vector<Point3f>& global_p3d_mm) // for PGS
{
	try
	{
		vector<Point3f> local_mm = GL_to_LOCAL(camID, global_p3d_mm);
		vector<Point3f> local_p3d = get_Logic_InLOCAL(pxml, local_mm);
		vector<Point2f> fisheye_p2d = LOCAL_to_fisheye(camID, pxml, local_p3d);

		return fisheye_p2d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


vector<Point2f> sanWorld::GL_mm_to_defisheye(int camID, sanXML* pxml, vector<Point3f>& global_p3d_mm) // for PGS
{
	try
	{
		vector<Point3f> local_mm = GL_to_LOCAL(camID, global_p3d_mm);
		vector<Point3f> local_p3d = get_Logic_InLOCAL(pxml, local_mm);
		vector<Point2f> defisheye_p2d = LOCAL_to_defisheye(camID, pxml, local_p3d);

		return defisheye_p2d;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


Point2f sanWorld::normalize_points_in_defisheye(int camID, sanXML* pxml, Point2f defisheye_pt2d)
{
	float defisheye_offset_x_left = pxml->m_rcam[camID].camview_offset.hleft;
	float defisheye_offset_x_right = pxml->m_rcam[camID].camview_offset.hright;
	float defisheye_offset_y_top = pxml->m_rcam[camID].camview_offset.vtop;
	float defisheye_offset_y_bot = pxml->m_rcam[camID].camview_offset.vbot;

	float img_width = pxml->m_resolution.image.width;
	float img_height = pxml->m_resolution.image.height;

	float x_denumerator = (img_width - (defisheye_offset_x_left + defisheye_offset_x_right));
	float y_denumerator = (img_height - (defisheye_offset_y_top + defisheye_offset_y_bot));

	Point2f pt2d;

	if ((fabs(x_denumerator) <= FLT_EPSILON) || (fabs(y_denumerator) <= FLT_EPSILON))
	{
		string msg = "$denumerator is zero";
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
	else
	{
		float xnorm = 1.0f / x_denumerator;
		float ynorm = 1.0f / y_denumerator;


		pt2d.x = ((defisheye_pt2d.x - defisheye_offset_x_left) * xnorm - 0.500f) * 2.0f;
		pt2d.y = ((defisheye_pt2d.y - defisheye_offset_y_top) * ynorm - 0.500f) * 2.0f;
	}

	return pt2d;
}

vector<Point2f> sanWorld::normalize_points_in_defisheye(int camID, sanXML* pxml, vector<Point2f>& defisheye_pt2d)
{
	vector<Point2f> normalized_pt2d;
	for (int i = 0; i < (int)defisheye_pt2d.size(); i++)
	{
		normalized_pt2d.push_back(normalize_points_in_defisheye(camID, pxml, defisheye_pt2d[i]));
	}

	return normalized_pt2d;
}


float sanWorld::calBaseRadius(sanXML* pxml)
{
	return (float) sqrt(1.0f + pow(pxml->m_arrangement.poster.height / pxml->m_arrangement.poster.width, 2));
}


Size sanWorld::get_pattern_arrangement_space(int camID, sanXML* pxml)
{
	Size pattern_space(0, 0);

	switch (camID)
	{
	case CAMID_FRONT:
	case CAMID_REAR:
		pattern_space = Size((int)pxml->m_arrangement.poster.width, (int)pxml->m_arrangement.poster.height);
		break;
	case CAMID_RIGHT:
	case CAMID_LEFT:
		pattern_space = Size((int)pxml->m_arrangement.poster.height, (int)pxml->m_arrangement.poster.width);
		break;
	default:
		string str_camID = "(camID[" + to_string(camID) + "])";
		throw runtime_error(str_camID + delimiter(string("$camID out of range")));
		break;
	}
	return pattern_space;
}

// world coordiantes of real patterns
void sanWorld::generate_real_pattern_point_mm_in_GLOBAL(sanXML* pxml, XYZ global_pt[PATTERN_MAX_NUM][PATTERN_POINTS_NUM] /*[out]*/)
{
	float top_left_x = (-1.0f / 2.0f) * pxml->m_arrangement.poster.width;
	float top_left_y = (-1.0f / 2.0f) * pxml->m_arrangement.poster.height;

	if(pxml->m_svm_patterns_num == 4)
	{
		// pattern #1
		global_pt[0][0].x = top_left_x + 2 * pxml->m_pattern.unit_space + pxml->m_arrangement.pattern_offset_from_car.left_pattern_offset;
		global_pt[0][0].y = top_left_y + 2 * pxml->m_pattern.unit_space;
		global_pt[0][0].z = 0.0f;

		global_pt[0][1].x = global_pt[0][0].x;
		global_pt[0][1].y = global_pt[0][0].y - pxml->m_pattern.unit_space;
		global_pt[0][1].z = 0.0f;

		global_pt[0][2].x = global_pt[0][1].x;
		global_pt[0][2].y = global_pt[0][1].y - pxml->m_pattern.unit_space;
		global_pt[0][2].z = 0.0f;

		// pattern #2
		global_pt[1][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space - pxml->m_arrangement.pattern_offset_from_car.right_pattern_offset;
		global_pt[1][0].y = top_left_y + 2 * pxml->m_pattern.unit_space;
		global_pt[1][0].z = 0.0f;

		global_pt[1][1].x = global_pt[1][0].x;
		global_pt[1][1].y = global_pt[1][0].y - pxml->m_pattern.unit_space;
		global_pt[1][1].z = 0.0f;

		global_pt[1][2].x = global_pt[1][1].x;
		global_pt[1][2].y = global_pt[1][1].y - pxml->m_pattern.unit_space;
		global_pt[1][2].z = 0.0f;

		// pattern #3
		global_pt[2][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space;
		global_pt[2][0].y = top_left_y + 2 * pxml->m_pattern.unit_space + pxml->m_arrangement.pattern_offset_from_car.front_pattern_offset;
		global_pt[2][0].z = 0.0f;

		global_pt[2][1].x = global_pt[2][0].x + pxml->m_pattern.unit_space;
		global_pt[2][1].y = global_pt[2][0].y;
		global_pt[2][1].z = 0.0f;

		global_pt[2][2].x = global_pt[2][1].x + pxml->m_pattern.unit_space;
		global_pt[2][2].y = global_pt[2][1].y;
		global_pt[2][2].z = 0.0f;

		// pattern #4
		global_pt[3][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space;
		global_pt[3][0].y = global_pt[2][0].y + pxml->m_arrangement.distance_between_patterns.vertical_1st_distance;
		global_pt[3][0].z = 0.0f;

		global_pt[3][1].x = global_pt[3][0].x + pxml->m_pattern.unit_space;
		global_pt[3][1].y = global_pt[3][0].y;
		global_pt[3][1].z = 0.0f;

		global_pt[3][2].x = global_pt[3][1].x + pxml->m_pattern.unit_space;
		global_pt[3][2].y = global_pt[3][1].y;
		global_pt[3][2].z = 0.0f;

		// pattern #2'
		global_pt[4][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space - pxml->m_arrangement.pattern_offset_from_car.right_pattern_offset;
		global_pt[4][0].y = top_left_y + pxml->m_arrangement.poster.height - 2 * pxml->m_pattern.unit_space;
		global_pt[4][0].z = 0.0f;

		global_pt[4][1].x = global_pt[4][0].x;
		global_pt[4][1].y = global_pt[4][0].y + pxml->m_pattern.unit_space;
		global_pt[4][1].z = 0.0f;

		global_pt[4][2].x = global_pt[4][1].x;
		global_pt[4][2].y = global_pt[4][1].y + pxml->m_pattern.unit_space;
		global_pt[4][2].z = 0.0f;

		// pattern #1'
		global_pt[5][0].x = top_left_x + 2 * pxml->m_pattern.unit_space + pxml->m_arrangement.pattern_offset_from_car.left_pattern_offset;
		global_pt[5][0].y = top_left_y + pxml->m_arrangement.poster.height - 2 * pxml->m_pattern.unit_space;
		global_pt[5][0].z = 0.0f;

		global_pt[5][1].x = global_pt[5][0].x;
		global_pt[5][1].y = global_pt[5][0].y + pxml->m_pattern.unit_space;
		global_pt[5][1].z = 0.0f;

		global_pt[5][2].x = global_pt[5][1].x;
		global_pt[5][2].y = global_pt[5][1].y + pxml->m_pattern.unit_space;
		global_pt[5][2].z = 0.0f;

		// pattern #4'
		global_pt[6][0].x = top_left_x + 2 * pxml->m_pattern.unit_space;
		global_pt[6][0].y = top_left_y + pxml->m_arrangement.poster.height - 2 * pxml->m_pattern.unit_space - pxml->m_arrangement.pattern_offset_from_car.rear_pattern_offset;
		global_pt[6][0].z = 0.0f;

		global_pt[6][1].x = global_pt[6][0].x - pxml->m_pattern.unit_space;
		global_pt[6][1].y = global_pt[6][0].y;
		global_pt[6][1].z = 0.0f;

		global_pt[6][2].x = global_pt[6][1].x - pxml->m_pattern.unit_space;
		global_pt[6][2].y = global_pt[6][1].y;
		global_pt[6][2].z = 0.0f;

		// pattern #3'
		global_pt[7][0].x = top_left_x + 2 * pxml->m_pattern.unit_space;
		global_pt[7][0].y = global_pt[6][0].y - pxml->m_arrangement.distance_between_patterns.vertical_1st_distance;
		global_pt[7][0].z = 0.0f;

		global_pt[7][1].x = global_pt[7][0].x - pxml->m_pattern.unit_space;
		global_pt[7][1].y = global_pt[7][0].y;
		global_pt[7][1].z = 0.0f;

		global_pt[7][2].x = global_pt[7][1].x - pxml->m_pattern.unit_space;
		global_pt[7][2].y = global_pt[7][1].y;
		global_pt[7][2].z = 0.0f;
	}
	else if(pxml->m_svm_patterns_num == 6)
	{
		// pattern #1
		global_pt[0][0].x = top_left_x + 2 * pxml->m_pattern.unit_space + pxml->m_arrangement.pattern_offset_from_car.left_pattern_offset;
		global_pt[0][0].y = top_left_y + 2 * pxml->m_pattern.unit_space;
		global_pt[0][0].z = 0.0f;

		global_pt[0][1].x = global_pt[0][0].x;
		global_pt[0][1].y = global_pt[0][0].y - pxml->m_pattern.unit_space;
		global_pt[0][1].z = 0.0f;

		global_pt[0][2].x = global_pt[0][1].x;
		global_pt[0][2].y = global_pt[0][1].y - pxml->m_pattern.unit_space;
		global_pt[0][2].z = 0.0f;

		// pattern #2
		global_pt[1][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space - pxml->m_arrangement.pattern_offset_from_car.right_pattern_offset;
		global_pt[1][0].y = top_left_y + 2 * pxml->m_pattern.unit_space;
		global_pt[1][0].z = 0.0f;

		global_pt[1][1].x = global_pt[1][0].x;
		global_pt[1][1].y = global_pt[1][0].y - pxml->m_pattern.unit_space;
		global_pt[1][1].z = 0.0f;

		global_pt[1][2].x = global_pt[1][1].x;
		global_pt[1][2].y = global_pt[1][1].y - pxml->m_pattern.unit_space;
		global_pt[1][2].z = 0.0f;

		// pattern #3
		global_pt[2][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space;
		global_pt[2][0].y = top_left_y + 2 * pxml->m_pattern.unit_space + pxml->m_arrangement.pattern_offset_from_car.front_pattern_offset;
		global_pt[2][0].z = 0.0f;

		global_pt[2][1].x = global_pt[2][0].x + pxml->m_pattern.unit_space;
		global_pt[2][1].y = global_pt[2][0].y;
		global_pt[2][1].z = 0.0f;

		global_pt[2][2].x = global_pt[2][1].x + pxml->m_pattern.unit_space;
		global_pt[2][2].y = global_pt[2][1].y;
		global_pt[2][2].z = 0.0f;

		// pattern #4
		global_pt[3][0].x = global_pt[2][0].x;
		global_pt[3][0].y = global_pt[2][0].y + pxml->m_arrangement.distance_between_patterns.vertical_1st_distance;
		global_pt[3][0].z = 0.0f;

		global_pt[3][1].x = global_pt[3][0].x + pxml->m_pattern.unit_space;
		global_pt[3][1].y = global_pt[3][0].y;
		global_pt[3][1].z = 0.0f;

		global_pt[3][2].x = global_pt[3][1].x + pxml->m_pattern.unit_space;
		global_pt[3][2].y = global_pt[3][1].y;
		global_pt[3][2].z = 0.0f;

		// pattern #5
		global_pt[4][0].x = global_pt[3][0].x;
		global_pt[4][0].y = global_pt[3][0].y + pxml->m_arrangement.distance_between_patterns.vertical_2nd_distance;
		global_pt[4][0].z = 0.0f;

		global_pt[4][1].x = global_pt[4][0].x + pxml->m_pattern.unit_space;
		global_pt[4][1].y = global_pt[4][0].y;
		global_pt[4][1].z = 0.0f;

		global_pt[4][2].x = global_pt[4][1].x + pxml->m_pattern.unit_space;
		global_pt[4][2].y = global_pt[4][1].y;
		global_pt[4][2].z = 0.0f;

		// pattern #6
		global_pt[5][0].x = global_pt[4][0].x;
		global_pt[5][0].y = global_pt[4][0].y + pxml->m_arrangement.distance_between_patterns.vertical_3rd_distance;
		global_pt[5][0].z = 0.0f;

		global_pt[5][1].x = global_pt[5][0].x + pxml->m_pattern.unit_space;
		global_pt[5][1].y = global_pt[5][0].y;
		global_pt[5][1].z = 0.0f;

		global_pt[5][2].x = global_pt[5][1].x + pxml->m_pattern.unit_space;
		global_pt[5][2].y = global_pt[5][1].y;
		global_pt[5][2].z = 0.0f;


		// pattern #2'
		global_pt[6][0].x = top_left_x + pxml->m_arrangement.poster.width - 2 * pxml->m_pattern.unit_space - pxml->m_arrangement.pattern_offset_from_car.right_pattern_offset;
		global_pt[6][0].y = top_left_y + pxml->m_arrangement.poster.height - 2 * pxml->m_pattern.unit_space;
		global_pt[6][0].z = 0.0f;

		global_pt[6][1].x = global_pt[6][0].x;
		global_pt[6][1].y = global_pt[6][0].y + pxml->m_pattern.unit_space;
		global_pt[6][1].z = 0.0f;

		global_pt[6][2].x = global_pt[6][1].x;
		global_pt[6][2].y = global_pt[6][1].y + pxml->m_pattern.unit_space;
		global_pt[6][2].z = 0.0f;

		// pattern #1'
		global_pt[7][0].x = top_left_x + 2 * pxml->m_pattern.unit_space + pxml->m_arrangement.pattern_offset_from_car.left_pattern_offset;
		global_pt[7][0].y = top_left_y + pxml->m_arrangement.poster.height - 2 * pxml->m_pattern.unit_space;
		global_pt[7][0].z = 0.0f;

		global_pt[7][1].x = global_pt[7][0].x;
		global_pt[7][1].y = global_pt[7][0].y + pxml->m_pattern.unit_space;
		global_pt[7][1].z = 0.0f;

		global_pt[7][2].x = global_pt[7][1].x;
		global_pt[7][2].y = global_pt[7][1].y + pxml->m_pattern.unit_space;
		global_pt[7][2].z = 0.0f;

		// pattern #6'
		global_pt[8][0].x = top_left_x + 2 * pxml->m_pattern.unit_space;
		global_pt[8][0].y = top_left_y + pxml->m_arrangement.poster.height - 2 * pxml->m_pattern.unit_space - pxml->m_arrangement.pattern_offset_from_car.rear_pattern_offset;
		global_pt[8][0].z = 0.0f;

		global_pt[8][1].x = global_pt[8][0].x - pxml->m_pattern.unit_space;
		global_pt[8][1].y = global_pt[8][0].y;
		global_pt[8][1].z = 0.0f;

		global_pt[8][2].x = global_pt[8][1].x - pxml->m_pattern.unit_space;
		global_pt[8][2].y = global_pt[8][1].y;
		global_pt[8][2].z = 0.0f;

		// pattern #5'
		global_pt[9][0].x = global_pt[8][0].x;
		global_pt[9][0].y = global_pt[8][0].y - pxml->m_arrangement.distance_between_patterns.vertical_3rd_distance;
		global_pt[9][0].z = 0.0f;

		global_pt[9][1].x = global_pt[9][0].x - pxml->m_pattern.unit_space;
		global_pt[9][1].y = global_pt[9][0].y;
		global_pt[9][1].z = 0.0f;

		global_pt[9][2].x = global_pt[9][1].x - pxml->m_pattern.unit_space;
		global_pt[9][2].y = global_pt[9][1].y;
		global_pt[9][2].z = 0.0f;

		// pattern #4'
		global_pt[10][0].x = global_pt[9][0].x;
		global_pt[10][0].y = global_pt[9][0].y - pxml->m_arrangement.distance_between_patterns.vertical_2nd_distance;
		global_pt[10][0].z = 0.0f;

		global_pt[10][1].x = global_pt[10][0].x - pxml->m_pattern.unit_space;
		global_pt[10][1].y = global_pt[10][0].y;
		global_pt[10][1].z = 0.0f;

		global_pt[10][2].x = global_pt[10][1].x - pxml->m_pattern.unit_space;
		global_pt[10][2].y = global_pt[10][1].y;
		global_pt[10][2].z = 0.0f;

		// pattern #3'
		global_pt[11][0].x = global_pt[10][0].x;
		global_pt[11][0].y = global_pt[10][0].y - pxml->m_arrangement.distance_between_patterns.vertical_1st_distance;
		global_pt[11][0].z = 0.0f;

		global_pt[11][1].x = global_pt[11][0].x - pxml->m_pattern.unit_space;
		global_pt[11][1].y = global_pt[11][0].y;
		global_pt[11][1].z = 0.0f;

		global_pt[11][2].x = global_pt[11][1].x - pxml->m_pattern.unit_space;
		global_pt[11][2].y = global_pt[11][1].y;
		global_pt[11][2].z = 0.0f;
	}

	
}


void sanWorld::get_real_pattern_point_mm_in_PATTERN(int camID, sanXML* pxml, XYZ* pattern_pt3d_mm /*[out]*/)
{
	float top_left_x = 0.0f, top_left_y = 0.0f, top_left_z = 0.0f;
	int left_idx = 0, right_idx = 0;

	switch (camID)
	{
	case CAMID_FRONT:
	{
		top_left_x = (-1.0f / 2.0f) * pxml->m_arrangement.poster.width;
		top_left_y = (-1.0f / 2.0f) * pxml->m_arrangement.poster.height;
		top_left_z = 0.0f;
		left_idx = 0;
		right_idx = 1;
		break;
	}
	case CAMID_RIGHT:
	{
		top_left_x = (+1.0f / 2.0f) * pxml->m_arrangement.poster.width;
		top_left_y = (-1.0f / 2.0f) * pxml->m_arrangement.poster.height;
		top_left_z = 0.0f;

		if(pxml->m_svm_patterns_num == 4)
		{
			left_idx = 2;
			right_idx = 3;
		}
		else if(pxml->m_svm_patterns_num == 6)
		{
			left_idx = 3;
			right_idx = 4;
		}

		break;
	}
	case CAMID_REAR:
	{
		top_left_x = (+1.0f / 2.0f) * pxml->m_arrangement.poster.width;
		top_left_y = (+1.0f / 2.0f) * pxml->m_arrangement.poster.height;
		top_left_z = 0.0f;
		if(pxml->m_svm_patterns_num == 4)
		{
			left_idx = 4;
			right_idx = 5;
		}
		else if(pxml->m_svm_patterns_num == 6)
		{
			left_idx = 6;
			right_idx = 7;
		}
		break;
	}
	case CAMID_LEFT:
	{
		top_left_x = (-1.0f / 2.0f) * pxml->m_arrangement.poster.width;
		top_left_y = (+1.0f / 2.0f) * pxml->m_arrangement.poster.height;
		top_left_z = 0.0f;
		if(pxml->m_svm_patterns_num == 4)
		{
			left_idx = 6;
			right_idx = 7;
		}
		else if(pxml->m_svm_patterns_num == 6)
		{
			left_idx = 9;
			right_idx = 10;
		}
		break;
	}
	default:
		break;
	}

	Point3f top_left_local = GLOBAL_to_LOCAL(camID, Point3f(top_left_x, top_left_y, top_left_z));

	for (int i = 0; i < PATTERN_POINTS_NUM; i++)
	{
		Point3f left = GLOBAL_to_LOCAL(camID, Point3f(pxml->m_global_pts[left_idx][i].x, pxml->m_global_pts[left_idx][i].y, pxml->m_global_pts[left_idx][i].z));
		pattern_pt3d_mm[i].x = left.x - top_left_local.x;
		pattern_pt3d_mm[i].y = left.y - top_left_local.y;
		pattern_pt3d_mm[i].z = left.z - top_left_local.z;

		Point3f right = GLOBAL_to_LOCAL(camID, Point3f(pxml->m_global_pts[right_idx][i].x, pxml->m_global_pts[right_idx][i].y, pxml->m_global_pts[right_idx][i].z));
		pattern_pt3d_mm[i + PATTERN_POINTS_NUM].x = right.x - top_left_local.x;
		pattern_pt3d_mm[i + PATTERN_POINTS_NUM].y = right.y - top_left_local.y;
		pattern_pt3d_mm[i + PATTERN_POINTS_NUM].z = right.z - top_left_local.z;
	}

}


vector<Point3f> sanWorld::get_vehicle_box_InGLOBAL(sanXML* pxml, int vehicle_box_unit /*0: VEHICLE_BOX_UNIT_MM, 1: VEHICLE_BOX_UNIT_LOGIC*/)
{
	// If the offset of car to pattern exists, then the position of the real vehicle has to be shifted.
	// Note the GL coordinate system as well and the normalization range of -1 to +1.
	float half_poster_width = (vehicle_box_unit == VEHICLE_BOX_UNIT_LOGIC) ? (pxml->m_arrangement.poster.width / 2.0f) : 1.0f;
	float half_vehicle_length = (pxml->m_vehicle_spec.length / 2.0f) / half_poster_width;
	float half_vehicle_width = (pxml->m_vehicle_spec.width / 2.0f) / half_poster_width;

	/* vertex order of the vehicle box, based on GL
	   v0 ------ v2
	   |	     |
	   | 	     |
	   |	     |
	   v1 ------ v3	  */

	vector<Point3f> vehicle_position;
	vehicle_position.push_back(Point3f(-half_vehicle_width, half_vehicle_length, 0.0f));   // v0
	vehicle_position.push_back(Point3f(-half_vehicle_width, -half_vehicle_length, 0.0f));  // v1
	vehicle_position.push_back(Point3f(half_vehicle_width, half_vehicle_length, 0.0f));    // v2
	vehicle_position.push_back(Point3f(half_vehicle_width, -half_vehicle_length, 0.0f));   // v3

	return vehicle_position;
}


void sanWorld::apply_expanded_distance_to_vehicle_box(sanXML* pxml, vector<Point3f>& vehicle_box, int vehicle_box_unit /*0: VEHICLE_BOX_UNIT_MM, 1: VEHICLE_BOX_UNIT_LOGIC*/)
{
	float half_poster_width = (vehicle_box_unit == VEHICLE_BOX_UNIT_LOGIC) ? (pxml->m_arrangement.poster.width / 2.0f) : 1.0f;
	float front_expanded_distance = pxml->m_vbc_parameter.front_expanded_distance_mm / half_poster_width;
	float right_expanded_distance = pxml->m_vbc_parameter.right_expanded_distance_mm / half_poster_width;
	float rear_expanded_distance = pxml->m_vbc_parameter.rear_expanded_distance_mm / half_poster_width;
	float left_expanded_distance = pxml->m_vbc_parameter.left_expanded_distance_mm / half_poster_width;

	/* vertex order of the vehicle box, based on GL
	   v0 ------ v2
	   |	     |
	   | 	     |
	   |	     |
	   v1 ------ v3	  */

	vehicle_box[0].x -= left_expanded_distance; //v0
	vehicle_box[0].y += front_expanded_distance;

	vehicle_box[1].x -= left_expanded_distance; //v1
	vehicle_box[1].y -= rear_expanded_distance;

	vehicle_box[2].x += right_expanded_distance; //v2
	vehicle_box[2].y += front_expanded_distance;

	vehicle_box[3].x += right_expanded_distance; //v3
	vehicle_box[3].y -= rear_expanded_distance;
}


vector<float> sanWorld::get_distance_from_vehicle_mm(sanXML* pxml, vector<Point3f>& global_pt3d_mm)
{
 /*     position order	     1  |  2  | 3
         v0 --- v2	        ----|-----|----            GL coordinate system              
         |	     |              |     |                    | Y
         | 	     |	         8  |     |  4                 |
         |	     | 	            |     |                 ---|-----> X
         v1 --- v3	        ----|-----|----                |
	                         7  |  6  |  5                                   */

	vector<float> distance;
	vector<Point3f> vehicle_pt3d_mm = get_vehicle_box_InGLOBAL(pxml, VEHICLE_BOX_UNIT_MM);
	vector<Point3f> vehicle_zone = { vehicle_pt3d_mm[3], vehicle_pt3d_mm[2], vehicle_pt3d_mm[0], vehicle_pt3d_mm[1] }; // to match the input parameters of isInsideZone()

	for (int i = 0; i < (int)global_pt3d_mm.size(); i++)
	{
		float distance_from_car = 0.0f;
		
		bool is_inside_zone = sanWorld::isInsideZone(vehicle_zone, global_pt3d_mm[i]);

		if (!is_inside_zone)
		{
			float x = global_pt3d_mm[i].x;
			float y = global_pt3d_mm[i].y;
			

			if ((x <= vehicle_pt3d_mm[0].x) && (y >= vehicle_pt3d_mm[0].y))  // 1
			{
				float dx = x - vehicle_pt3d_mm[0].x;
				float dy = y - vehicle_pt3d_mm[0].y;
				distance_from_car = (float)sqrt(dx * dx + dy * dy);
			}
			else if ((x >= vehicle_pt3d_mm[0].x) && (y >= vehicle_pt3d_mm[0].y) && (x <= vehicle_pt3d_mm[2].x)) // 2
			{
				distance_from_car = fabs(y - vehicle_pt3d_mm[0].y);
			}
			else if ((x >= vehicle_pt3d_mm[2].x) && (y >= vehicle_pt3d_mm[2].y))  // 3
			{
				float dx = x - vehicle_pt3d_mm[2].x;
				float dy = y - vehicle_pt3d_mm[2].y;
				distance_from_car = (float)sqrt(dx * dx + dy * dy);
			}
			else if ((x >= vehicle_pt3d_mm[2].x) && (y <= vehicle_pt3d_mm[2].y) && (y >= vehicle_pt3d_mm[3].y)) // 4
			{
				distance_from_car = fabs(x - vehicle_pt3d_mm[2].x);
			}
			else if ((x >= vehicle_pt3d_mm[3].x) && (y <= vehicle_pt3d_mm[3].y)) // 5
			{
				float dx = x - vehicle_pt3d_mm[3].x;
				float dy = y - vehicle_pt3d_mm[3].y;
				distance_from_car = (float)sqrt(dx * dx + dy * dy);
			}
			else if ((x <= vehicle_pt3d_mm[3].x) && (y <= vehicle_pt3d_mm[3].y) && (x >= vehicle_pt3d_mm[1].x)) // 6
			{
				distance_from_car = fabs(y - vehicle_pt3d_mm[3].y);
			}
			else if ((x <= vehicle_pt3d_mm[1].x) && (vehicle_pt3d_mm[1].y >= y)) // 7
			{
				float dx = x - vehicle_pt3d_mm[1].x;
				float dy = y - vehicle_pt3d_mm[1].y;
				distance_from_car = (float)sqrt(dx * dx + dy * dy);
			}
			else if ((x <= vehicle_pt3d_mm[1].x) && (y >= vehicle_pt3d_mm[1].y) && (y <= vehicle_pt3d_mm[0].y)) // 8
			{
				distance_from_car = fabs(x - vehicle_pt3d_mm[1].x);
			}
			else
				distance_from_car = 0.0f; // inside the area occupied by the car
		}
		else
		{
			distance_from_car = 0.0f;
		}

		distance.push_back(distance_from_car);
	}

	return  distance;
}


//bool sanWorld::isInsideZone(vector<Point3f> zone_pt3d_mm, Point3f target_pt3d_mm)
//{
//	/* zone_pt3d_mm point order
//	*     p3------------p2
//	*     |              |
//	*     |              |
//	*     p4------------p1
//	*/
//
//	float x = target_pt3d_mm.x;
//	float y = target_pt3d_mm.y;
//	bool inside = false;
//
//	if (zone_pt3d_mm.size())
//	{
//		Point3f p1 = zone_pt3d_mm[zone_pt3d_mm.size() - 1];
//		Point3f p2;
//
//		for (int i = 0; i < zone_pt3d_mm.size(); i++)
//		{
//			p2 = zone_pt3d_mm[i];
//
//			if (y >= min(p1.y, p2.y))
//			{
//				if (y <= max(p1.y, p2.y))
//				{
//					if (x <= max(p1.x, p2.x))
//					{
//						if (fabs(p2.y - p1.y) <= FLT_EPSILON)
//						{
//							if (x >= min(p1.x, p2.x))
//							{
//								inside = true;
//								break;
//							}
//							else noop;
//						}
//						else
//						{
//							float x_intersection = p1.x + (y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
//							if ((fabs(p1.x - p2.x) <= FLT_EPSILON) || (x <= x_intersection)) inside = !inside;
//							else noop;
//						}
//					}
//					else noop;
//				}
//				else noop;
//			}
//			else noop;
//
//			p1 = p2;
//		}
//	}
//	else noop;
//
//	return inside;
//}

bool sanWorld::is_P_InSegment_P0P1(Point3f P, Point3f P0, Point3f P1)
{
	Point2f p0 = Point2f(P0.x - P.x, P0.y - P.y);
	Point2f p1 = Point2f(P1.x - P.x, P1.y - P.y);

	float det = (p0.x * p1.y - p1.x * p0.y);
	float prod = (p0.x * p1.x + p0.y * p1.y);

	return (det == 0.0f && prod < 0.0f) || (p0.x == 0.0f && p0.y == 0.0f) || (p1.x == 0.0f && p1.y == 0.0f);
}

bool sanWorld::isInsideZone(vector<Point3f> zone_pt3d_mm, Point3f target_pt3d_mm, bool validBorder)
{
	/* https://www.linkedin.com/pulse/short-formula-check-given-point-lies-inside-outside-polygon-ziemecki */

	/* zone_pt3d_mm point order
	*     p3------------p2
	*     |              |
	*     |              |
	*     p4------------p1
	*/
	bool return_value = false;

	std::complex<float> sum(0.0f, 0.0f);

	for (int i = 1; i <= (int)zone_pt3d_mm.size(); i++)
	{
		Point3f v0 = zone_pt3d_mm[i - 1];
		Point3f v1 = zone_pt3d_mm[i % zone_pt3d_mm.size()];

		if (is_P_InSegment_P0P1(target_pt3d_mm, v0, v1))
		{
			return_value = validBorder;
			break;
		}
		else noop;

		sum += std::log((std::complex<float>(v1.x, v1.y) - std::complex<float>(target_pt3d_mm.x, target_pt3d_mm.y)) / (std::complex<float>(v0.x, v0.y) - std::complex<float>(target_pt3d_mm.x, target_pt3d_mm.y)));
	}

	return_value = std::abs(sum) > 1.0f;

	return return_value;
}

bool sanWorld::is_P_InSegment2d_P0P1(Point2f P, Point2f P0, Point2f P1)
{
	Point2f p0 = Point2f(P0.x - P.x, P0.y - P.y);
	Point2f p1 = Point2f(P1.x - P.x, P1.y - P.y);

	float det = (p0.x * p1.y - p1.x * p0.y);
	float prod = (p0.x * p1.x + p0.y * p1.y);

	return (det == 0.0f && prod < 0.0f) || (p0.x == 0.0f && p0.y == 0.0f) || (p1.x == 0.0f && p1.y == 0.0f);
}

bool sanWorld::isInsideZone2d(vector<Point2f> zone_pt2d_mm, Point2f target_pt2d_mm, bool validBorder)
{
	/* https://www.linkedin.com/pulse/short-formula-check-given-point-lies-inside-outside-polygon-ziemecki */

	/* zone_pt3d_mm point order
	*     p3------------p2
	*     |              |
	*     |              |
	*     p4------------p1
	*/
	bool return_value = false;

	std::complex<float> sum(0.0f, 0.0f);

	for (int i = 1; i <= (int)zone_pt2d_mm.size(); i++)
	{
		Point2f v0 = zone_pt2d_mm[i - 1];
		Point2f v1 = zone_pt2d_mm[i % zone_pt2d_mm.size()];

		if (is_P_InSegment2d_P0P1(target_pt2d_mm, v0, v1))
		{
			return_value = validBorder;
			break;
		}
		else noop;

		sum += std::log((std::complex<float>(v1.x, v1.y) - std::complex<float>(target_pt2d_mm.x, target_pt2d_mm.y)) / (std::complex<float>(v0.x, v0.y) - std::complex<float>(target_pt2d_mm.x, target_pt2d_mm.y)));
	}

	return_value = std::abs(sum) > 1.0f;

	return return_value;
}
