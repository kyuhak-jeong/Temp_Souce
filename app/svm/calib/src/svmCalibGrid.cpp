#include "svmCalibGrid.hpp"
#include "svmProject.hpp"
#include "svmLogger.hpp"
#include "svmCalibMask.hpp"
#include "svmWorld.hpp"
#include "svmShaderString.hpp"
#include "svmError.hpp"


sanCalibGrid::sanCalibGrid(sanXML* pxml, sanCamera* pcameras, sanCalibCal* pcalibration)
{
	m_pxml = pxml;
	m_pcameras = pcameras;
	m_pcalibration = pcalibration;
}

sanCalibGrid::~sanCalibGrid()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		if(m_gridLUT[camID] != nullptr)
		{
			free(m_gridLUT[camID]);
			m_gridLUT[camID] = nullptr;
		}
		else noop;
		
		if(!m_grids[camID].v3d.empty()) m_grids[camID].v3d.clear(); else noop;
		if(!m_grids[camID].p2d.empty()) m_grids[camID].p2d.clear(); else noop;
		if(!m_grids[camID].seam_candidators_defisheye.empty()) m_grids[camID].seam_candidators_defisheye.clear(); else noop;
		if(!m_grids[camID].seam_candidators_local.empty()) m_grids[camID].seam_candidators_local.clear(); else noop;
	}

	m_gridShader.~sanShader();
}

void sanCalibGrid::initialize()
{
	try
	{
		m_base_radius = sanWorld::calBaseRadius(m_pxml);
		m_pgrid_params = &m_pxml->m_grid;
		m_pgrid_params->nop_z = get_minimal_NOP_Z();
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1105201", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibGrid::updateGrids(int camID)
{
	try
	{
		createGridLUT(camID);

		if(!m_is_vabt_generated[camID]) generateVAB(m_gridLUT[camID], (GLuint)m_vertices_num[camID], 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(camID, m_gridLUT[camID], (GLuint) m_vertices_num[camID], 3, 0, GL_DYNAMIC_DRAW);
		m_is_vabt_generated[camID] = true;
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1205221", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibGrid::createGridLUT(int camID)
{
	try 
	{
		// need to clear old memory when updating new data
		if(m_gridLUT[camID] != nullptr) free(m_gridLUT[camID]); else noop;

		int rowNum = 1;
		int colNum = 1;
		float rowNorm = 1.0f / rowNum;
		float colNorm = 1.0f / colNum;
		int rowIdx = 0;
		int colIdx = 0;
		Point2f top_left_point = Point2f((colIdx * colNorm - 0.5f) * 2.0f, -(rowIdx * rowNorm - 0.5f) * 2.0f);

		string str_camID = "(camID[" + to_string(camID) + "])";
		int vertices_num = (int)m_grids[camID].p2d.size();
		float* vertices = (float*)calloc((size_t)vertices_num * 3, sizeof(float));	// 3: (x, y, z)

		if (vertices == nullptr)
			throw runtime_error(str_camID + delimiter(string("$failed to allocate memory")));
		else
		{
			float img_width = (float)m_pcameras->m_xmaps[camID].cols;
			float img_height = (float)m_pcameras->m_xmaps[camID].rows;
			float x_norm = 1.0f / img_width;
			float y_norm = 1.0f / img_height;

			int k = 0;

			for (int i = 0; i < (int)m_grids[camID].p2d.size(); i++)
			{
				if ((m_grids[camID].p2d[i].x < img_width) && (m_grids[camID].p2d[i].y < img_height) && (m_grids[camID].p2d[i].x > 0) && (m_grids[camID].p2d[i].y > 0))
				{

					vertices[k + 0] = top_left_point.x + m_grids[camID].p2d[i].x * x_norm * 2.0f * colNorm;
					vertices[k + 1] = top_left_point.y - m_grids[camID].p2d[i].y * y_norm * 2.0f * rowNorm;
					vertices[k + 2] = 0.0f;

					k += 3;
				}
				else noop;
			}

			m_gridLUT[camID] = vertices;
			m_vertices_num[camID] = vertices_num;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


void sanCalibGrid::renderGrids(int camID, int glDrawingType)
{
	try
	{
		m_gridShader.use();
		glDisable(GL_BLEND);

		string str_camID = "(camID[" + to_string(camID) + "])";
		sanError::glClearError();

		glBindVertexArray(getVaoID(camID));
		glBindBuffer(GL_ARRAY_BUFFER, getVboID(camID));

		// glActiveTexture(GL_TEXTURE0);
		// glBindTexture(GL_TEXTURE_2D, m_vabt_list[camID].texID);

		#if defined(_WIN32) || defined(_WIN64)
			if (glDrawingType == GL_POINTS)	glPointSize(2.0f);
			else noop;
		#endif

		glDrawArrays(glDrawingType, 0, getVnum(camID));

		sanError::glCheckError(str_camID);
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1205231", __FUNCTION__ + delimiter(string(e.what())));
	}
}


void sanCalibGrid::createGrids(int camID)
{
	try
	{
		string str_camID = "(camID[" + to_string(camID) + "])";
	
		try
		{
			double circle_radius = m_pgrid_params->base_radius_times * m_base_radius;

			CurvilinearGrid cg_p1p8; //curvillinear grid
			cg_p1p8.v3d = get_seam_candidator_p1p8(camID);
			project_pt3d(camID, cg_p1p8);

			CurvilinearGrid cg;
			create_curvilinear_grid(circle_radius, cg, cg_p1p8.v3d);	// 3d points(with OpenCV coordinates)
			project_pt3d(camID, cg);				 						    // 2d points(with OpenCV coordinates), defisheye image
			// reorganize_grid(camID, cg);

			find_seam_candidators(camID, cg, cg_p1p8);
			m_grids[camID] = cg;

			#if (0) //for debugging, it shows the 8 seam candidators.
				string camera_name[SVM_CAMERAS_NUM] = { string("Front"), string("Right"), string("Rear"), string("Left") };
				int img_width = (int)(m_pxml->m_resolution.image.width / 2.0f);
				int img_height = (int)(m_pxml->m_resolution.image.height / 2.0f);
				Mat img(img_height, img_width, CV_8UC3, Scalar(0, 0, 0));

				for (int i = 0; i < (int)cg.p2d.size(); i++)
				{
					int x = (int)(cg.p2d[i].x / 2.0f);
					int y = (int)(cg.p2d[i].y / 2.0f);
					circle(img, Point(x, y), 1, Scalar(0, 128, 128), CV_FILLED);
				}

				for (int i = 0; i < (int)cg.seam_candidators_defisheye.size(); i++)
				{
					int x = (int)(cg.seam_candidators_defisheye[i].x / 2.0f);
					int y = (int)(cg.seam_candidators_defisheye[i].y / 2.0f);
					int txt_x = (x + 40 < img_width) ? ((x - 40 < 0) ? x + 20 : x + 10) : ((x + 40 >= img_width) ? x - 30 : x + 10);
					int txt_y = (y + 40 < img_height) ? ((y - 40 < 0) ? y + 20 : y) : y - 20;

					string txt = "p" + to_string(i + 1);
					putText(img, txt, Point(txt_x, txt_y), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255));
					circle(img, Point(x, y), 4, Scalar(255, 0, 0), CV_FILLED);
				}
				string window_title = camera_name[camID] + ": " + to_string(cg.seam_candidators_defisheye.size()) + " points";
				imshow(window_title.c_str(), img);
				waitKey(0);
			#endif
		}
		catch (exception& e)
		{
			throw runtime_error(str_camID + delimiter(string(e.what())));
		}

		logger.record_message(logger.svm_inform("--------", str_camID + "grid creation successful").what());
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1205211", __FUNCTION__ + delimiter(string(e.what())));
	}
}


void sanCalibGrid::create_curvilinear_grid(double circle_radius, CurvilinearGrid& cg, vector<Point3f>& local_threshold_pt3d)
{
	int start_circle_step_index = m_pgrid_params->start_step_index;
	float length_per_step = m_pgrid_params->step_length;
	int total_circle_steps_num = (int)(circle_radius / length_per_step);

	int start_arc_index = m_pgrid_params->start_arc_index;
	int total_arcs_num = m_pgrid_params->arcs_num;
	double radian_per_arc = (M_PI / total_arcs_num);

	// information about arcs and steps when creating girds
	cg.actual_circle_steps_num = (total_circle_steps_num - start_circle_step_index);
	cg.actual_parabola_steps_num = m_pgrid_params->nop_z;
	cg.actual_arcs_num = (total_arcs_num - 2 * start_arc_index);
	float local_threshold_y = max(local_threshold_pt3d[0].y, local_threshold_pt3d[1].y);
	
	int last_arc_index = cg.actual_arcs_num + start_arc_index;

	for (int arc_line_index = start_arc_index + 1; arc_line_index <= last_arc_index; arc_line_index++) // note the use of index
	{		
		double second_arc_line_radian = (arc_line_index + 0) * radian_per_arc; // End angle of the current circular arc
		double first_arc_line_radian = (arc_line_index - 1) * radian_per_arc; // Start angle of the current circular arc

		//openCV 좌표계(X+: 오른쪽, Y+: 아래쪽, Z+: 시선 방향)에서 Y- 방향에 있는 bowl의 반쪽만 생성
		//           /   Z+
		//          /   
		//         ------>X+
		//         |
		//         | Y+

		// Flat bottom points about the trajectory of a circle (x^2 + y^2 = r^2).
		for (int step_point_index = start_circle_step_index; step_point_index < total_circle_steps_num; step_point_index++)
		{
			// x= r*cos(θ), y=r*sin(θ)
			double step_radius = step_point_index * length_per_step;
			float second_x = (float)(step_radius * cos(second_arc_line_radian));
			float second_y = (float)(-step_radius * sin(second_arc_line_radian));
			float second_z = 0.0f;

			if (local_threshold_y < second_y)
				second_y = local_threshold_y;
			else noop;

			cg.v3d.push_back(Point3f(second_x, second_y, second_z));
	
			float first_x = (float)(step_radius * cos(first_arc_line_radian));
			float first_y = (float)(-step_radius * sin(first_arc_line_radian));
			float first_z = 0.0f;
			if (local_threshold_y < first_y)
				first_y = local_threshold_y;
			else noop;

			cg.v3d.push_back(Point3f(first_x, first_y, first_z));
		}

		// Points on bowl side ( h = (radius - base_radius)^2, note the starting index
		for (int step_point_index = 1; step_point_index <= cg.actual_parabola_steps_num; step_point_index++)
		{
			double parabola_domain_distance =  step_point_index * length_per_step;
			double step_radius = circle_radius + parabola_domain_distance;	

			float second_x = (float)(step_radius * cos(second_arc_line_radian));
			float second_y = (float)(-step_radius * sin(second_arc_line_radian));
			float second_z = (float)(-pow(parabola_domain_distance, 2));
			if (local_threshold_y < second_y)
				second_y = local_threshold_y;
			else noop;

			cg.v3d.push_back(Point3f(second_x, second_y, second_z));

			float first_x = (float)(step_radius * cos(first_arc_line_radian));
			float first_y = (float)(-step_radius * sin(first_arc_line_radian));
			float first_z = (float)(-pow(parabola_domain_distance, 2));
			if (local_threshold_y < first_y)
				first_y = local_threshold_y;
			else noop;

			cg.v3d.push_back(Point3f(first_x, first_y, first_z));
		}
	}
}

// to project normalized 3d points in local space to abnormalized 2d points in fisheye image.
void sanCalibGrid::project_pt3d(int camID, CurvilinearGrid& cg)
{
	Mat rvec = m_pcameras->getRvec(camID);
	Mat tvec = m_pcameras->getTvec(camID);
	Mat K = m_pcameras->getK(camID);
	Mat distCoeffs = m_pcameras->getDistCoeffs(camID); // not used

	sanProject::projectPoints(cg.v3d, rvec, tvec, K, distCoeffs, cg.p2d);
	
#if (0) // for debugging
	cout << rvec << endl << endl;
	cout << tvec << endl << endl;
	cout << K << endl << endl;
	cout << distCoeffs << endl << endl;
	for (int i = 0; i < cg.p2d.size(); i++)
		printf("[%6d] %10.4f %10.4f\n", i, cg.p2d[i].x, cg.p2d[i].y);
#endif
}

#if (0)
void sanCalibGrid::reorganize_grid(int camID, double circle_radius, CurvilinearGrid& cg)
{
	int img_width = m_pcameras->m_xmaps[camID].cols;
	int img_height = m_pcameras->m_xmaps[camID].rows;
	int points_num_per_arc = 2 * (cg.actual_circle_steps_num + cg.actual_parabola_steps_num);

	Mat tmp_mask(img_height, img_width, CV_8U, Scalar(255));
	remap(tmp_mask, tmp_mask, m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], cv::INTER_LINEAR);

	int half_img_height = img_height / 2;
	bool isfound = false;
	float local_black_y = img_height;
	for (int arc_idx = 0; arc_idx < cg.actual_arcs_num; arc_idx++)
	{
		for (int point_idx = 1; point_idx < points_num_per_arc; point_idx += 2)
		{
			int idx = arc_idx * points_num_per_arc + point_idx;
			int imgx = (int)cg.p2d[idx].x;
			int imgy = (int)cg.p2d[idx].y;

			if ((0 <= imgx) && 	(imgx < img_width) &&
				(half_img_height <= imgy) && (imgy < img_height))
			{
				uchar mask_value = tmp_mask.at<uchar>(imgy, imgx);
				if (mask_value < 255)
				{
					isfound = true;
					local_black_y = min(local_black_y, cg.v3d[idx].y);
				}
				else noop;
			}
			else noop;
		}
	}

	if (isfound)
	{
		for (int arc_idx = 0; arc_idx < cg.actual_arcs_num; arc_idx++)
		{
			for (int point_idx = 0; point_idx < points_num_per_arc; point_idx++)
			{
				int idx = arc_idx * points_num_per_arc + point_idx;
				if (local_black_y < cg.v3d[idx].y)
					cg.v3d[idx].y = local_black_y;
				else noop;
			}
		}
		cg.p2d.clear();
		project_pt3d(camID, circle_radius, cg);
	}
	else noop;
}

#else

void sanCalibGrid::reorganize_grid(int camID, CurvilinearGrid& cg)
{
	int img_width = m_pcameras->m_xmaps[camID].cols;
	int img_height = m_pcameras->m_xmaps[camID].rows;
	int points_num_per_arc = 2 * (cg.actual_circle_steps_num + cg.actual_parabola_steps_num);

	Mat tmp_mask(img_height, img_width, CV_8U, Scalar(255));
	remap(tmp_mask, tmp_mask, m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], cv::INTER_LINEAR);

	Point2f defisheye_min_point((float)img_width - 1, (float)(img_height - 1));

	for (int imgy = img_height / 2; imgy < img_height; imgy++)
	{
		for (int imgx = 0; imgx < img_width; imgx++)
		{
			uchar mask_value = tmp_mask.at<uchar>(imgy, imgx);
			if ((mask_value < 255) && (imgy < defisheye_min_point.y))
			{
				defisheye_min_point = Point2f((float)imgx, (float)imgy);
			}
			else noop;
		}
	}
	
	vector<Point2f> defisheye_p2d;
	defisheye_p2d.push_back(defisheye_min_point);
	vector<Point3f> local_min_point = sanWorld::defisheye_to_LOCAL(camID, m_pxml, defisheye_p2d);
	for (int arc_idx = 0; arc_idx < cg.actual_arcs_num; arc_idx++)
	{
		for (int point_idx = 0; point_idx < points_num_per_arc; point_idx++)
		{
			int idx = arc_idx * points_num_per_arc + point_idx;
			if (local_min_point[0].y < cg.v3d[idx].y)
				cg.v3d[idx].y = local_min_point[0].y;
			else noop;
		}
	}

	cg.p2d.clear();
	project_pt3d(camID, cg);
}

#endif


int sanCalibGrid::get_nop_index_from_bowl(int camID, double circle_radius, double step_length)
{
	try
	{
		Mat tmp_mask(m_pcameras->m_xmaps[camID].rows, m_pcameras->m_xmaps[camID].cols, CV_8U, Scalar(255));
		remap(tmp_mask, tmp_mask, m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], cv::INTER_LINEAR);

		Mat K = m_pcameras->getK(camID);
		Mat distCoeffs = m_pcameras->getDistCoeffs(camID);
		Mat rvec = m_pcameras->getRvec(camID);
		Mat tvec = m_pcameras->getTvec(camID);

		int total_lines_min_step_index = m_pxml->m_grid.nop_z;

		for (int bowl_index = 1; bowl_index <= m_pgrid_params->nop_z; bowl_index++)
		{
			bool existed = false;

			for (int arc_index = m_pgrid_params->start_arc_index; arc_index <= (m_pgrid_params->arcs_num - m_pgrid_params->start_arc_index); arc_index++)
			{
				double angle = arc_index * (M_PI / m_pgrid_params->arcs_num); // Start angle of the current circular arc		
				double hptn = circle_radius + bowl_index * step_length;
				float x = (float)(hptn * cos(angle));
				float y = (float)(-hptn * sin(angle));
				float z = (float)(-pow(bowl_index * step_length, 2));

				vector<Point2f> pt2d;
				vector<Point3f> pt3d;
				pt3d.push_back(Point3f(x, y, z));
				sanProject::projectPoints(pt3d, rvec, tvec, K, distCoeffs, pt2d);

				if ((0 <= pt2d[0].y) && (round(pt2d[0].y) < tmp_mask.rows) && (0 <= pt2d[0].x) && (round(pt2d[0].x) < tmp_mask.cols))
				{
					existed = true;
					break;
				}
				else noop;
			}

			if (!existed)
			{
				total_lines_min_step_index = min(bowl_index - 1, total_lines_min_step_index);
			}
			else noop;
		}

		return(max(0, (total_lines_min_step_index)));
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}


// After finding the top of the lower black convex in the fisheye image, transform it to that in the local space.
Point3f sanCalibGrid::get_local_threshold_point(int camID)
{
	Mat tmp_mask(m_pcameras->m_xmaps[camID].rows, m_pcameras->m_xmaps[camID].cols, CV_8U, Scalar(255, 255, 255));
	remap(tmp_mask, tmp_mask, m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], cv::INTER_LINEAR);
	Point2f top_convex = sanCamera::get_top_of_black_convex(tmp_mask);
	float fisheye_x = m_pcameras->m_xmaps[camID].at<float>((int)top_convex.y, (int)top_convex.x);
	float fisheye_y = m_pcameras->m_ymaps[camID].at<float>((int)top_convex.y, (int)top_convex.x);

	vector<Point2f> pt2d;
	pt2d.push_back(Point2f(fisheye_x, fisheye_y));
	vector<Point3f> pt3d = sanWorld::fisheye_to_LOCAL(camID, m_pxml, pt2d);

	return pt3d[0];
}


vector<Point3f> sanCalibGrid::get_seam_candidator_p1p8(int camID)
{
	vector<Point3f> p1p8;
	try
	{
		vector<Point3f> vehicle_box_global_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
		vector<Point3f> vehicle_box_local_mm = sanWorld::GL_to_LOCAL(camID, vehicle_box_global_mm);
		vector<Point3f> vehicle_box_local = sanWorld::get_Logic_InLOCAL(m_pxml, vehicle_box_local_mm);
		
		switch (camID)
		{
		case CAMID_FRONT:
			p1p8.push_back(vehicle_box_local[0]); // p1
			p1p8.push_back(vehicle_box_local[2]); // p8
			break;
		case CAMID_RIGHT:
			p1p8.push_back(vehicle_box_local[2]); // p1
			p1p8.push_back(vehicle_box_local[3]); // p8
			break;
		case CAMID_REAR:
			p1p8.push_back(vehicle_box_local[3]); // p1
			p1p8.push_back(vehicle_box_local[1]); // p8
			break;
		case CAMID_LEFT:
			p1p8.push_back(vehicle_box_local[1]); // p1
			p1p8.push_back(vehicle_box_local[0]); // p8
			break;
		default:
			string str_camID = "(camID[" + to_string(camID) + "])";
			throw runtime_error(str_camID + delimiter(string("$camID out of range")));
			break;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}

	return p1p8;
}


int sanCalibGrid::get_minimal_NOP_Z()
{
	int min_nop_z = m_pgrid_params->nop_z;
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++) // Get number of points in z axis
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			double circle_radius = m_pgrid_params->base_radius_times * m_base_radius;
			int valid_nopz = get_nop_index_from_bowl(camID, circle_radius, m_pgrid_params->step_length);
			min_nop_z = min(min_nop_z, valid_nopz);
		}
		catch (exception& e)
		{
			throw runtime_error(__FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
	
	return min_nop_z;
}

/**************************************************************************************************************
 *
 * @brief  			Find seam candidator points( 8 points).
 *
 * @param  in		sanCamera* camera - pointer to the sanCamera object
 * 		   out		vector<Point3f> seam_candidator_local_points
 *
 * @return 			-
 *
 * @remarks			The function search 8 points to define the edge of grid. If the seam_points vector size is
 * 					not equal 8, then there was a problem with points searching and result of function is inapplicable.
 *                  the discription is based on OpenCV coordinate system, XY(flat plane) and -Z(height).

 * 					Points 1-4 describe the left edge of grid (they are located in II quadrant).
 * 					- 1st point is located on flat circle base (z = 0). It is the leftmost point with minimum value of y coordinate;
 * 					- 2nd point is located on flat circle base (z = 0). It is the leftmost point of grid which lies on base circle edge;
 * 					- 3rd point is located on bowl edge. It is the last point in first grid column with (z != 0);
 * 					- 4th point is located on bowl edge. It is the leftmost point with maximum value of z coordinate.
 *
 * 					Points 5-8 describe right edge of grid (they are located in I quadrant).
 * 					- 5th point is located on bowl edge. It is the rightmost point with maximum value of z coordinate.
 * 					- 6th point is located on bowl edge. It is the last point in last grid column with (z != 0);
 * 					- 7th point is located on flat circle base (z = 0). It is the rightmost point of grid which lies on base circle edge;
 *					- 8th point is located on flat circle base (z = 0). It is the rightmost point with minimum value of y coordinate.
 *
 **************************************************************************************************************/
void sanCalibGrid::find_seam_candidators(int camID, CurvilinearGrid& cg, CurvilinearGrid& cg_p1p8)
{
	int img_width = m_pcameras->m_xmaps[camID].cols;
	int img_height = m_pcameras->m_xmaps[camID].rows;
	int points_num_per_arc = 2 * (cg.actual_circle_steps_num + cg.actual_parabola_steps_num);
	int half_arcs_num = cg.actual_arcs_num / 2; // index of the middle angle

	// Get mask for defisheye transformation
	Mat tmp_mask(img_height, img_width, CV_8U, Scalar(255));
	remap(tmp_mask, tmp_mask, m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], cv::INTER_LINEAR);

	cg.seam_candidators_defisheye.clear();
	cg.seam_candidators_local.clear();

	bool isfound = false;

#if (0)
	/*********************************************************************************************************************
	 * p1 - 1st point is located on flat circle base (z = 0). It is the leftmost point with minimum value of radial
	 * 		coordinate in polar coordinate system
	 *********************************************************************************************************************/
	
	for (int point_idx = 1; point_idx < 2 * cg.actual_circle_steps_num; point_idx += 2)	
	{		
		for (int arc_idx = cg.actual_arcs_num - 1; arc_idx >= half_arcs_num; arc_idx--)
		{
			int idx = arc_idx * points_num_per_arc + point_idx;
			if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
			{
				if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
				{
					cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
					cg.seam_candidators_local.push_back(cg.v3d[idx]);
					isfound = true;
					break;
				}
				else
					noop;
			}
		}
		if (isfound) break; else noop;
	}
#else
	cg.seam_candidators_defisheye.push_back(cg_p1p8.p2d[0]);
	cg.seam_candidators_local.push_back(cg_p1p8.v3d[0]);
#endif

	/*********************************************************************************************************************
	 * p2 - 2nd point is located on flat circle base (z = 0). It is the leftmost point of grid which lies on base circle edge
	 *********************************************************************************************************************/	
	for (int arc_idx = cg.actual_arcs_num; arc_idx >= half_arcs_num; arc_idx--)
	{
		int idx = arc_idx * points_num_per_arc - 2 * cg.actual_parabola_steps_num - 2;
		if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
		{
			if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
			{
				cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
				cg.seam_candidators_local.push_back(cg.v3d[idx]);
				break;
			}
			else
				noop;
		}
		else
			noop;
	}

	/*************************************************************************************************************
	 * p3 - 3rd point is located on bowl edge. It is the last point in first grid column with (z != 0);
	 *************************************************************************************************************/
	isfound = false;
	for(int arc_idx = cg.actual_arcs_num; arc_idx >= half_arcs_num; arc_idx--)
	{
		for (int idx = arc_idx * points_num_per_arc - 2; idx >= (arc_idx * points_num_per_arc - 2 * cg.actual_parabola_steps_num); idx -= 2)
		{
			if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
			{
				if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
				{
					cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
					cg.seam_candidators_local.push_back(cg.v3d[idx]);
					isfound = true;
					break;
				}
				else
					noop;
			}
			else
				noop;
		}
		if (isfound)
			break;
		else
			noop;
	}

	/*********************************************************************************************************************
	 * p4 - 4th point is located on bowl edge. It is the leftmost point with maximum value of z coordinate.
	 *********************************************************************************************************************/
	for(int arc_idx = cg.actual_arcs_num; arc_idx >= half_arcs_num; arc_idx--)
	{
		int idx = arc_idx * points_num_per_arc - 2;
		if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
		{
			if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
			{
				cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
				cg.seam_candidators_local.push_back(cg.v3d[idx]);
				break;
			}
			else
				noop;
		}
		else
			noop;
	}


	/*********************************************************************************************************************
	 * p5 - 5th point is located on bowl edge. It is the rightmost point with maximum value of z coordinate.
	 *********************************************************************************************************************/
	for(int arc_idx = 1; arc_idx <= half_arcs_num; arc_idx++)
	{
		int idx = arc_idx * points_num_per_arc - 1;
		if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
		{
			if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
			{
				cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
				cg.seam_candidators_local.push_back(cg.v3d[idx]);
				break;
			}
			else
				noop;
		}
		else
			noop;
	}

	/*************************************************************************************************************
	 * p6 - 6th point is located on bowl edge. It is the last point in last grid column with (z != 0);
	 *************************************************************************************************************/
	isfound = false;
	for (int arc_idx = 1;  arc_idx <= half_arcs_num; arc_idx++)
	{
		for (int idx = arc_idx * points_num_per_arc - 1; idx >= (int)(arc_idx * points_num_per_arc - 2 * cg.actual_parabola_steps_num); idx -= 2)
		{
			if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
			{
				if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
				{
					cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
					cg.seam_candidators_local.push_back(cg.v3d[idx]);
					isfound = true;
					break;
				}
				else
					noop;
			}
			else
				noop;
		}
		if (isfound)
			break;
		else
			noop;
	}


	/*********************************************************************************************************************
	 * p7 - 7th point is located on flat circle base (z = 0). It is the rightmost point of grid which lies on base circle edge;
	 *********************************************************************************************************************/
	for (int arc_idx = 1; arc_idx <= half_arcs_num; arc_idx++)
	{
		int idx = arc_idx * points_num_per_arc  - 2 * cg.actual_parabola_steps_num - 1;
		if ((round(cg.p2d[idx].x) < img_width) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < img_height) && (cg.p2d[idx].y >= 0))
		{
			if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x) != 0))
			{
				cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
				cg.seam_candidators_local.push_back(cg.v3d[idx]);
				break;
			}
			else
				noop;
		}
		else
			noop;
	}

#if (0)
	/*********************************************************************************************************************
	 * p8 - 8th point is located on flat circle base (z = 0). It is the rightmost point with minimum value of radial
	 * 		coordinate in polar coordinate system
	*********************************************************************************************************************/
	isfound = false;
	for(int point_idx = 1; point_idx < 2 * cg.actual_circle_steps_num; point_idx +=2)
	{
		for (int arc_idx = 0; arc_idx < (int)half_arcs_num; arc_idx++)
		{
			int idx = arc_idx * points_num_per_arc + point_idx;
			if ((round(cg.p2d[idx].x) < tmp_mask.cols) && (cg.p2d[idx].x >= 0) && (round(cg.p2d[idx].y) < tmp_mask.rows) && (cg.p2d[idx].y >= 0))
			{
				if (tmp_mask.at<uchar>((int)round(cg.p2d[idx].y), (int)round(cg.p2d[idx].x)) != 0)
				{
					cg.seam_candidators_defisheye.push_back(cg.p2d[idx]);
					cg.seam_candidators_local.push_back(cg.v3d[idx]);
					isfound = true;
					break;
				}
			}
		}
		if (isfound) break; else noop;
	}
#else
	cg.seam_candidators_defisheye.push_back(cg_p1p8.p2d[1]);
	cg.seam_candidators_local.push_back(cg_p1p8.v3d[1]);
#endif
}
