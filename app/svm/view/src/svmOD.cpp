#include "svmOD.hpp"
#include "svmShaderString.hpp"
#include "svmWorld.hpp"
#include "svmLogger.hpp"
#include "svmError.hpp"
#include "svmTracker.h"
#include "svmIO.hpp"

#define LCA_TEST 1
#define DEBUG 1

sanOD::sanOD(sanXML* pxml, PM* ppm, sanCamera* pcameras, sanMOBS* pmobs, sanTextRenderer* ptextRenderer, VEHICLESIGNAL* pvehicle_signal)
{
	m_pxml = pxml;
	m_ppm = ppm;
	m_pcameras = pcameras;
	m_pmobs = pmobs;
	m_ptextRenderer = ptextRenderer;
	m_pvehicle_signal = pvehicle_signal;
	m_totalObjectCount = 0;
	m_ttcViolatedCount = 0;
}

sanOD::~sanOD()
{
	if (m_ptracker != nullptr)
	{
		delete m_ptracker;
		m_ptracker = nullptr;
	}
	if(!m_circle_target_shape.empty()) m_circle_target_shape.clear(); else noop;
	if (!m_previous_frame_pool.empty()) m_previous_frame_pool.clear(); else noop;
	m_odShader.~sanShader();
}

void sanOD::initialize()
{
	try
	{
		m_circle_target_gradient_unit_time_msec = m_pxml->m_od_parameter.target_gradient_time_msec / (2.0f * CIRCLE_TARGERT_GRADIENT_STEPS);

		//for layout 0
		this->generateVAB(nullptr, 0, 3, 0, GL_DYNAMIC_DRAW);

		//for layout 1
		this->generateVAB(nullptr, 0, 3, 0, GL_DYNAMIC_DRAW);

		generateCircleTarget(Point3f(0.0f, 0.0f, 0.0f));

		if (m_ptracker == nullptr)
		{
			m_ptracker = new sanTracker(m_pxml->m_od_parameter.coast_cycles_threshold);
		}
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2107001", __FUNCTION__ + delimiter(string(e.what())));
	}
}

vector<Point2f> sanOD::makeTargetMesh_2D(vector<Point2f>  rect_points)
{
	// order of rect_points as input
	//    i0 ----- i3
	//     |       | 
	//     |       |
	//    i1 ----- i2

	vector<Point2f> targetShape;

	if (rect_points.size() == 4)
	{
		// order of vertices as output
		//    v0 ----- v2
		//     |       | 
		//     |       |
		//    v1 ----- v3

		// top-left corner point
		Point2f v0 = rect_points[0];
		Point2f v1 = rect_points[1];
		Point2f v2 = rect_points[3];
		Point2f v3 = rect_points[2];

		targetShape.push_back(v0);
		targetShape.push_back(v1);
		targetShape.push_back(v2);
		targetShape.push_back(v2);
		targetShape.push_back(v1);
		targetShape.push_back(v3);
	}
	else noop;

	return targetShape;
}

vector<Point2f> sanOD::convert_object_to_point(SVM_OBJECT& one_object)
{	
	// p0---------p3
	//  |         |
	//  |         | 
	// p1---------p2
	vector<Point2f> rectangle_points;

	rectangle_points.push_back(Point2f(float(one_object.x),                    float(one_object.y))); // p0
	rectangle_points.push_back(Point2f(float(one_object.x),                    float(one_object.y + one_object.height))); // p1
	rectangle_points.push_back(Point2f(float(one_object.x + one_object.width), float(one_object.y + one_object.height))); // p2
	rectangle_points.push_back(Point2f(float(one_object.x + one_object.width), float(one_object.y))); // p3

	return rectangle_points;
}

// GLfloat* sanOD::getTargetMesh_2D(int camID, SVM_OBJECT& one_object, int& total_vertex_num)
// {
// 	float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

// 	vector<Point2f> points_in_fisheye = convert_object_to_point(one_object);
// 	vector<Point2f> points_in_defisheye = sanWorld::fisheye_to_defisheye(camID, m_pxml, points_in_fisheye);
// 	vector<Point2f> target_in_defisheye = makeTargetMesh_2D(points_in_defisheye);
// 	vector<Point2f> targetCV = sanWorld::normalize_points_in_defisheye(camID, m_pxml, target_in_defisheye); // based on CV coordinate system
// 	vector<Point2f> targetGL = sanWorld::CV2GL(targetCV);

// 	total_vertex_num = (int)targetGL.size();
// 	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 3), sizeof(GLfloat));
// 	if (vtx != nullptr)
// 	{
// 		for (int i = 0; i < total_vertex_num; i++)
// 		{
// 			vtx[3 * i + 0] = xflip * targetGL[i].x;
// 			vtx[3 * i + 1] = targetGL[i].y;
// 			vtx[3 * i + 2] = 0.0f;
// 		}
// 	}
// 	else
// 		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

// 	return vtx;
// }


GLfloat* sanOD::getTargetMesh_2D(int camID, SVM_OBJECT& one_object, int& total_vertex_num)
{
	float vertex_xnorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].cols - (m_pxml->m_rcam[camID].camview_offset.hleft + m_pxml->m_rcam[camID].camview_offset.hright));
	float vertex_ynorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].rows - (m_pxml->m_rcam[camID].camview_offset.vtop + m_pxml->m_rcam[camID].camview_offset.vbot));

	float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

	vector<Point2f> points_in_fisheye = convert_object_to_point(one_object);
	vector<Point2f> points_in_defisheye = sanWorld::fisheye_to_defisheye(camID, m_pxml, points_in_fisheye);
	vector<Point2f> target_in_defisheye = makeTargetMesh_2D(points_in_defisheye);

	total_vertex_num = (int)target_in_defisheye.size();
	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 3), sizeof(GLfloat));
	if (vtx != nullptr)
	{
		for (int i = 0; i < total_vertex_num; i++)
		{
			vtx[3 * i + 0] = xflip * ((target_in_defisheye[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			vtx[3 * i + 1] = -((target_in_defisheye[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
			vtx[3 * i + 2] = 0.0f;
		}
	}
	else
		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

	return vtx;
}

void sanOD::renderOD_2D(int vabt_idx, glm::vec3& color, float alpha, glm::mat4 mvp)
{
	sanError::glClearError();

	m_odShader.use();
	m_odShader.setMat4("mvp", mvp);
	m_odShader.setVec3("color", color);
	m_odShader.setFloat("alpha", alpha);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vabt_list[vabt_idx].vaoID);

	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_idx].vnum);

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__);
}

void sanOD::renderOD_3D(int vabt_idx, glm::vec3& color, float alpha, glm::mat4 mvp)
{
	sanError::glClearError();

	m_odShader.use();
	m_odShader.setMat4("mvp", mvp);
	m_odShader.setVec3("color", color);
	m_odShader.setFloat("alpha", alpha);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vabt_list[vabt_idx].vaoID);

	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_idx].vnum);

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__);
}

void sanOD::generateCircleTarget(Point3f center)
{
	float circle_target_radius = m_pxml->m_od_parameter.circle_target_radius;
	float cx = center.x;
	float cy = center.y;
	float r0 = 0.00f * circle_target_radius; // inner of the center circle(first circle)
	float r1 = 0.25f * circle_target_radius; // outer of the center circle(first circle)
	float r2 = 0.80f * circle_target_radius; // inner of the embracing circle(second circle)
	float r3 = 1.00f * circle_target_radius; // outer of the embracing circle(second circle)

	float inc_radian = (float)(2.0f * M_PI / CIRCLE_TARGET_DIVISION_NUM);
	for (int i = 1; i <= CIRCLE_TARGET_DIVISION_NUM; i++)
	{
		float uth = (float)(i * inc_radian); // upper theta
		Point3f up0(r0 * cos(uth) + cx, r0 * sin(uth) + cy, 0.0f);
		Point3f up1(r1 * cos(uth) + cx, r1 * sin(uth) + cy, 0.0f);
		Point3f up2(r2 * cos(uth) + cx, r2 * sin(uth) + cy, 0.0f);
		Point3f up3(r3 * cos(uth) + cx, r3 * sin(uth) + cy, 0.0f);

		float lth = (float)((i - 1) * inc_radian); // lower theta
		Point3f lp0(r0 * cos(lth) + cx, r0 * sin(lth) + cy, 0.0f);
		Point3f lp1(r1 * cos(lth) + cx, r1 * sin(lth) + cy, 0.0f);
		Point3f lp2(r2 * cos(lth) + cx, r2 * sin(lth) + cy, 0.0f);
		Point3f lp3(r3 * cos(lth) + cx, r3 * sin(lth) + cy, 0.0f);

		m_circle_target_shape.push_back(up0); //up0, lp0, up1	
		m_circle_target_shape.push_back(lp0);
		m_circle_target_shape.push_back(up1);

		m_circle_target_shape.push_back(up1); //up1, lp0, lp1
		m_circle_target_shape.push_back(lp0);
		m_circle_target_shape.push_back(lp1);

		m_circle_target_shape.push_back(up2); //up2, lp2, up3
		m_circle_target_shape.push_back(lp2);
		m_circle_target_shape.push_back(up3);

		m_circle_target_shape.push_back(up3); //up3, lp2, lp3
		m_circle_target_shape.push_back(lp2);
		m_circle_target_shape.push_back(lp3);
	}

	//////////////////////////////// rigtht rectangle at 0 degree 
	float hlength = circle_target_radius * 0.150f;
	float vlength = circle_target_radius * 0.100f;
	float new_cx = cx + r3 * (float)cos(0);
	float new_cy = cy - r3 * (float)sin(0);
	Point3f v0(new_cx + hlength, new_cy + vlength, 0.0f);
	Point3f v1(new_cx - 4.0f * hlength, new_cy, 0.0f);
	Point3f v2(new_cx + hlength, new_cy - vlength, 0.0f);

	m_circle_target_shape.push_back(v0);
	m_circle_target_shape.push_back(v1);
	m_circle_target_shape.push_back(v2);

	/////////////////////////////////// left rectangle at 180 degree
	new_cx = cx + r3 * (float)cos(M_PI);
	new_cy = cy - r3 * (float)sin(M_PI);
	v0 = Point3f(new_cx - hlength, new_cy - vlength, 0.0f);
	v1 = Point3f(new_cx - hlength, new_cy + vlength, 0.0f);
	v2 = Point3f(new_cx + 4.0f * hlength, new_cy, 0.0f);

	m_circle_target_shape.push_back(v0);
	m_circle_target_shape.push_back(v1);
	m_circle_target_shape.push_back(v2);

	float tmp = hlength; hlength = vlength;	vlength = tmp;

	///////////////////////////////////// top rectangle at 90 degree
	new_cx = cx + r3 * (float)cos(M_PI / 2.0);
	new_cy = cy - r3 * (float)sin(M_PI / 2.0);
	v0 = Point3f(new_cx - hlength, new_cy - vlength, 0.0f);
	v1 = Point3f(new_cx, new_cy + 4.0f * vlength, 0.0f);
	v2 = Point3f(new_cx + hlength, new_cy - vlength, 0.0f);

	m_circle_target_shape.push_back(v0);
	m_circle_target_shape.push_back(v1);
	m_circle_target_shape.push_back(v2);

	//////////////////////////////////// bottom rectangle at -90 degree
	new_cx = cx + r3 * (float)cos(-M_PI / 2.0);
	new_cy = cy - r3 * (float)sin(-M_PI / 2.0);
	v0 = Point3f(new_cx, new_cy - 4.0f * vlength, 0.0f);
	v1 = Point3f(new_cx - hlength, new_cy + vlength, 0.0f);
	v2 = Point3f(new_cx + hlength, new_cy + vlength, 0.0f);

	m_circle_target_shape.push_back(v0);
	m_circle_target_shape.push_back(v1);
	m_circle_target_shape.push_back(v2);
}

GLfloat* sanOD::getTargetMesh_3D(Point3f& center)
{
	int vertex_num = (int)m_circle_target_shape.size();
	GLfloat* vtx = (GLfloat*)calloc((size_t)(vertex_num * 3), sizeof(GLfloat));

	if (vtx != nullptr)
	{
		for (int i = 0; i < vertex_num; i++)
		{
			vtx[3 * i + 0] = (m_circle_target_shape[i].x + center.x);
			vtx[3 * i + 1] = (m_circle_target_shape[i].y + center.y);
			vtx[3 * i + 2] = (m_circle_target_shape[i].z + center.z);
		}
	}
	else
		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

	return vtx;
}


vector<Point2f> sanOD::pickout_candidates_with_min_distance(SVM_OBJECT& one_object)
{
	vector<Point2f> fisheye_p2d;
	
#if (0) // points of the bottom line
#define BOTTOM_LINE_DIVISION_NUM  (50)
	for(int i=0; i <= BOTTOM_LINE_DIVISION_NUM; i++)
		fisheye_p2d.push_back(Point2f(float(one_object.x + one_object.width * ((float)i / (float)(BOTTOM_LINE_DIVISION_NUM))), float(one_object.y + one_object.height)));
#else  // center point of the bottom line
	fisheye_p2d.push_back(Point2f(float(one_object.x + one_object.width/2), float(one_object.y + one_object.height)));
#endif

	return fisheye_p2d;
}

// vector<Point2f> sanOD::pickout_candidates_with_min_distance(SVM_OBJECT& one_object) {
//     vector<Point2f> fisheye_p2d;
//     if (one_object.width <= 0 || one_object.height <= 0) {
//         std::cerr << "Invalid bounding box" << std::endl;
//         return fisheye_p2d;
//     }

//     float unit_y = one_object.object_moving_direction.y;
//     if (unit_y < 0) {
//         fisheye_p2d.push_back(Point2f(
//             one_object.x + one_object.width, 
//             one_object.y + one_object.height
//         ));
//     } else {
//         fisheye_p2d.push_back(Point2f(
//             one_object.x,
//             one_object.y + one_object.height
//         ));
//     }
//     return fisheye_p2d;
// }


int choose_index_with_min_distance(vector<float>& distances)
{
	int final_index = 0;
	float min_distance = FLT_MAX;
	for (int index=0; index < (int)distances.size(); index++)
	{
		if (fabs(distances[index] - min_distance) <= FLT_EPSILON)
		{
			min_distance = distances[index];
			final_index = index;
		}
	}
	return final_index;
}



vector<ZONE_TYPE> sanOD::getObjectZoneType(MOBS_MARKERS* markers, Point3f object_pt3d_mm, float distance_mm_from_car)
{
	
	// auto start_time = std::chrono::steady_clock::now();

	vector<ZONE_TYPE> zone_type = { OUT_OF_ZONE, OUT_OF_ZONE};

	if (m_pxml->m_activation.od)
	{
		if (m_pxml->m_activation.mobs)
		{
			/////////////////////////////////////////// FRONT(MOIS) ////////
			if (sanWorld::isInsideZone(markers->mois_warning_pt3d_mm, object_pt3d_mm))
			{
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::MOIS_WARNING_ZONE;
				if (sanWorld::isInsideZone(markers->right_bsis_top_warning_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_TOP_WARNING_ZONE;
				else if (sanWorld::isInsideZone(markers->right_bsis_top_monitoring_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_TOP_MONITORING_ZONE;
				else if (sanWorld::isInsideZone(markers->left_bsis_top_warning_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::LEFT_BSIS_TOP_WARNING_ZONE;
				else if (sanWorld::isInsideZone(markers->left_bsis_top_monitoring_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::LEFT_BSIS_TOP_MONITORING_ZONE;
				else noop;
			}
			else if (sanWorld::isInsideZone(markers->mois_monitoring_pt3d_mm, object_pt3d_mm))
			{
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::MOIS_MONITORING_ZONE;
				if (sanWorld::isInsideZone(markers->right_bsis_top_warning_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_TOP_WARNING_ZONE;
				else if (sanWorld::isInsideZone(markers->right_bsis_top_monitoring_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_TOP_MONITORING_ZONE;
				else if (sanWorld::isInsideZone(markers->left_bsis_top_warning_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::LEFT_BSIS_TOP_WARNING_ZONE;
				else if (sanWorld::isInsideZone(markers->left_bsis_top_monitoring_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::LEFT_BSIS_TOP_MONITORING_ZONE;
				else noop;
			}

			/////////////////////////////////////////// RIGHT SIDE(BSIS)///////////////////
			else if (sanWorld::isInsideZone(markers->right_bsis_top_warning_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_TOP_WARNING_ZONE;
			else if (sanWorld::isInsideZone(markers->right_bsis_bot_warning_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_BOT_WARNING_ZONE;
			else if (sanWorld::isInsideZone(markers->right_bsis_top_monitoring_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_TOP_MONITORING_ZONE;
			else if (sanWorld::isInsideZone(markers->right_bsis_mid_monitoring_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_MID_MONITORING_ZONE;
			else if (sanWorld::isInsideZone(markers->right_bsis_bot_monitoring_pt3d_mm, object_pt3d_mm))
			{
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::RIGHT_BSIS_BOT_MONITORING_ZONE;
				if (sanWorld::isInsideZone(markers->right_rear_warning_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::RIGHT_REAR_WARNING_ZONE;
				else noop;
			}

			////////////////////////////////////////////// LEFT SIDE(BSIS)/////////////////
			else if (sanWorld::isInsideZone(markers->left_bsis_top_warning_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::LEFT_BSIS_TOP_WARNING_ZONE;
			else if (sanWorld::isInsideZone(markers->left_bsis_bot_warning_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::LEFT_BSIS_BOT_WARNING_ZONE;
			else if (sanWorld::isInsideZone(markers->left_bsis_top_monitoring_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::LEFT_BSIS_TOP_MONITORING_ZONE;
			else if (sanWorld::isInsideZone(markers->left_bsis_mid_monitoring_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::LEFT_BSIS_MID_MONITORING_ZONE;
			else if (sanWorld::isInsideZone(markers->left_bsis_bot_monitoring_pt3d_mm, object_pt3d_mm))
			{
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::LEFT_BSIS_BOT_MONITORING_ZONE;
				if (sanWorld::isInsideZone(markers->left_rear_warning_pt3d_mm, object_pt3d_mm))
					zone_type[SECONDARY_ZONE] = ZONE_TYPE::LEFT_REAR_WARNING_ZONE;
				else noop;
			}

			/////////////////////////////////////////// REAR ///////////////////////////
			else if (sanWorld::isInsideZone(markers->center_rear_warning_pt3d_mm, object_pt3d_mm))
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::CENTER_REAR_WARNING_ZONE;
			else
				zone_type[PRIMARY_ZONE] = ZONE_TYPE::OUT_OF_ZONE;
		}
		else
		{
			zone_type[PRIMARY_ZONE] = ((m_pxml->m_od_parameter.warning_min_distance <= distance_mm_from_car) &&
				(distance_mm_from_car <= m_pxml->m_od_parameter.warning_max_distance)) ? ZONE_TYPE::OD_WARNING_ZONE : ZONE_TYPE::OUT_OF_ZONE;
		}
	}
	else
	{
		zone_type[PRIMARY_ZONE] = ZONE_TYPE::OUT_OF_ZONE;
	}

	return zone_type;
}


void sanOD::extractBasicInformationFromObject(vector<SVM_OBJECT>& object_list /*[in/out]*/)
{
	try
	{
		for (int i = 0; i < (int)object_list.size(); i++)
		{
			vector<Point2f> fisheye_pts = pickout_candidates_with_min_distance(object_list[i]);
			vector<Point3f> gl_pts_mm = sanWorld::fisheye_to_GL_mm(object_list[i].camID, m_pxml, fisheye_pts); // millimeter points in GL space
			vector<Point3f> gl_pts = sanWorld::get_Logic_InGLOBAL(m_pxml, gl_pts_mm);

			// minimal distance
			vector<float> distances_mm_from_car = sanWorld::get_distance_from_vehicle_mm(m_pxml, gl_pts_mm); // millimeter distance
			int min_distance_index = choose_index_with_min_distance(distances_mm_from_car);			
			Point3f gl_pt3d_at_min_distance = gl_pts[min_distance_index];
			Point3f gl_pt3d_mm_at_min_distance = gl_pts_mm[min_distance_index];
			float min_distance_mm_from_car = distances_mm_from_car[min_distance_index];

			object_list[i].gx = gl_pt3d_at_min_distance.x;
			object_list[i].gy = gl_pt3d_at_min_distance.y;
			object_list[i].gz = gl_pt3d_at_min_distance.z;

			object_list[i].xmm = gl_pt3d_mm_at_min_distance.x;
			object_list[i].ymm = gl_pt3d_mm_at_min_distance.y;
			object_list[i].zmm = gl_pt3d_mm_at_min_distance.z;
			object_list[i].min_distance_mm_from_car = min_distance_mm_from_car; // millimeter distance from car


			///////////////////////
			//	v3 ----------- v2
			//  |				|
			//  |				|
			//	v0 ----------- v1
			///////////////////////
			vector<Point2f> bbox2d;
			bbox2d.push_back(Point2f((float)(object_list[i].x), (float)(object_list[i].y + object_list[i].height))); // v0
			bbox2d.push_back(Point2f((float)(object_list[i].x + object_list[i].width), (float)(object_list[i].y + object_list[i].height))); // v1
			bbox2d.push_back(Point2f((float)(object_list[i].x + object_list[i].width), (float)(object_list[i].y))); // v2
			bbox2d.push_back(Point2f((float)(object_list[i].x), (float)(object_list[i].y))); // v3
			object_list[i].bbox_pt3d_mm = sanWorld::fisheye_to_GL_mm(object_list[i].camID, m_pxml, bbox2d);

			vector<ZONE_TYPE> zone_type = { OUT_OF_ZONE, OUT_OF_ZONE };

			// apply ROI for each camera image before get zone type
			if (sanWorld::isInsideZone2d(m_pxml->m_mask_seam[object_list[i].camID], bbox2d[0]) ||
				sanWorld::isInsideZone2d(m_pxml->m_mask_seam[object_list[i].camID], bbox2d[1]) ||
				sanWorld::isInsideZone2d(m_pxml->m_mask_seam[object_list[i].camID], bbox2d[2]) ||
				sanWorld::isInsideZone2d(m_pxml->m_mask_seam[object_list[i].camID], bbox2d[3]))
			{
				zone_type = getObjectZoneType(m_pmobs->m_pmarkers, Point3f(object_list[i].xmm, object_list[i].ymm, object_list[i].zmm), object_list[i].min_distance_mm_from_car);
			}
			else noop;
			
			object_list[i].primary_zone_type = zone_type[PRIMARY_ZONE];
			object_list[i].secondary_zone_type = zone_type[SECONDARY_ZONE];
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

// syan_LCA

enum ObjectStateType { STATIONARY, MOVING_WITH_EGO, SLOWER_THAN_EGO, FASTER_THAN_EGO, UNKNOWN };

std::unordered_map<int, std::deque<float>> speed_history;
std::unordered_map<int, std::deque<ObjectStateType>> state_history;


float CalculateTrimmedMean(const std::vector<float>& speeds) 
{
    if (speeds.empty()) return 0.0f;
    if (speeds.size() == 1) return speeds[0];
    if (speeds.size() == 2) return (speeds[0] + speeds[1]) / 2.0f;

    std::vector<float> sorted_speeds = speeds;
    std::sort(sorted_speeds.begin(), sorted_speeds.end());

    size_t size = sorted_speeds.size();
    float sum = 0.0f;

    for (size_t i = 1; i < size - 1; ++i) 
    {
        sum += sorted_speeds[i];
    }

    return sum / (size - 2);
}


float CalculateAverageRelativeSpeed(int object_id, float object_speed) 
{
	size_t low_speed_count = 0;

	auto& history = speed_history[object_id];
	
	history.push_back(object_speed);
	if (history.size() > 5) history.pop_front();
	
	float prev_speed = history.front();

	std::deque<float> filtered_history;
	filtered_history.push_back(prev_speed);
	
	for (const auto& speed : filtered_history) 
	{
		if (speed < -10.0f) low_speed_count++;
	}

	if (low_speed_count >= filtered_history.size() * 0.7) return 0.0f;
	
	if (filtered_history.size() < 5) return filtered_history.back();

	return filtered_history.back();
}


std::string toString(ObjectStateType state) 
{
    switch (state) 
	{
        case ObjectStateType::STATIONARY:
            return "STATIONARY";
        case ObjectStateType::SLOWER_THAN_EGO:
            return "SLOWER";
        case ObjectStateType::FASTER_THAN_EGO:
            return "FASTER";
        case ObjectStateType::MOVING_WITH_EGO:
            return "MOVING_WITH_EGO";
        default:
            return "UNKNOWN";
    }
}

#if (1)
	bool IsStateConsistent(int object_id, ObjectStateType new_state)
	{
		std::unordered_map<ObjectStateType, int> frequency;

		auto& history = state_history[object_id];
		history.push_back(new_state);

		if (history.size() > 10) history.pop_front();		
		if (history.size() < 10) return false;
		
		for (const auto& state : history) 
		{
			frequency[state]++;
		}
		
		auto max_state = std::max_element(frequency.begin(), frequency.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
		
		return max_state->first == new_state;
	}

#else // Compare with stored history state (this is must be all the same as new state) and new state
	bool IsStateConsistent(int object_id, ObjectStateType new_state) 
	{
		auto& history = state_history[object_id];
		history.push_back(new_state);

		if (history.size() > 10) history.pop_front();
		if (history.size() < 10) return false;
		
		return std::all_of(history.begin(), history.end(),[&](const ObjectStateType& state) { return state == new_state; });
	}
#endif


ObjectStateType CalculateRelativeObjectState(SVM_OBJECT& obj, float ego_velocity) 
{
    ObjectStateType state = ObjectStateType::UNKNOWN;

    float avg_relative_speed = CalculateAverageRelativeSpeed(obj.objectID, obj.object_moving_speed);

    if (avg_relative_speed == 0 || avg_relative_speed < -5.0f)
        state = ObjectStateType::STATIONARY;
    else if (avg_relative_speed > -5.0f && avg_relative_speed < -1.0f)
        state = ObjectStateType::SLOWER_THAN_EGO;
    else if (avg_relative_speed > 1.0)
        state = ObjectStateType::FASTER_THAN_EGO;
    else
        state = ObjectStateType::MOVING_WITH_EGO;

    // checking State Consistent
    if (!IsStateConsistent(obj.objectID, state)) // if the consecutive state occurs fewer than 3 times, retain the previous state
	{
        auto& history = state_history[obj.objectID];
			if (!history.empty()) 
			{
				state = history.back(); // Continue Prev State
			} 
			else
			{
				state = ObjectStateType::UNKNOWN; // No History
			}
    }

    #if (0)
        // std::cout << "[CalculateRelativeObjectState] ======================\n";
        // std::cout << "  Object ID             : " << obj.objectID << "\n";
        std::cout << "  Object Relative Speed : " << avg_relative_speed << " km/h\n";
        // std::cout << "  Ego Vehicle Speed     : " << ego_velocity << " km/h\n";
        std::cout << "  ObjectStateType       : " << toString(state) << "\n";
        // std::cout << "===================================================\n";
        // std::cout << " " << std::endl;
	#else
		(void) ego_velocity;
    #endif

    return state;
}


void sanOD::extractExtendedInformationFromSameObjectBetweenFrames(SVM_OBJECT& prev_object/*[in]*/, SVM_OBJECT& curr_object /*[in/out]*/)
{
	curr_object.delta_xmm = curr_object.xmm - prev_object.xmm;
	curr_object.delta_ymm = curr_object.ymm - prev_object.ymm;
	curr_object.delta_zmm = curr_object.zmm - prev_object.zmm;
	curr_object.delta_timestamp = curr_object.timestamp - prev_object.timestamp;

	double object_moving_distance_mm = sqrt( pow(curr_object.delta_xmm, 2.0) + pow(curr_object.delta_ymm, 2.0));

    if (DBL_EPSILON < object_moving_distance_mm && FLT_EPSILON < curr_object.min_distance_mm_from_car) {

		float unit_x = (float)(curr_object.delta_xmm / object_moving_distance_mm);
        float unit_y = (float)(curr_object.delta_ymm / object_moving_distance_mm);
        curr_object.object_moving_direction = Point3f(unit_x, unit_y, 0.0f);

        double moving_km = object_moving_distance_mm / 1000000.0;
        double time_hour = (curr_object.delta_timestamp / (double)CLOCKS_PER_SEC) / 3600.0;

        double speed_kmh = (DBL_EPSILON < time_hour) ? (moving_km / time_hour) : 0.0;
        double object_speed_y = speed_kmh * unit_y;

        curr_object.object_moving_speed = (float)(object_speed_y);
        curr_object.estimated_object_speed = (float)(object_speed_y + m_pvehicle_signal->m_vehicle_velocity);
    } 
	else 
	{
        curr_object.object_moving_direction = Point3f(0.0f, 0.0f, 0.0f);
        curr_object.object_moving_speed = 0.0f;
    }
    curr_object.TTC = calcTTC(curr_object);
}


enum MOVING_TYPE { NOT_MOVING=0, PARALLEL_MOVING=1, COLLISION_MOVING=2, OPPOSITE_MOVING=3 };

#define DISTANCE(a, b)  (float)sqrt( ((a).x - (b).x) * ((a).x - (b).x) + ((a).y - (b).y) * ((a).y - (b).y) + ((a).z - (b).z) * ((a).z - (b).z) )
#define SAFE_ACOS(val)  (float)( (val) <= -1.0f ? M_PI : (val) >= 1.0f ? 0.0f : acos(val) )

int calcIntersectionPoint_3D(vector<float>& plane_coefficients, Point3f object_position, Point3f object_direction, Point3f& intersection_point /*[out]*/)
{
	int moving_state = MOVING_TYPE::NOT_MOVING;

	if (plane_coefficients.size() == 4)
	{
		//L(t) = (x + dx * t, y + dy * t, z + dz * t), ax + by + cz + d = 0;
		float t_numerator = (plane_coefficients[0] * object_position.x + plane_coefficients[1] * object_position.y + plane_coefficients[2] * object_position.z + plane_coefficients[3]);
		float t_denumerator = (plane_coefficients[0] * object_direction.x + plane_coefficients[1] * object_direction.y + plane_coefficients[2] * object_direction.z); // It gets zero when not moving or parallel moving with the plane

		if (FLT_EPSILON < fabs(t_denumerator))
		{
			float t = t_numerator / t_denumerator;
			Point3f ip(object_position.x - object_direction.x * t, object_position.y - object_direction.y * t, object_position.z - object_direction.z * t); // intersection point
			Point3f ip_position_vec = ip - object_position;
			double ip_position_vec_norm = sqrt(pow(ip_position_vec.x, 2.0) + pow(ip_position_vec.y, 2.0) + pow(ip_position_vec.z, 2.0));
			if (DBL_EPSILON < ip_position_vec_norm)
			{
				ip_position_vec.x = (float)(ip_position_vec.x / ip_position_vec_norm);
				ip_position_vec.y = (float)(ip_position_vec.y / ip_position_vec_norm);
				ip_position_vec.z = (float)(ip_position_vec.z / ip_position_vec_norm);
			}
			else noop;

			float dot_product = (float)ip_position_vec.dot(object_direction);
			float included_angle_degree = (float)(SAFE_ACOS(dot_product) * 180.0f / M_PI);

			if (included_angle_degree < 90.0f)
			{
				moving_state = MOVING_TYPE::COLLISION_MOVING;
				intersection_point = ip;
			}
			else
			{
				moving_state = MOVING_TYPE::OPPOSITE_MOVING;
				intersection_point = Point3f(FLT_MAX, FLT_MAX, FLT_MAX);
			}
		}
		else
		{
			float direction_norm = (float)sqrt(object_direction.x * object_direction.x + object_direction.y * object_direction.y + object_direction.z * object_direction.z);
			moving_state = (FLT_EPSILON < direction_norm) ? MOVING_TYPE::PARALLEL_MOVING : MOVING_TYPE::NOT_MOVING;
			intersection_point = Point3f(FLT_MAX, FLT_MAX, FLT_MAX);
		}
	}
	else
	{
		throw runtime_error(__FILE__ + string(" $ it must have 4 points to calculate the intersection point"));
	}

	return moving_state;
}

float sanOD::calcTTC_in_Front(SVM_OBJECT& object)
{
	Point3f object_position_mm(object.xmm, object.ymm, 0.0f);
	Point3f object_direction = object.object_moving_direction; // unit vector
	float object_speed = object.object_moving_speed;	
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	vector<Point3f> vehicle_zone = { vehicle_pt3d_mm[3], vehicle_pt3d_mm[2], vehicle_pt3d_mm[0], vehicle_pt3d_mm[1] }; // to match the input parameters of isInsideZone()

	Point3f front_collision_point_mm(0.0f, 0.0f, 0.0f);// intersection point with front plane of the vehicle
	vector<float> front_plane_equation = { 0.0f, 1.0f, 0.0f, -vehicle_pt3d_mm[0].y }; // y = v0.y --> a=0, b=1, c=0, d = -v0.y where ax + by + cz + d = 0
	int front_state = calcIntersectionPoint_3D(front_plane_equation, object_position_mm, object_direction, front_collision_point_mm);

	float TTC = INFINITY;
	if ((FLT_EPSILON < object_speed) && (1 < object.hit_streak))
	{
		if (front_state == MOVING_TYPE::COLLISION_MOVING)
		{
			if (sanWorld::isInsideZone(vehicle_zone, front_collision_point_mm))
				TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
			else
			{
				if (isinf(object.previous_TTC))
					TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
				else
					TTC = object.previous_TTC * (object.previous_object_speed / object_speed);
			}
		}
	}

	object.previous_TTC = TTC;
	object.previous_object_speed = object_speed;

	return TTC;
}

float sanOD::calcTTC_on_Right(SVM_OBJECT& object)
{
	Point3f object_position_mm(object.xmm, object.ymm, 0.0f);
	Point3f object_direction = object.object_moving_direction; // unit vector
	float object_speed = object.object_moving_speed; // vehicle_speed + pure_object_speed
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	vector<Point3f> vehicle_zone = { vehicle_pt3d_mm[3], vehicle_pt3d_mm[2], vehicle_pt3d_mm[0], vehicle_pt3d_mm[1] }; // to match the input parameters of isInsideZone()

	Point3f right_collision_point_mm(0.0f, 0.0f, 0.0f); // intersection point
	vector<float> right_plane_equation = { 1.0f, 0.0f, 0.0f, -vehicle_pt3d_mm[0].x };  // x = v0.x --> a=1, b=0, c=0, d = -v0.x where ax + by + cz + d = 0
	int right_state = calcIntersectionPoint_3D(right_plane_equation, object_position_mm, object_direction, right_collision_point_mm);

	float TTC = INFINITY;
	if ((FLT_EPSILON < object_speed) && (1 < object.hit_streak))	
	{
		if (right_state == MOVING_TYPE::COLLISION_MOVING)
		{
			if (sanWorld::isInsideZone(vehicle_zone, right_collision_point_mm))
				TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
			else
			{
				if (isinf(object.previous_TTC))
					TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
				else
					TTC = object.previous_TTC * (object.previous_object_speed / object_speed);
			}
		}
	}

	object.previous_TTC = TTC;
	object.previous_object_speed = object_speed;

	return TTC;
}

float sanOD::calcTTC_in_Rear(SVM_OBJECT& object)
{
	Point3f object_position_mm(object.xmm, object.ymm, 0.0f);
	Point3f object_direction = object.object_moving_direction; // unit vector
	float object_speed = object.object_moving_speed;
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	vector<Point3f> vehicle_zone = { vehicle_pt3d_mm[3], vehicle_pt3d_mm[2], vehicle_pt3d_mm[0], vehicle_pt3d_mm[1] }; // to match the input parameters of isInsideZone()

	Point3f rear_collision_point_mm(0.0f, 0.0f, 0.0f);//intersection point
	vector<float> rear_plane_equation = { 0.0f, 1.0f, 0.0f, -vehicle_pt3d_mm[1].y };  // y = v1.y --> a=0, b=1, c=0, d = -v1.y where ax + by + cz + d = 0
	int rear_state = calcIntersectionPoint_3D(rear_plane_equation, object_position_mm, object_direction, rear_collision_point_mm);

	float TTC = INFINITY;
	if ((FLT_EPSILON < object_speed) && (1 < object.hit_streak))
	{
		if (rear_state == MOVING_TYPE::COLLISION_MOVING)
		{
			if (sanWorld::isInsideZone(vehicle_zone, rear_collision_point_mm))
				TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
			else 
			{
				if (isinf(object.previous_TTC))
					TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
				else
					TTC = object.previous_TTC * (object.previous_object_speed / object_speed);
			}
		}
	}

	object.previous_TTC = TTC;
	object.previous_object_speed = object_speed;

	return TTC;
}

float sanOD::calcTTC_on_Left(SVM_OBJECT& object)
{	
	Point3f object_position_mm(object.xmm, object.ymm, 0.0f);
	Point3f object_direction = object.object_moving_direction; // unit vector
	float object_speed = object.object_moving_speed; // vehicle_speed + pure_object_speed
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);
	vector<Point3f> vehicle_zone = { vehicle_pt3d_mm[3], vehicle_pt3d_mm[2], vehicle_pt3d_mm[0], vehicle_pt3d_mm[1] }; // to match the input parameters of isInsideZone()

	Point3f left_collision_point_mm(0.0f, 0.0f, 0.0f); // intersection point
	vector<float> left_plane_equation = { 1.0f, 0.0f, 0.0f, -vehicle_pt3d_mm[2].x }; // x = v2.x --> a=1, b=0, c=0, d = -v2.x  where ax + by + cz + d = 0
	int left_state = calcIntersectionPoint_3D(left_plane_equation, object_position_mm, object_direction, left_collision_point_mm);

	float TTC = INFINITY;
	if ((FLT_EPSILON < object_speed) && (1 < object.hit_streak))
	{
		if (left_state == MOVING_TYPE::COLLISION_MOVING)
		{
			if (sanWorld::isInsideZone(vehicle_zone, left_collision_point_mm))
				TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
			else
			{
				if (isinf(object.previous_TTC))
					TTC = 3600.0f * (object.min_distance_mm_from_car / 1000000.0f) / object_speed;
				else
					TTC = object.previous_TTC * (object.previous_object_speed / object_speed);
			}
		}
	}

	object.previous_TTC = TTC;
	object.previous_object_speed = object_speed;

	return TTC;
}

float sanOD::calcTTC(SVM_OBJECT& object) // Time To Collision
{
	/*         |  front  |
	           |---------|
	           |         |
	    left   |   car   |  right
	           |         |
	           |---------|
	           |   rear  |    	*/         

	float TTC = INFINITY;
	Point3f object_position(object.xmm, object.ymm, 0.0f);
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);

	if (FLT_EPSILON < object.min_distance_mm_from_car)
	{
		if ((vehicle_pt3d_mm[0].x <= object_position.x) && (object_position.x <= vehicle_pt3d_mm[2].x) && (vehicle_pt3d_mm[0].y <= object_position.y)) // when an object is in front
		{
			TTC = calcTTC_in_Front(object);
		}
		else if (((vehicle_pt3d_mm[0].x <= object_position.x) && (object_position.x <= vehicle_pt3d_mm[2].x) && (object_position.y <= vehicle_pt3d_mm[3].y))) // when an object is in rear
		{
			TTC = calcTTC_in_Rear(object);
		}
		else if (object_position.x < vehicle_pt3d_mm[0].x) // when an object is on the left
		{
			TTC = calcTTC_on_Left(object);
		}
		else if (vehicle_pt3d_mm[1].x < object_position.x) // when an object is on the right
		{
			TTC = calcTTC_on_Right(object);
		}
		else noop;
	}
	else
		TTC = 0.0f;

	return TTC;
}

vector<SVM_OBJECT> sanOD::convertQuadrantCoords(vector<EXT_OBJECT>& frame_extented_objects)
{
	vector<SVM_OBJECT> svm_objects;

	for (auto& frame_object : frame_extented_objects)
	{
		if (0 <= frame_object.cam_id)
		{
			svm_objects.push_back(SVM_OBJECT(frame_object.cam_id, frame_object.class_id, frame_object.object_id, 
							frame_object.x, frame_object.y, frame_object.width, frame_object.height,
							frame_object.timestamp, frame_object.coast_cycles, frame_object.hit_streak));
		}
		else noop;		
	}

	return svm_objects;
}

vector<SVM_OBJECT> sanOD::extractObjectInformation(vector<SVM_OBJECT>& frame_objects)
{
	try
	{
		map<int, SVM_OBJECT> current_frame_pool;

		if (m_pxml->m_activation.od)
		{			
			extractBasicInformationFromObject(frame_objects);

			for (auto& frame_object_iter : frame_objects)  // traveling the frame objects to remove duplicate objects and extract the object information
			{
				int frame_object_key = frame_object_iter.objectID;
				map<int, SVM_OBJECT>::iterator frame_pool_iter = current_frame_pool.find(frame_object_key);
				if (frame_pool_iter == current_frame_pool.end())
				{					
					map<int, SVM_OBJECT>::iterator previous_frame_pool_iter = m_previous_frame_pool.find(frame_object_key);
					SVM_OBJECT previous_object;
					if (previous_frame_pool_iter != m_previous_frame_pool.end())
						previous_object = m_previous_frame_pool[frame_object_key];
					else noop;
					
					extractExtendedInformationFromSameObjectBetweenFrames(previous_object, frame_object_iter);
					current_frame_pool[frame_object_key] = frame_object_iter;
				}
				else noop;
			}

			// update global object pool
			if (!m_previous_frame_pool.empty()) m_previous_frame_pool.clear(); else noop;
			m_previous_frame_pool.insert(current_frame_pool.begin(), current_frame_pool.end());
		}
		else noop;

		// manage all objects from every frame
		static map<int, SVM_OBJECT> pool_manager;

		if (pool_manager.empty())
			pool_manager.insert(current_frame_pool.begin(), current_frame_pool.end());
		else
		{
			// update or insert new objects
			for (auto& current_frame_pool_iter : current_frame_pool)
				pool_manager[current_frame_pool_iter.first] = current_frame_pool_iter.second; 

			// delete objects
			map<int, sanTrack> tracks = m_ptracker->GetTracks();
			for (auto pool_manager_iter = pool_manager.begin(); pool_manager_iter != pool_manager.end();)
			{
				map<int, sanTrack>::iterator tracks_iter = tracks.find(pool_manager_iter->first);
				if (tracks_iter == tracks.end())
					pool_manager_iter = pool_manager.erase(pool_manager_iter);
				else
					pool_manager_iter++;
			}
		}

		// because of return type
		vector<SVM_OBJECT> return_object_list;
		for (auto& pool_manager_iter : pool_manager)
			return_object_list.push_back(pool_manager_iter.second);

		return return_object_list;
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2207401", __FUNCTION__ + delimiter(string(e.what())));
	}
}

vector<SVM_OBJECT> sanOD::applyAlertPolicy(vector<SVM_OBJECT>& target_objects/*[in]*/)
{
	vector<SVM_OBJECT> alert_objects;

	try
	{
		if (m_pxml->m_activation.od)
		{
			for (auto& iter : target_objects)
			{
				if (m_pxml->m_od_parameter.min_hit_streak < iter.hit_streak)
				{
					if (m_pxml->m_activation.mobs)
					{
						bool mois_cond = m_pvehicle_signal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.mois_speed_max; /* && m_pvehicle_signal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.mois_speed_min */
						bool bsis_cond = m_pvehicle_signal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.bsis_speed_max; /* && m_pvehicle_signal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.bsis_speed_min */
						bool rear_cond = m_pvehicle_signal->m_vehicle_velocity <= m_pxml->m_mobs_parameter.reverse_speed_max; /* && (m_pvehicle_signal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.reverse_speed_min) */
						bool lca_cond = m_pvehicle_signal->m_vehicle_velocity  > m_pxml->m_mobs_parameter.bsis_lca_speed_min; /* && (m_pvehicle_signal->m_vehicle_velocity >= m_pxml->m_mobs_parameter.bsis_lca_speed_min)*/
						
						///////////////////////////////// FRONT(MOIS) /////////////////////////////////
						if (iter.primary_zone_type == MOIS_WARNING_ZONE || iter.primary_zone_type == MOIS_MONITORING_ZONE)
						{
							if ((m_pvehicle_signal->m_trigger.gear != GEAR_REVERSE))
							{
								if (mois_cond)
								{
									if(iter.primary_zone_type == MOIS_WARNING_ZONE)
									{
										iter.alert_level = ALERT_LEVEL1;
										alert_objects.push_back(iter);
									}
									else if(iter.primary_zone_type == MOIS_MONITORING_ZONE)
									{
										iter.alert_level = ALERT_LEVEL2;
										alert_objects.push_back(iter);
									}
									else noop;
								}
								else if (bsis_cond)
								{
									if( ((m_pvehicle_signal->m_trigger.turn_signal != TURN_SIGNAL_LEFT) && (iter.secondary_zone_type == RIGHT_BSIS_TOP_WARNING_ZONE)) ||
										((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_LEFT) && (iter.secondary_zone_type == LEFT_BSIS_TOP_WARNING_ZONE)) )
										{
											if(bsis_cond)
											{
												iter.alert_level = ALERT_LEVEL1;
												alert_objects.push_back(iter);
											}
											else noop;
										}
									
									if( ((m_pvehicle_signal->m_trigger.turn_signal != TURN_SIGNAL_LEFT) && (iter.secondary_zone_type == RIGHT_BSIS_TOP_MONITORING_ZONE)) ||
										((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_LEFT) && (iter.secondary_zone_type == LEFT_BSIS_TOP_MONITORING_ZONE)) )
										{
											if(bsis_cond)
											{
												iter.alert_level = ALERT_LEVEL2;
												alert_objects.push_back(iter);
											}
											else noop;
										}
								}
								else 
								{
									iter.alert_level = NO_ALERT;
									alert_objects.push_back(iter);
								}									
							}
							else noop;
						}

						///////////////////////////////// (BSIS) /////////////////////////////
						else if ( ((m_pvehicle_signal->m_trigger.turn_signal != TURN_SIGNAL_LEFT) && ((iter.primary_zone_type == RIGHT_BSIS_TOP_WARNING_ZONE) || (iter.primary_zone_type == RIGHT_BSIS_BOT_WARNING_ZONE))) ||
								  ((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_LEFT) && ((iter.primary_zone_type == LEFT_BSIS_TOP_WARNING_ZONE) || (iter.primary_zone_type == LEFT_BSIS_BOT_WARNING_ZONE))))
						{
							if (m_pvehicle_signal->m_trigger.gear != GEAR_REVERSE)
							{
								if(bsis_cond)
								{
									iter.alert_level = ALERT_LEVEL1;
									alert_objects.push_back(iter);
								}
								else if(lca_cond && (iter.classID == 1) && (iter.camID != 2) && 
									(((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_LEFT) && (iter.primary_zone_type == LEFT_BSIS_BOT_WARNING_ZONE)) ||
									((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_RIGHT) && (iter.primary_zone_type == RIGHT_BSIS_BOT_WARNING_ZONE))))
								{
									ObjectStateType object_state = CalculateRelativeObjectState(iter, m_pvehicle_signal->m_vehicle_velocity);
									switch (object_state)
									{
										case FASTER_THAN_EGO:
										case MOVING_WITH_EGO:
										case SLOWER_THAN_EGO:
											iter.alert_level = ALERT_LCA;
											break;
										default:
											break;
									}
									alert_objects.push_back(iter);
								}
								else 
								{
									iter.alert_level = NO_ALERT;
									alert_objects.push_back(iter);
								}								
							}
							else noop;
						}
						else if ( ((m_pvehicle_signal->m_trigger.turn_signal != TURN_SIGNAL_LEFT) && ((iter.primary_zone_type == RIGHT_BSIS_TOP_MONITORING_ZONE) || (iter.primary_zone_type == RIGHT_BSIS_MID_MONITORING_ZONE) || (iter.primary_zone_type == RIGHT_BSIS_BOT_MONITORING_ZONE))) ||
								  ((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_LEFT) && ((iter.primary_zone_type == LEFT_BSIS_TOP_MONITORING_ZONE) || (iter.primary_zone_type == LEFT_BSIS_MID_MONITORING_ZONE) || (iter.primary_zone_type == LEFT_BSIS_BOT_MONITORING_ZONE))) )
						{
							if (m_pvehicle_signal->m_trigger.gear != GEAR_REVERSE)
							{
								if(bsis_cond)
								{
									iter.alert_level = ALERT_LEVEL2;
									alert_objects.push_back(iter);
								}

								else if(lca_cond && (iter.classID == 1) && (iter.object_moving_speed != 0) && 
									(((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_LEFT) && (iter.primary_zone_type == LEFT_BSIS_MID_MONITORING_ZONE || iter.primary_zone_type == LEFT_BSIS_BOT_MONITORING_ZONE)) ||
									((m_pvehicle_signal->m_trigger.turn_signal == TURN_SIGNAL_RIGHT) && (iter.primary_zone_type == RIGHT_BSIS_MID_MONITORING_ZONE || iter.primary_zone_type == RIGHT_BSIS_BOT_MONITORING_ZONE))))
								{
									ObjectStateType object_state = CalculateRelativeObjectState(iter, m_pvehicle_signal->m_vehicle_velocity);
									switch (object_state)
									{
										case FASTER_THAN_EGO:
											iter.alert_level = ALERT_LCA;
											break;
										default:
											break;
									}
									alert_objects.push_back(iter);
								}
								else 
								{
									iter.alert_level = NO_ALERT;
									alert_objects.push_back(iter);
								}
							}
							else noop;
						}

						///////////////////////////////// REAR ///////////////////////////////////////
						else if ((iter.primary_zone_type == CENTER_REAR_WARNING_ZONE) && rear_cond)
						{
							if ((m_pvehicle_signal->m_trigger.gear == GEAR_REVERSE) /* && (iter.TTC <= m_pxml->m_mobs_parameter.reverse_ttc_warning) */)
							{
								iter.alert_level = ALERT_LEVEL1;
								alert_objects.push_back(iter);
							}
							else noop;
						}
						else
						{
							iter.alert_level = NO_ALERT;
							alert_objects.push_back(iter);
						}
					}
					else
					{
						if (iter.primary_zone_type == OD_WARNING_ZONE)
						{
							iter.alert_level = ALERT_OD;
							alert_objects.push_back(iter);
						}
						else
						{
							iter.alert_level = NO_ALERT;
							alert_objects.push_back(iter);
						}
					}
				}
				else noop;
			}
		}	
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2207501", __FUNCTION__ + delimiter(string(e.what())));
	}

	return alert_objects;
}

vector<SVM_OBJECT> sanOD::removeDuplicateObjects(vector<SVM_OBJECT>& target_objects /*[in], [out]*/, float iou_threshold /*[in]*/)
{
	vector<SVM_OBJECT> new_target_objects;

	try
	{
		if (m_pxml->m_activation.od)
		{
			std::vector<ORG_OBJECT> objects;

			for (auto& iter : target_objects) // reconstruct rectangles
			{
				float minx = FLT_MAX, maxx = 0.0f, miny = FLT_MAX, maxy = 0.0f;

				if ((3 < iter.width) && (5 < iter.height))
				{
					for (int i = 0; i < 4; i++)
					{
						minx = min(minx, iter.bbox_pt3d_mm[i].x);
						maxx = max(maxx, iter.bbox_pt3d_mm[i].x);
						miny = min(miny, iter.bbox_pt3d_mm[i].y);
						maxy = max(maxy, iter.bbox_pt3d_mm[i].y);
					}
					ORG_OBJECT obj(iter.classID, (int)minx, (int)miny, (int)(maxx - minx), (int)(maxy - miny));
					objects.push_back(obj);
				}
			}

			std::vector<std::vector<float>> iou_matrix;
			iou_matrix.resize(objects.size(), std::vector<float>(objects.size()));

			for (int i = 0; i < (int)objects.size(); i++)
			{
				for (int j = 0; j < (int)objects.size(); j++)
				{
					iou_matrix[i][j] = sanTracker::CalculateIoU(objects[i], objects[j]);
				}
			}

			std::map<int, SVM_OBJECT> duplicate_detection_map;
			for (int i = 0; i < (int)objects.size(); i++)
			{
				for (int j = i + 1; j < (int)objects.size(); j++)
				{
					if (iou_threshold <= iou_matrix[i][j] && (objects[i].class_id == objects[j].class_id))
					{
						duplicate_detection_map[j] = target_objects[j];
					}
					else noop;
				}
			}

			for (int idx = 0; idx < (int)objects.size(); idx++)
			{
				auto dupl_iter = duplicate_detection_map.find(idx);
				if (dupl_iter == duplicate_detection_map.end())
				{
					new_target_objects.push_back(target_objects[idx]);
				}
				else noop;
			}
		}
		else noop;
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2207601", __FUNCTION__ + delimiter(string(e.what())));
	}

	return new_target_objects;
}

void sanOD::drawLayout0(glm::mat4& vcvm, vector<SVM_OBJECT>& alert_objects/*[in]*/)
{
	try
	{
		static int circle_target_gradient_idx = 0;
		static bool circle_target_gradient_direction_flag = true;
		static clock_t od_rendering_previous_clock = clock();

		int resource_idx = 0;
		int vertex_num = (int)m_circle_target_shape.size();

		glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

		int currentObjectCount = 0;

		// glm::mat4 mvp_gl = m_ppm->opm * vcvm;
		// float scale_gl = m_pxml->m_od_parameter.info_scale_3d * m_ptextRenderer->m_fontSize / ((float)m_pxml->m_arrangement.poster.width);
		// float line_height_gl = m_ptextRenderer->m_fontSize * scale_gl;

		for (auto& iter : alert_objects)
		{
			currentObjectCount++;

			// updating the target
			Point3f gl_pt3d_at_min_distance(iter.gx, iter.gy, iter.gz);
			GLfloat* target = getTargetMesh_3D(gl_pt3d_at_min_distance);
			this->updateVAB(resource_idx, target, (GLuint)vertex_num, 3, 0, GL_DYNAMIC_DRAW);
			if (target != nullptr) free(target);
			else noop;

			// drawing the target
			float target_alpha = 0.8f * (float)circle_target_gradient_idx / (float)CIRCLE_TARGERT_GRADIENT_STEPS;
			target_alpha = min(max(0.30f, target_alpha), 1.0f);
			renderOD_3D(resource_idx, m_color_pallet[iter.classID], target_alpha, m_ppm->opm * vcvm);

			// drawing info text
			glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f);
			if (!isinf(iter.TTC) && iter.TTC <= m_pxml->m_mobs_parameter.mois_ttc_warning)
			{
				color = glm::vec3(1.0f, 0.0f, 0.0f);
				m_ttcViolatedCount++;
			}
		}

		// drawing info text
		m_totalObjectCount += currentObjectCount;

		if (m_pxml->m_od_parameter.info_enable)
		{
			// glm::mat4 mvp_cv = glm::ortho(0.0f, (float)m_pxml->m_layout[0].width, (float)m_pxml->m_layout[0].height, 0.0f);
			// float scale_cv = m_pxml->m_od_parameter.info_scale_2d;
			// float line_height_cv = m_ptextRenderer->m_fontSize * scale_cv;

			// m_ptextRenderer->renderText(to_mystring(m_ttcViolatedCount, 0), glm::vec3(20, 2.0f * line_height_cv, 0.0f), TEXT_RENDER_MODE::TEXT_CV, mvp_cv, scale_cv, glm::vec3(1.0f, 0.0f, 0.0f));
			// m_ptextRenderer->renderText(to_mystring(m_totalObjectCount, 0), glm::vec3(20, 3.0f * line_height_cv, 0.0f), TEXT_RENDER_MODE::TEXT_CV, mvp_cv, scale_cv);
		}

		clock_t current_clock = clock();
		float delta_clock_msec = (float)((1000 * current_clock - od_rendering_previous_clock) / (float)CLOCKS_PER_SEC); // msec

		if (m_circle_target_gradient_unit_time_msec <= delta_clock_msec)
		{
			(circle_target_gradient_direction_flag) ? circle_target_gradient_idx++ : circle_target_gradient_idx--;
			od_rendering_previous_clock = current_clock;
			if ((circle_target_gradient_idx == 0) || (circle_target_gradient_idx == CIRCLE_TARGERT_GRADIENT_STEPS)) circle_target_gradient_direction_flag = !circle_target_gradient_direction_flag;
			else noop;
		}
		else noop;
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.od)
		{
			m_pxml->m_activation.od = false;
			runtime_error  err_msg = logger.svm_fatal("C2207701", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}

void sanOD::drawLayout1_3D(glm::mat4& vcvm, vector<SVM_OBJECT>& alert_objects)
{
	try
	{
		int resource_idx = 1;

		glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

		switch (m_pxml->m_layout[1].view_mode)
		{
		case CAMVIEW3D_FRONT: 
		case CAMVIEW3D_RIGHT:
		case CAMVIEW3D_REAR:
		case CAMVIEW3D_LEFT:
		{
			int vertex_num = (int)m_circle_target_shape.size();

			// glm::mat4 mvp_gl = m_ppm->ppm * vcvm;
			// float scale_gl = m_pxml->m_od_parameter.info_scale_3d * m_ptextRenderer->m_fontSize / ((float)m_pxml->m_arrangement.poster.width);
			// float line_height_gl = m_ptextRenderer->m_fontSize * scale_gl;

			for (auto& iter : alert_objects)
			{
				if (iter.primary_zone_type != ZONE_TYPE::OUT_OF_ZONE)
				{
					// updating the target						
					Point3f gl_pt3d_at_min_distance(iter.gx, iter.gy, iter.gz);
					GLfloat* target = getTargetMesh_3D(gl_pt3d_at_min_distance);
					this->updateVAB(resource_idx, target, (GLuint)vertex_num, 3, 0, GL_DYNAMIC_DRAW);
					if (target != nullptr)	free(target);
					else noop;

					// drawing the target
					renderOD_3D(resource_idx, m_color_pallet[iter.classID], 0.5f, m_ppm->ppm * vcvm);

					// drawing info text
					glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f);
					if (!isinf(iter.TTC) && iter.TTC <= m_pxml->m_mobs_parameter.mois_ttc_warning)
					{
						color = glm::vec3(1.0f, 0.0f, 0.0f);
					}
				}
				else noop;
			}
			break;
		}
		default:
			break;
		}
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.od)
		{
			m_pxml->m_activation.od = false;
			runtime_error  err_msg = logger.svm_fatal("C2207801", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}

void sanOD::drawLayout1_2D(vector<SVM_OBJECT>& alert_objects)
{
	try
	{
		int resource_idx = 1;

		glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

		switch (m_pxml->m_layout[1].view_mode)
		{
		case CAMVIEW2D_FRONT:
		{
			// render objects 
			GLint camID = m_pxml->m_layout[1].view_mode - CAMVIEW2D_FRONT;
			
			glm::mat4 mvp_cv = glm::ortho(	0.0f,
											m_pxml->m_resolution.display.width - m_pxml->m_rcam[camID].camview_offset.hleft - m_pxml->m_rcam[camID].camview_offset.hright,
											m_pxml->m_resolution.display.height - m_pxml->m_rcam[camID].camview_offset.vtop - m_pxml->m_rcam[camID].camview_offset.vbot,
											0.0f);

			float scale_cv = m_pxml->m_od_parameter.info_scale_2d;
			float line_height_cv = m_ptextRenderer->m_fontSize * scale_cv;

			for (auto& iter : alert_objects)
			{
				if (camID == iter.camID)
				{
					// generating the target
					int total_vertex_num = 0;
					GLfloat* triangles = this->getTargetMesh_2D(camID, iter, total_vertex_num);
					this->updateVAB(resource_idx, triangles, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
					if (triangles != nullptr) free(triangles);
					else noop;

					// drawing the target
					renderOD_2D(resource_idx, m_color_pallet[iter.classID], 0.2f, glm::mat4(1.0f));

					// render info text
					Point2f p2f = Point2f((float)(iter.x), (float)(iter.y));
					Point2f p2d = sanWorld::fisheye_to_defisheye(camID, m_pxml, p2f);
					Point2f pt = Point2f(p2d.x - m_pxml->m_rcam[camID].camview_offset.hleft, p2d.y - m_pxml->m_rcam[camID].camview_offset.vtop);
					(void) pt;

					glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f);

					if (!isinf(iter.TTC) && iter.TTC <= m_pxml->m_mobs_parameter.mois_ttc_warning)
					{
						color = glm::vec3(1.0f, 0.0f, 0.0f);
					}

					if (m_pxml->m_od_parameter.info_enable)
					{
						m_ptextRenderer->renderText(to_mystring(iter.min_distance_mm_from_car / 1000.0f) + " m", glm::vec3(pt.x, pt.y + line_height_cv * 0.0f, 0.0f), TEXT_RENDER_MODE::TEXT_CV, mvp_cv, scale_cv, color);
						// m_ptextRenderer->renderText(to_mystring(iter.TTC) + " s", glm::vec3(pt.x, pt.y + line_height_cv * 1.0f, 0.0f), TEXT_RENDER_MODE::TEXT_CV, mvp_cv, scale_cv, color);
					}
				}
				else noop;
			}

			break;
		}
		case CAMVIEW2D_RIGHT:
		case CAMVIEW2D_REAR:
		case CAMVIEW2D_LEFT:
		{
			GLint camID = m_pxml->m_layout[1].view_mode - CAMVIEW2D_FRONT;
			for (auto& iter : alert_objects)
			{
				if (camID == iter.camID)
				{
					// generating the target
					int total_vertex_num = 0;
					GLfloat* triangles = this->getTargetMesh_2D(camID, iter, total_vertex_num);
					this->updateVAB(resource_idx, triangles, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
					if (triangles != nullptr) free(triangles);
					else noop;

					// drawing the target
					renderOD_2D(resource_idx, m_color_pallet[iter.classID], 0.2f, glm::mat4(1.0f));
				}
				else noop;
			}
			break;
		}
		case CAMVIEW2D_ADD0:
			noop;
			break;
		default:
			break;
		}
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.od)
		{
			m_pxml->m_activation.od = false;
			runtime_error  err_msg = logger.svm_fatal("C2207901", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}
