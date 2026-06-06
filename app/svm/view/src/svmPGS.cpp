#include "svmPGS.hpp"
#include "svmShaderString.hpp"
#include "svmWorld.hpp"
#include "svmFromFile.hpp"
#include "svmError.hpp"

#define	ENABLE_TEXTURE	(1)
#define ENABLE_QUADS	(1)

sanPGS::sanPGS(sanXML* pxml, PM* ppm, sanCamera* pcameras, VEHICLESIGNAL* pvehicleSignal) : m_pxml(pxml), m_ppm(ppm), m_pcameras(pcameras), m_pvehicleSignal(pvehicleSignal)
{
}

sanPGS::~sanPGS()
{
	if (!m_pgs_wheel_box_mm.empty()) 
	{
		m_pgs_wheel_box_mm.clear(); 
		vector<Point3f>().swap(m_pgs_wheel_box_mm);
	}
	else noop;
	if (!m_front_left_pt3d_mm.empty()) 
	{
		m_front_left_pt3d_mm.clear(); 
		vector<Point3f>().swap(m_front_left_pt3d_mm);
	}
	else noop;
	if (!m_front_right_pt3d_mm.empty()) 
	{
		m_front_right_pt3d_mm.clear(); 
		vector<Point3f>().swap(m_front_right_pt3d_mm);
	}
	else noop;
	if (!m_rear_left_pt3d_mm.empty()) 
	{
		m_rear_left_pt3d_mm.clear(); 
		vector<Point3f>().swap(m_rear_left_pt3d_mm);
	}
	else noop;
	if (!m_rear_right_pt3d_mm.empty()) 
	{
		m_rear_right_pt3d_mm.clear(); 
		vector<Point3f>().swap(m_rear_right_pt3d_mm);
	}
	else noop;
	if (!m_center_pt3d_mm.empty()) 
	{
		m_center_pt3d_mm.clear(); 
		vector<Point3f>().swap(m_center_pt3d_mm);
	}
	else noop;
	if (!m_vehicle_heading_rad.empty()) 
	{
		m_vehicle_heading_rad.clear(); 
		vector<float>().swap(m_vehicle_heading_rad);
	}
	else noop;

	if (m_texDriveLeft)
	{
		glDeleteTextures(1, &m_texDriveLeft);
		m_texDriveLeft = 0;
	}
	if (m_texDriveRight)
	{
		glDeleteTextures(1, &m_texDriveRight);
		m_texDriveRight = 0;
	}
	if (m_texReverseLeft)
	{
		glDeleteTextures(1, &m_texReverseLeft);
		m_texReverseLeft = 0;
	}
	if (m_texReverseRight)
	{
		glDeleteTextures(1, &m_texReverseRight);
		m_texReverseRight = 0;
	}

	m_pgsShader.~sanShader();
}

void sanPGS::initialize()
{
	try
	{
		generate_and_update_PGSTexture();
		generate_wheel_box_mm(m_pgs_wheel_box_mm, m_rear_track_width_mm, m_front_track_width_mm, m_wheel_base_mm);
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2105001", __FUNCTION__+ delimiter(string(e.what())));
	}
}


void sanPGS::generate_and_update_PGSTexture()
{
	try
	{
		TEXTURE_INFO texture_info_drive_left = sanFromFile::read_texture_from_file(_TEXTURES_PATH_ + string("/pgs/drive_left.png"), false, true);
		sanVABT::generateTexture(GL_TEXTURE0, &m_texDriveLeft);
		sanVABT::updateTexture(GL_TEXTURE0, m_texDriveLeft, texture_info_drive_left);

		TEXTURE_INFO texture_info_drive_right = sanFromFile::read_texture_from_file(_TEXTURES_PATH_ + string("/pgs/drive_right.png"), false, true);
		sanVABT::generateTexture(GL_TEXTURE0, &m_texDriveRight);
		sanVABT::updateTexture(GL_TEXTURE0, m_texDriveRight, texture_info_drive_right);

		TEXTURE_INFO texture_info_reverse_left = sanFromFile::read_texture_from_file(_TEXTURES_PATH_ + string("/pgs/reverse_left.png"), true, false);
		sanVABT::generateTexture(GL_TEXTURE0, &m_texReverseLeft);
		sanVABT::updateTexture(GL_TEXTURE0, m_texReverseLeft, texture_info_reverse_left);

		TEXTURE_INFO texture_info_reverse_right = sanFromFile::read_texture_from_file(_TEXTURES_PATH_ + string("/pgs/reverse_right.png"), true, false);
		sanVABT::generateTexture(GL_TEXTURE0, &m_texReverseRight);
		sanVABT::updateTexture(GL_TEXTURE0, m_texReverseRight, texture_info_reverse_right);
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanPGS::generate_wheel_box_mm(vector<Point3f>& wheel_position /*[out]*/, float& rear_track_width_mm /*[out]*/, float& front_track_width_mm/*[out]*/, float& wheel_base_mm/*[out]*/)
{
	// based on GL
	/* the order of vehicle box points
	v0 ----- v2
	|	     |
	| 	     |
	|	     |
	v1 ----- v3	 */

	//vector<Point3f> vehicle_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM); //v0(top-left), v1(bottom-left), v2(top-right), v3(bottom-right)

	////rear_track_width_mm = m_pxml->m_vehicle_spec.rear_track;
	//rear_track_width_mm = m_pxml->m_vehicle_spec.front_track;
	//front_track_width_mm = m_pxml->m_vehicle_spec.front_track;
	//wheel_base_mm = m_pxml->m_vehicle_spec.wheel_base;

	//float wheel_thickness_mm = m_pxml->m_pgs_parameter.unit_sample_width_mm;
	//float center_x = (vehicle_mm[1].x + vehicle_mm[3].x) / 2.0f;

	//wheel_position.push_back(Point3f(center_x - m_pxml->m_vehicle_spec.front_track / 2.0f, vehicle_mm[0].y - m_pxml->m_vehicle_spec.front_overhang, 0.0f));
	//wheel_position.push_back(Point3f(center_x - m_pxml->m_vehicle_spec.front_track / 2.0f, vehicle_mm[1].y + m_pxml->m_vehicle_spec.rear_overhang, 0.0f));
	//wheel_position.push_back(Point3f(center_x + m_pxml->m_vehicle_spec.front_track / 2.0f, vehicle_mm[2].y - m_pxml->m_vehicle_spec.front_overhang, 0.0f));
	//wheel_position.push_back(Point3f(center_x + m_pxml->m_vehicle_spec.front_track / 2.0f, vehicle_mm[3].y + m_pxml->m_vehicle_spec.rear_overhang, 0.0f));


	vector<Point3f> vehicle_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM); //v0(top-left), v1(bottom-left), v2(top-right), v3(bottom-right)

	float wheel_thickness_mm = m_pxml->m_pgs_parameter.unit_sample_width_mm;
	float center_x = (vehicle_mm[1].x + vehicle_mm[3].x) / 2.0f;

	wheel_position.push_back(Point3f(center_x - m_pxml->m_vehicle_spec.front_track / 2.0f + wheel_thickness_mm / 2.0f, vehicle_mm[0].y - m_pxml->m_vehicle_spec.front_overhang, 0.0f));
	wheel_position.push_back(Point3f(center_x - m_pxml->m_vehicle_spec.front_track / 2.0f + wheel_thickness_mm / 2.0f, vehicle_mm[1].y + m_pxml->m_vehicle_spec.rear_overhang, 0.0f));
	wheel_position.push_back(Point3f(center_x + m_pxml->m_vehicle_spec.front_track / 2.0f - wheel_thickness_mm / 2.0f, vehicle_mm[2].y - m_pxml->m_vehicle_spec.front_overhang, 0.0f));
	wheel_position.push_back(Point3f(center_x + m_pxml->m_vehicle_spec.front_track / 2.0f - wheel_thickness_mm / 2.0f, vehicle_mm[3].y + m_pxml->m_vehicle_spec.rear_overhang, 0.0f));

	rear_track_width_mm = wheel_position[3].x - wheel_position[1].x;
	front_track_width_mm = wheel_position[3].x - wheel_position[1].x;
	wheel_base_mm = m_pxml->m_vehicle_spec.wheel_base;

}

void sanPGS::generate_wheel_trajectory(float vehicle_initial_heading /* [in] degree */)
{
	if (!m_front_left_pt3d_mm.empty()) m_front_left_pt3d_mm.clear(); else noop;
	if (!m_front_right_pt3d_mm.empty()) m_front_right_pt3d_mm.clear(); else noop;
	if (!m_rear_left_pt3d_mm.empty()) m_rear_left_pt3d_mm.clear(); else noop;
	if (!m_rear_right_pt3d_mm.empty()) m_rear_right_pt3d_mm.clear(); else noop;
	if (!m_center_pt3d_mm.empty()) m_center_pt3d_mm.clear(); else noop;
	if (!m_vehicle_heading_rad.empty()) m_vehicle_heading_rad.clear(); else noop;

	Point3f front_left_pt3d_mm(0.0f, 0.0f, 0.0f);
	Point3f front_right_pt3d_mm(0.0f, 0.0f, 0.0f);
	Point3f rear_left_pt3d_mm(0.0f, 0.0f, 0.0f);
	Point3f rear_right_pt3d_mm(0.0f, 0.0f, 0.0f);
	Point3f center_pt3d_mm(0.0f, 0.0f, 0.0f);

	float vehicle_velocity = m_pvehicleSignal->m_vehicle_velocity;

	if (vehicle_velocity == 0)	vehicle_velocity = 5.0f;
	else noop;

	vehicle_velocity *= (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE ? -1.0f : 1.0f);

	float vehicle_heading_rad = float(vehicle_initial_heading * M_PI / 180.0f);
	float vehicle_velocity_mps = vehicle_velocity * 1000.0f / 3600.0f;
	int sample_num = 1 + (int)(m_pxml->m_pgs_parameter.guide_max_distance / m_pxml->m_pgs_parameter.unit_sample_length_mm);		// consider fixed vertical thickness or not? because this is integer number
	float sample_time = abs(m_pxml->m_pgs_parameter.guide_max_distance / (1000.0f * vehicle_velocity_mps * sample_num));// in second

	float wheel_angle = m_pvehicleSignal->m_wheel_angle;
	float wheel_angle_rad = float(wheel_angle * M_PI / 180.0f);

	//   wheel box
	//  v0 ----- v2  <- front axis
	//  |        |      wheelbase
	//  |        |
	//  v1 ------v3  <- rear axis

	Point3f base_position = (wheel_angle >= 0.0f) ? m_pgs_wheel_box_mm[1] : m_pgs_wheel_box_mm[3];
	Point3f travel_position(0.0f);

	float track_plus = (m_rear_track_width_mm + m_front_track_width_mm) / 2.0f;
	float track_minus = (m_rear_track_width_mm - m_front_track_width_mm) / 2.0f;

	for (int i = 0; i < sample_num; i++)
	{
		if (i == 0)
		{
			travel_position = base_position + ((wheel_angle >= 0.0f) ? 1.0f : -1.0f) * Point3f(m_rear_track_width_mm * sin(vehicle_heading_rad), -m_rear_track_width_mm * cos(vehicle_heading_rad), 0.0f);
		}
		else
		{
			float dx = 1000.0f * vehicle_velocity_mps * cos(vehicle_heading_rad);	// mm
			float dy = 1000.0f * vehicle_velocity_mps * sin(vehicle_heading_rad);	// mm
			float dh = (1000.0f * vehicle_velocity_mps) / ((m_rear_track_width_mm - m_front_track_width_mm) / 2.0f + m_wheel_base_mm / tan(wheel_angle_rad));	// radian

			travel_position.x += sample_time * dx;
			travel_position.y += sample_time * dy;
			travel_position.z += 0;
			vehicle_heading_rad += sample_time * dh;
		}

		if (wheel_angle >= 0.0f)	// Left wheel based, distance travelled calculated on right wheel
		{
			rear_right_pt3d_mm = travel_position;
			rear_left_pt3d_mm = rear_right_pt3d_mm - Point3f(m_rear_track_width_mm * sin(vehicle_heading_rad), -m_rear_track_width_mm * cos(vehicle_heading_rad), 0.0f);
			front_left_pt3d_mm = rear_right_pt3d_mm + Point3f(m_wheel_base_mm * cos(vehicle_heading_rad) - track_plus * sin(vehicle_heading_rad), m_wheel_base_mm * sin(vehicle_heading_rad) + track_plus * cos(vehicle_heading_rad), 0.0f);
			front_right_pt3d_mm = rear_right_pt3d_mm + Point3f(m_wheel_base_mm * cos(vehicle_heading_rad) - track_minus * sin(vehicle_heading_rad), m_wheel_base_mm * sin(vehicle_heading_rad) + track_minus * cos(vehicle_heading_rad), 0.0f);
			center_pt3d_mm = rear_right_pt3d_mm + Point3f(cos(vehicle_heading_rad) * m_wheel_base_mm / 2.0f - sin(vehicle_heading_rad) * m_rear_track_width_mm / 2.0f, sin(vehicle_heading_rad) * m_wheel_base_mm / 2.0f + cos(vehicle_heading_rad) * m_rear_track_width_mm / 2.0f, 0.0f);
		}
		else						// Right wheel based, distance travelled calculated on left wheel
		{
			rear_left_pt3d_mm = travel_position;
			rear_right_pt3d_mm = rear_left_pt3d_mm + Point3f(m_rear_track_width_mm * sin(vehicle_heading_rad), -m_rear_track_width_mm * cos(vehicle_heading_rad), 0.0f);
			front_left_pt3d_mm = rear_left_pt3d_mm + Point3f(m_wheel_base_mm * cos(vehicle_heading_rad) + track_minus * sin(vehicle_heading_rad), m_wheel_base_mm * sin(vehicle_heading_rad) - track_minus * cos(vehicle_heading_rad), 0.0f);
			front_right_pt3d_mm = rear_left_pt3d_mm + Point3f(m_wheel_base_mm * cos(vehicle_heading_rad) + track_plus * sin(vehicle_heading_rad), m_wheel_base_mm * sin(vehicle_heading_rad) - track_plus * cos(vehicle_heading_rad), 0.0f);
			center_pt3d_mm = rear_left_pt3d_mm + Point3f(cos(vehicle_heading_rad) * m_wheel_base_mm / 2.0f + sin(vehicle_heading_rad) * m_rear_track_width_mm / 2.0f, sin(vehicle_heading_rad) * m_wheel_base_mm / 2.0f - cos(vehicle_heading_rad) * m_rear_track_width_mm / 2.0f, 0.0f);
		}

		m_front_left_pt3d_mm.push_back(front_left_pt3d_mm);
		m_front_right_pt3d_mm.push_back(front_right_pt3d_mm);
		m_rear_left_pt3d_mm.push_back(rear_left_pt3d_mm);
		m_rear_right_pt3d_mm.push_back(rear_right_pt3d_mm);
		m_center_pt3d_mm.push_back(center_pt3d_mm);
		m_vehicle_heading_rad.push_back(vehicle_heading_rad);

#if (0)
		ofstream out_points;
		string file_path = _ARRAYS_PATH_ + "/points";
		out_points.open(file_path.c_str(), std::ofstream::out | std::ofstream::trunc);

		// overlapped grids 
		for (int i = 0; i < m_front_left_pt3d_mm.size(); i++)
		{
			out_points	<< m_front_left_pt3d_mm[i].x << " " << m_front_left_pt3d_mm[i].y << " " << m_front_left_pt3d_mm[i].z << " " \
						<< m_front_right_pt3d_mm[i].x << " " << m_front_right_pt3d_mm[i].y << " " << m_front_right_pt3d_mm[i].z << " " \
						<< m_rear_left_pt3d_mm[i].x << " " << m_rear_left_pt3d_mm[i].y << " " << m_rear_left_pt3d_mm[i].z << " " \
						<< m_rear_right_pt3d_mm[i].x << " " << m_rear_right_pt3d_mm[i].y << " " << m_rear_right_pt3d_mm[i].z << " " \
						<< m_center_pt3d_mm[i].x << " " << m_center_pt3d_mm[i].y << " " << m_center_pt3d_mm[i].z << " " \
						<< endl;
		}
		out_points.close();
#endif
	}
}

void sanPGS::generateLeftRightPoints_3D(vector<Point3f> center_pt3d /*[in]*/, float horizontal_distance /*[in]*/, vector<Point3f>& right_pt3d /*[out]*/, vector<Point3f>& left_pt3d/*[out]*/)
{
	Point3f up(0.0f, 0.0f, 1.0f);

	for (int i = 0; i < (int)center_pt3d.size(); i++)
	{
		Point3f dir;
		if (i == 0)
		{
			dir = center_pt3d[i + 1] - center_pt3d[i];
		}
		else if (i == (int)center_pt3d.size() - 1)
		{
			dir = center_pt3d[i] - center_pt3d[i - 1];
		}
		else
		{
			dir = center_pt3d[i + 1] - center_pt3d[i - 1];
		}


		float norm_dir = (float)sqrt(pow(dir.x, 2) + pow(dir.y, 2) + pow(dir.z, 2));
		dir /= norm_dir;

		Point3f right_dir = dir.cross(up);
		Point3f left_dir = up.cross(dir);

		right_pt3d.push_back(center_pt3d[i] + (horizontal_distance / 2.0f) * right_dir);
		left_pt3d.push_back(center_pt3d[i] + (horizontal_distance / 2.0f) * left_dir);
	}
}

GLfloat* sanPGS::generatePGSMeshQuad_3D(vector<Point3f> pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/)
{
#if(ENABLE_QUADS)
	total_vertex_num = ((int)pt3d_mm.size()) * 2;
#else
	total_vertex_num = ((int)pt3d_mm.size() - 1) * 6;
#endif
	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 5), sizeof(GLfloat));
	if (vtx != nullptr)
	{
		int vtx_base_idx = 0;

		vector<Point3f> right_pt3d_mm, left_pt3d_mm;
		vector<Point3f> right_pt3d, left_pt3d;

		generateLeftRightPoints_3D(pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, right_pt3d_mm, left_pt3d_mm);
		right_pt3d = sanWorld::get_Logic_InGLOBAL(m_pxml, right_pt3d_mm);
		left_pt3d = sanWorld::get_Logic_InGLOBAL(m_pxml, left_pt3d_mm);

#if(ENABLE_QUADS)
		for (int i = 0; i < (int)pt3d_mm.size(); i++)
		{
			vtx[vtx_base_idx + 0] = left_pt3d[i].x;			vtx[vtx_base_idx + 1] = left_pt3d[i].y;			vtx[vtx_base_idx + 2] = left_pt3d[i].z;			vtx[vtx_base_idx + 3] = 0.0f;			vtx[vtx_base_idx + 4] = i * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 5] = right_pt3d[i].x;		vtx[vtx_base_idx + 6] = right_pt3d[i].y;		vtx[vtx_base_idx + 7] = right_pt3d[i].z;		vtx[vtx_base_idx + 8] = 1.0f;			vtx[vtx_base_idx + 9] = i * 1.0f / (pt3d_mm.size() - 1);

			vtx_base_idx += 10;
		}
#else
		for (int i = 0; i < pt3d_mm.size() - 1; i++)
		{
			vtx[vtx_base_idx + 0] = left_pt3d[i + 1].x;			vtx[vtx_base_idx + 1] = left_pt3d[i + 1].y;			vtx[vtx_base_idx + 2] = left_pt3d[i + 1].z;			vtx[vtx_base_idx + 3] = 0.0f;			vtx[vtx_base_idx + 4] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 5] = left_pt3d[i].x;				vtx[vtx_base_idx + 6] = left_pt3d[i].y;				vtx[vtx_base_idx + 7] = left_pt3d[i].z;				vtx[vtx_base_idx + 8] = 0.0f;			vtx[vtx_base_idx + 9] = i * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 10] = right_pt3d[i + 1].x;		vtx[vtx_base_idx + 11] = right_pt3d[i + 1].y;		vtx[vtx_base_idx + 12] = right_pt3d[i + 1].z;		vtx[vtx_base_idx + 13] = 1.0f;			vtx[vtx_base_idx + 14] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 15] = right_pt3d[i + 1].x;		vtx[vtx_base_idx + 16] = right_pt3d[i + 1].y;		vtx[vtx_base_idx + 17] = right_pt3d[i + 1].z;		vtx[vtx_base_idx + 18] = 1.0f;			vtx[vtx_base_idx + 19] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 20] = left_pt3d[i].x;			vtx[vtx_base_idx + 21] = left_pt3d[i].y;			vtx[vtx_base_idx + 22] = left_pt3d[i].z;			vtx[vtx_base_idx + 23] = 0.0f;			vtx[vtx_base_idx + 24] = i * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 25] = right_pt3d[i].x;			vtx[vtx_base_idx + 26] = right_pt3d[i].y;			vtx[vtx_base_idx + 27] = right_pt3d[i].z;			vtx[vtx_base_idx + 28] = 1.0f;			vtx[vtx_base_idx + 29] = i * 1.0f / (pt3d_mm.size() - 1);
			
			vtx_base_idx += 30;
		}
#endif
		if (!right_pt3d_mm.empty()) right_pt3d_mm.clear(); else noop;
		if (!left_pt3d_mm.empty()) left_pt3d_mm.clear(); else noop;
		if (!right_pt3d.empty()) right_pt3d.clear(); else noop;
		if (!left_pt3d.empty()) left_pt3d.clear(); else noop;
	}
	else
		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

	return vtx;
}

// GLfloat* sanPGS::generatePGS_2D(int camID, float pgs_length_mm, vector<Point3f> left_pt3d_mm /*[in]*/, vector<Point3f> right_pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/)
// {
// 	float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

// 	int point_num = 1 + (int)((pgs_length_mm + m_pxml->m_vehicle_spec.rear_overhang) / m_pxml->m_pgs_parameter.unit_sample_length_mm);
// 	point_num = (point_num <= (int)left_pt3d_mm.size()) ? point_num : (int)left_pt3d_mm.size();
	
// 	vector<Point3f> left_outer_pt3d_mm, left_inner_pt3d_mm, right_inner_pt3d_mm, right_outer_pt3d_mm;
// 	generateLeftRightPoints_3D(left_pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, left_inner_pt3d_mm, left_outer_pt3d_mm);
// 	generateLeftRightPoints_3D(right_pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, right_outer_pt3d_mm, right_inner_pt3d_mm);

// 	Point3f right_inner = right_inner_pt3d_mm[point_num - 1];
// 	Point3f left_inner = left_inner_pt3d_mm[point_num - 1];
// 	float piece_x = (right_inner.x - left_inner.x) / (float)(PGS_LINE_POINTS_NUM - 1);
// 	float piece_y = (right_inner.y - left_inner.y) / (float)(PGS_LINE_POINTS_NUM - 1);
// 	vector<Point3f> rl_pt3d_mm, rl_lower_pt3d_mm, rl_upper_pt3d_mm;
// 	for (int i = 0; i < PGS_LINE_POINTS_NUM; i++)
// 	{
// 		rl_pt3d_mm.push_back(right_inner + Point3f(-i * piece_x, -i * piece_y, 0.0f));
// 	}
// 	generateLeftRightPoints_3D(rl_pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, rl_upper_pt3d_mm, rl_lower_pt3d_mm);

// 	// collect points
// 	vector<Point3f> final_pt3d_mm;
// 	for (int i = 0; i < point_num; i++)
// 	{
// 		final_pt3d_mm.push_back(right_inner_pt3d_mm[i]);
// 		final_pt3d_mm.push_back(right_outer_pt3d_mm[i]);
// 	}

// 	for (int i = 0; i < (int)rl_pt3d_mm.size(); i++)
// 	{
// 		final_pt3d_mm.push_back(rl_lower_pt3d_mm[i]);
// 		final_pt3d_mm.push_back(rl_upper_pt3d_mm[i]);
// 	}

// 	for (int i = (int)point_num - 1; i >= 0; i--)
// 	{
// 		final_pt3d_mm.push_back(left_outer_pt3d_mm[i]);
// 		final_pt3d_mm.push_back(left_inner_pt3d_mm[i]);
// 	}

// 	vector<Point2f> final_pt2d, viewport_final_pt2d;
// 	final_pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, final_pt3d_mm);
// 	viewport_final_pt2d = sanWorld::normalize_points_in_defisheye(camID, m_pxml, final_pt2d);


// 	// create vtx
// 	total_vertex_num = 4 * point_num + 2 * PGS_LINE_POINTS_NUM;
// 	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 3), sizeof(GLfloat));

// 	if (vtx != nullptr)
// 	{
// 		int vtx_base_idx = 0;
// 		for (int i = 0; i < total_vertex_num; i++)
// 		{
// 			vtx[vtx_base_idx + 0] = xflip * viewport_final_pt2d[i].x;
// 			vtx[vtx_base_idx + 1] = -viewport_final_pt2d[i].y;
// 			vtx[vtx_base_idx + 2] = 0.0f;

// 			vtx_base_idx += 3;
// 		}

// 		if (!left_outer_pt3d_mm.empty()) left_outer_pt3d_mm.clear(); else noop;
// 		if (!left_inner_pt3d_mm.empty()) left_inner_pt3d_mm.clear(); else noop;
// 		if (!right_inner_pt3d_mm.empty()) right_inner_pt3d_mm.clear(); else noop;
// 		if (!right_outer_pt3d_mm.empty()) right_outer_pt3d_mm.clear(); else noop;
		
// 		if (!rl_pt3d_mm.empty()) rl_pt3d_mm.clear(); else noop;
// 		if (!rl_lower_pt3d_mm.empty()) rl_lower_pt3d_mm.clear(); else noop;
// 		if (!rl_upper_pt3d_mm.empty()) rl_upper_pt3d_mm.clear(); else noop;

// 		if (!final_pt3d_mm.empty()) final_pt3d_mm.clear(); else noop;
// 		if (!final_pt2d.empty()) final_pt2d.clear(); else noop;
// 		if (!viewport_final_pt2d.empty()) viewport_final_pt2d.clear(); else noop;
// 	}
// 	else
// 		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

// 	return vtx;
// }


GLfloat* sanPGS::generatePGS_2D(int camID, float pgs_length_mm, vector<Point3f> left_pt3d_mm /*[in]*/, vector<Point3f> right_pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/)
{
	float vertex_xnorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].cols - (m_pxml->m_rcam[camID].camview_offset.hleft + m_pxml->m_rcam[camID].camview_offset.hright));
	float vertex_ynorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].rows - (m_pxml->m_rcam[camID].camview_offset.vtop + m_pxml->m_rcam[camID].camview_offset.vbot));

	float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

	int point_num = 1 + (int)((pgs_length_mm + m_pxml->m_vehicle_spec.rear_overhang) / m_pxml->m_pgs_parameter.unit_sample_length_mm);
	point_num = (point_num <= (int)left_pt3d_mm.size()) ? point_num : (int)left_pt3d_mm.size();
	
	vector<Point3f> left_outer_pt3d_mm, left_inner_pt3d_mm, right_inner_pt3d_mm, right_outer_pt3d_mm;
	generateLeftRightPoints_3D(left_pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, left_inner_pt3d_mm, left_outer_pt3d_mm);
	generateLeftRightPoints_3D(right_pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, right_outer_pt3d_mm, right_inner_pt3d_mm);

	Point3f right_inner = right_inner_pt3d_mm[point_num - 1];
	Point3f left_inner = left_inner_pt3d_mm[point_num - 1];
	float piece_x = (right_inner.x - left_inner.x) / (float)(PGS_LINE_POINTS_NUM - 1);
	float piece_y = (right_inner.y - left_inner.y) / (float)(PGS_LINE_POINTS_NUM - 1);
	vector<Point3f> rl_pt3d_mm, rl_lower_pt3d_mm, rl_upper_pt3d_mm;
	for (int i = 0; i < PGS_LINE_POINTS_NUM; i++)
	{
		rl_pt3d_mm.push_back(right_inner + Point3f(-i * piece_x, -i * piece_y, 0.0f));
	}
	generateLeftRightPoints_3D(rl_pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, rl_upper_pt3d_mm, rl_lower_pt3d_mm);

	// collect points
	vector<Point3f> final_pt3d_mm;
	for (int i = 0; i < point_num; i++)
	{
		final_pt3d_mm.push_back(right_inner_pt3d_mm[i]);
		final_pt3d_mm.push_back(right_outer_pt3d_mm[i]);
	}

	for (int i = 0; i < (int)rl_pt3d_mm.size(); i++)
	{
		final_pt3d_mm.push_back(rl_lower_pt3d_mm[i]);
		final_pt3d_mm.push_back(rl_upper_pt3d_mm[i]);
	}

	for (int i = (int)point_num - 1; i >= 0; i--)
	{
		final_pt3d_mm.push_back(left_outer_pt3d_mm[i]);
		final_pt3d_mm.push_back(left_inner_pt3d_mm[i]);
	}

	vector<Point2f> final_pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, final_pt3d_mm);

	// create vtx
	total_vertex_num = 4 * point_num + 2 * PGS_LINE_POINTS_NUM;
	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 3), sizeof(GLfloat));

	if (vtx != nullptr)
	{
		int vtx_base_idx = 0;
		for (int i = 0; i < total_vertex_num; i++)
		{
			vtx[vtx_base_idx + 0] = xflip * ((final_pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			vtx[vtx_base_idx + 1] = -((final_pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
			vtx[vtx_base_idx + 2] = 0.0f;

			vtx_base_idx += 3;
		}

		if (!left_outer_pt3d_mm.empty()) left_outer_pt3d_mm.clear(); else noop;
		if (!left_inner_pt3d_mm.empty()) left_inner_pt3d_mm.clear(); else noop;
		if (!right_inner_pt3d_mm.empty()) right_inner_pt3d_mm.clear(); else noop;
		if (!right_outer_pt3d_mm.empty()) right_outer_pt3d_mm.clear(); else noop;
		
		if (!rl_pt3d_mm.empty()) rl_pt3d_mm.clear(); else noop;
		if (!rl_lower_pt3d_mm.empty()) rl_lower_pt3d_mm.clear(); else noop;
		if (!rl_upper_pt3d_mm.empty()) rl_upper_pt3d_mm.clear(); else noop;

		if (!final_pt3d_mm.empty()) final_pt3d_mm.clear(); else noop;
		if (!final_pt2d.empty()) final_pt2d.clear(); else noop;
	}
	else
		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

	return vtx;
}


// GLfloat* sanPGS::generatePGSMeshQuad_2D(int camID, vector<Point3f> pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/)
// {
// 	float xflip = ((float)(camID == CAMID_REAR) - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1

// #if(ENABLE_QUADS)
// 	total_vertex_num = ((int)pt3d_mm.size()) * 2;
// #else
// 	total_vertex_num = ((int)pt3d_mm.size() - 1) * 6;
// #endif

// 	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 5), sizeof(GLfloat));
// 	if (vtx != nullptr)
// 	{
// 		int vtx_base_idx = 0;

// 		vector<Point3f> right_pt3d_mm, left_pt3d_mm;
// 		vector<Point2f> right_pt2d, left_pt2d;
// 		vector<Point2f> viewport_right_pt2d, viewport_left_pt2d;

// 		generateLeftRightPoints_3D(pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, right_pt3d_mm, left_pt3d_mm);

// 		right_pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, right_pt3d_mm);
// 		left_pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, left_pt3d_mm);

// 		viewport_right_pt2d = sanWorld::normalize_points_in_defisheye(camID, m_pxml, right_pt2d);
// 		viewport_left_pt2d = sanWorld::normalize_points_in_defisheye(camID, m_pxml, left_pt2d);


// #if(ENABLE_QUADS)
// 		for (int i = 0; i < (int)pt3d_mm.size(); i++)
// 		{
// 			vtx[vtx_base_idx + 0] = xflip * viewport_left_pt2d[i].x;			vtx[vtx_base_idx + 1] = -viewport_left_pt2d[i].y;			vtx[vtx_base_idx + 2] = 0.0f;			vtx[vtx_base_idx + 3] = 0.0f;			vtx[vtx_base_idx + 4] = i * 1.0f / (pt3d_mm.size() - 1);
// 			vtx[vtx_base_idx + 5] = xflip * viewport_right_pt2d[i].x;			vtx[vtx_base_idx + 6] = -viewport_right_pt2d[i].y;			vtx[vtx_base_idx + 7] = 0.0f;			vtx[vtx_base_idx + 8] = 1.0f;			vtx[vtx_base_idx + 9] = i * 1.0f / (pt3d_mm.size() - 1);

// 			vtx_base_idx += 10;
// 		}
// #else
// 		for (int i = 0; i < pt3d_mm.size() - 1; i++)
// 		{
// 			vtx[vtx_base_idx + 0] = xflip * viewport_left_pt2d[i + 1].x;		vtx[vtx_base_idx + 1] = -viewport_left_pt2d[i + 1].y;		vtx[vtx_base_idx + 2] = 0.0f;			vtx[vtx_base_idx + 3] = 0.0f;			vtx[vtx_base_idx + 4] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
// 			vtx[vtx_base_idx + 5] = xflip * viewport_left_pt2d[i].x;			vtx[vtx_base_idx + 6] = -viewport_left_pt2d[i].y;			vtx[vtx_base_idx + 7] = 0.0f;			vtx[vtx_base_idx + 8] = 0.0f;			vtx[vtx_base_idx + 9] = i * 1.0f / (pt3d_mm.size() - 1);
// 			vtx[vtx_base_idx + 10] = xflip * viewport_right_pt2d[i + 1].x;		vtx[vtx_base_idx + 11] = -viewport_right_pt2d[i + 1].y;		vtx[vtx_base_idx + 12] = 0.0f;			vtx[vtx_base_idx + 13] = 1.0f;			vtx[vtx_base_idx + 14] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
// 			vtx[vtx_base_idx + 15] = xflip * viewport_right_pt2d[i + 1].x;		vtx[vtx_base_idx + 16] = -viewport_right_pt2d[i + 1].y;		vtx[vtx_base_idx + 17] = 0.0f;			vtx[vtx_base_idx + 18] = 1.0f;			vtx[vtx_base_idx + 19] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
// 			vtx[vtx_base_idx + 20] = xflip * viewport_left_pt2d[i].x;			vtx[vtx_base_idx + 21] = -viewport_left_pt2d[i].y;			vtx[vtx_base_idx + 22] = 0.0f;			vtx[vtx_base_idx + 23] = 0.0f;			vtx[vtx_base_idx + 24] = i * 1.0f / (pt3d_mm.size() - 1);
// 			vtx[vtx_base_idx + 25] = xflip * viewport_right_pt2d[i].x;			vtx[vtx_base_idx + 26] = -viewport_right_pt2d[i].y;			vtx[vtx_base_idx + 27] = 0.0f;			vtx[vtx_base_idx + 28] = 1.0f;			vtx[vtx_base_idx + 29] = i * 1.0f / (pt3d_mm.size() - 1);
			
// 			vtx_base_idx += 30;
// 		}
// #endif

// 		if (!right_pt3d_mm.empty()) right_pt3d_mm.clear(); else noop;
// 		if (!left_pt3d_mm.empty()) left_pt3d_mm.clear(); else noop;
// 		if (!right_pt2d.empty()) right_pt2d.clear(); else noop;
// 		if (!left_pt2d.empty()) left_pt2d.clear(); else noop;
// 		if (!viewport_right_pt2d.empty()) viewport_right_pt2d.clear(); else noop;
// 		if (!viewport_left_pt2d.empty()) viewport_left_pt2d.clear(); else noop;
// 	}
// 	else
// 		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

// 	return vtx;
// }


GLfloat* sanPGS::generatePGSMeshQuad_2D(int camID, vector<Point3f> pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/)
{
	float vertex_xnorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].cols - (m_pxml->m_rcam[camID].camview_offset.hleft + m_pxml->m_rcam[camID].camview_offset.hright));
	float vertex_ynorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].rows - (m_pxml->m_rcam[camID].camview_offset.vtop + m_pxml->m_rcam[camID].camview_offset.vbot));

	float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1


#if(ENABLE_QUADS)
	total_vertex_num = ((int)pt3d_mm.size()) * 2;
#else
	total_vertex_num = ((int)pt3d_mm.size() - 1) * 6;
#endif

	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 5), sizeof(GLfloat));
	if (vtx != nullptr)
	{
		int vtx_base_idx = 0;

		vector<Point3f> right_pt3d_mm, left_pt3d_mm;
		vector<Point2f> right_pt2d, left_pt2d;

		generateLeftRightPoints_3D(pt3d_mm, m_pxml->m_pgs_parameter.unit_sample_width_mm, right_pt3d_mm, left_pt3d_mm);

		right_pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, right_pt3d_mm);
		left_pt2d = sanWorld::GL_mm_to_defisheye(camID, m_pxml, left_pt3d_mm);

#if(ENABLE_QUADS)
		for (int i = 0; i < (int)pt3d_mm.size(); i++)
		{
			float viewport_left_x = xflip * ((left_pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			float viewport_left_y = -((left_pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
			float viewport_right_x = xflip * ((right_pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			float viewport_right_y = -((right_pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;


			vtx[vtx_base_idx + 0] = viewport_left_x;			vtx[vtx_base_idx + 1] = viewport_left_y;			vtx[vtx_base_idx + 2] = 0.0f;			vtx[vtx_base_idx + 3] = 0.0f;			vtx[vtx_base_idx + 4] = i * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 5] = viewport_right_x;			vtx[vtx_base_idx + 6] = viewport_right_y;			vtx[vtx_base_idx + 7] = 0.0f;			vtx[vtx_base_idx + 8] = 1.0f;			vtx[vtx_base_idx + 9] = i * 1.0f / (pt3d_mm.size() - 1);

			vtx_base_idx += 10;
		}
#else
		for (int i = 0; i < pt3d_mm.size() - 1; i++)
		{
			float viewport_left_x = xflip * ((left_pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			float viewport_left_y = -((left_pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
			float viewport_right_x = xflip * ((right_pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			float viewport_right_y = -((right_pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;

			float viewport_left_x_1 = xflip * ((left_pt2d[i+1].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			float viewport_left_y_1 = -((left_pt2d[i+1].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
			float viewport_right_x_1 = xflip * ((right_pt2d[i+1].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			float viewport_right_y_1 = -((right_pt2d[i+1].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;

			
			vtx[vtx_base_idx + 0] = viewport_left_x_1;			vtx[vtx_base_idx + 1] = viewport_left_y_1;			vtx[vtx_base_idx + 2] = 0.0f;			vtx[vtx_base_idx + 3] = 0.0f;			vtx[vtx_base_idx + 4] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 5] = viewport_left_x;			vtx[vtx_base_idx + 6] = viewport_left_y;			vtx[vtx_base_idx + 7] = 0.0f;			vtx[vtx_base_idx + 8] = 0.0f;			vtx[vtx_base_idx + 9] = i * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 10] = viewport_right_x_1;		vtx[vtx_base_idx + 11] = viewport_right_y_1;		vtx[vtx_base_idx + 12] = 0.0f;			vtx[vtx_base_idx + 13] = 1.0f;			vtx[vtx_base_idx + 14] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 15] = viewport_right_x_1;		vtx[vtx_base_idx + 16] = viewport_right_y_1;		vtx[vtx_base_idx + 17] = 0.0f;			vtx[vtx_base_idx + 18] = 1.0f;			vtx[vtx_base_idx + 19] = (i + 1) * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 20] = viewport_left_x;			vtx[vtx_base_idx + 21] = viewport_left_y;			vtx[vtx_base_idx + 22] = 0.0f;			vtx[vtx_base_idx + 23] = 0.0f;			vtx[vtx_base_idx + 24] = i * 1.0f / (pt3d_mm.size() - 1);
			vtx[vtx_base_idx + 25] = viewport_right_x;			vtx[vtx_base_idx + 26] = viewport_right_y;			vtx[vtx_base_idx + 27] = 0.0f;			vtx[vtx_base_idx + 28] = 1.0f;			vtx[vtx_base_idx + 29] = i * 1.0f / (pt3d_mm.size() - 1);
			
			vtx_base_idx += 30;
		}
#endif

		if (!right_pt3d_mm.empty()) right_pt3d_mm.clear(); else noop;
		if (!left_pt3d_mm.empty()) left_pt3d_mm.clear(); else noop;
		if (!right_pt2d.empty()) right_pt2d.clear(); else noop;
		if (!left_pt2d.empty()) left_pt2d.clear(); else noop;
	}
	else
		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

	return vtx;
}


void sanPGS::generate_and_update_PGSMeshQuad_3D()
{
	try
	{
		// front left
		int total_vertex_num = 0;
		GLfloat* pgs3D_FL = this->generatePGSMeshQuad_3D(m_front_left_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(pgs3D_FL, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		else updateVAB(0, pgs3D_FL, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (pgs3D_FL != nullptr) free(pgs3D_FL); else noop;

		// rear left
		total_vertex_num = 0;
		GLfloat* pgs3D_RL = this->generatePGSMeshQuad_3D(m_rear_left_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(pgs3D_RL, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		else updateVAB(1, pgs3D_RL, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (pgs3D_RL != nullptr) free(pgs3D_RL);  else noop;

		// front right
		total_vertex_num = 0;
		GLfloat* pgs3D_FR = this->generatePGSMeshQuad_3D(m_front_right_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(pgs3D_FR, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		else updateVAB(2, pgs3D_FR, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (pgs3D_FR != nullptr) free(pgs3D_FR);  else noop;

		// rear right
		total_vertex_num = 0;
		GLfloat* pgs3D_RR = this->generatePGSMeshQuad_3D(m_rear_right_pt3d_mm, total_vertex_num);
		if (!is_3D_VAB_generated) generateVAB(pgs3D_RR, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		else updateVAB(3, pgs3D_RR, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (pgs3D_RR != nullptr) free(pgs3D_RR);  else noop;

		is_3D_VAB_generated = true;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanPGS::generate_and_update_PGSMeshQuad_2D(int camID)
{
	try
	{

#if(0)
		// rear left
		int total_vertex_num = 0;
		GLfloat* pgs2D_RL = this->generatePGSMeshQuad_2D(camID, m_rear_left_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(pgs2D_RL, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		else updateVAB(4, pgs2D_RL, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (pgs2D_RL != nullptr) free(pgs2D_RL); else noop;

		// rear right
		total_vertex_num = 0;
		GLfloat* pgs2D_RR = this->generatePGSMeshQuad_2D(camID, m_rear_right_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(pgs2D_RR, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		else updateVAB(5, pgs2D_RR, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (pgs2D_RR != nullptr) free(pgs2D_RR); else noop;
#else
		// rear 1st segment
		int total_vertex_num = 0;
		GLfloat* pgs2D_1m = this->generatePGS_2D(camID, m_pxml->m_pgs_parameter.rear_1st_distance, m_rear_left_pt3d_mm, m_rear_right_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(pgs2D_1m, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(4, pgs2D_1m, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (pgs2D_1m != nullptr) free(pgs2D_1m); else noop;

		// rear 2nd segment
		total_vertex_num = 0;
		GLfloat* pgs2D_2m = this->generatePGS_2D(camID, m_pxml->m_pgs_parameter.rear_2nd_distance, m_rear_left_pt3d_mm, m_rear_right_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(pgs2D_2m, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(5, pgs2D_2m, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (pgs2D_2m != nullptr) free(pgs2D_2m); else noop;

		// rear 3rd segment
		total_vertex_num = 0;
		GLfloat* pgs2D_3m = this->generatePGS_2D(camID, m_pxml->m_pgs_parameter.rear_3rd_distance, m_rear_left_pt3d_mm, m_rear_right_pt3d_mm, total_vertex_num);
		if (!is_2D_VAB_generated) generateVAB(pgs2D_3m, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(6, pgs2D_3m, (GLuint)total_vertex_num, 3, 0, GL_DYNAMIC_DRAW);
		if (pgs2D_3m != nullptr) free(pgs2D_3m); else noop;
#endif

		is_2D_VAB_generated = true;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanPGS::renderPGS_2D(int vabt_idx, glm::vec3 color, float alpha)
{
	sanError::glClearError();

	m_pgsShader.use();
	m_pgsShader.setVec3("scale", glm::vec3(1.0f, 1.0f, 1.0f));
	m_pgsShader.setVec3("translate", glm::vec3(0.0f, 0.0f, 0.0f));
	m_pgsShader.setMat4("mvp", glm::mat4(1.0f));
	m_pgsShader.setVec3("color", color);
	m_pgsShader.setFloat("alpha", alpha);
	m_pgsShader.setInt("colorType", 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vabt_list[vabt_idx].vaoID);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, m_vabt_list[vabt_idx].vnum);

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__);
}

void sanPGS::renderPGS(int vabt_idx, glm::vec3 color, float alpha, glm::mat4 mvp)
{
	sanError::glClearError();

	m_pgsShader.use();
	m_pgsShader.setVec3("scale", glm::vec3(1.0f, 1.0f, 1.0f));
	m_pgsShader.setVec3("translate", glm::vec3(0.0f, 0.0f, 0.0f));
	m_pgsShader.setMat4("mvp", mvp);
	m_pgsShader.setVec3("color", color);
	m_pgsShader.setFloat("alpha", alpha);
	m_pgsShader.setInt("colorType", 0);


	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vabt_list[vabt_idx].vaoID);

	#if (ENABLE_TEXTURE)
		m_pgsShader.setInt("colorType", 1);
		glActiveTexture(GL_TEXTURE0);

		switch (vabt_idx)
		{
		case 0: //front_left
		case 1: //rear_left
		case 4: //rear_left reverse
			glBindTexture(GL_TEXTURE_2D, (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE) ? m_texReverseLeft : m_texDriveLeft);
			break;
		case 2: //front_right
		case 3: //rear_right
		case 5: //rear_right reverse
			glBindTexture(GL_TEXTURE_2D, (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE) ? m_texReverseRight : m_texDriveRight);
			break;
		default:
			break;
		}
	#endif

	#if(ENABLE_QUADS)
		glDrawArrays(GL_TRIANGLE_STRIP, 0, m_vabt_list[vabt_idx].vnum);
	#else
		glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_idx].vnum);
	#endif

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);


	sanError::glCheckError(__FUNCTION__);
}

void sanPGS::drawLayout0(glm::mat4& vcvm)
{
	try
	{
		if (m_pxml->m_activation.pgs)
		{
			glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

			generate_wheel_trajectory(90.0f);
			generate_and_update_PGSMeshQuad_3D();

			if (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE)
			{
				//renderPGS(0, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->opm * vcvm);
				renderPGS(1, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->opm * vcvm);
				//renderPGS(2, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->opm * vcvm);
				renderPGS(3, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->opm * vcvm);
			}
			else
			{
				renderPGS(0, m_pgs_curve_color, 0.20f, m_ppm->opm * vcvm);
				//renderPGS(1, m_pgs_curve_color, 0.20f, m_ppm->opm * vcvm);
				renderPGS(2, m_pgs_curve_color, 0.20f, m_ppm->opm * vcvm);
				//renderPGS(3, m_pgs_curve_color, 0.20f, m_ppm->opm * vcvm);
			}
		}
		else noop;
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.pgs)
		{
			m_pxml->m_activation.pgs = false;
			runtime_error  err_msg = logger.svm_fatal("C2205101", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}


}

void sanPGS::drawLayout1(glm::mat4& vcvm)
{
	try
	{
		if (m_pxml->m_activation.pgs)
		{
			generate_wheel_trajectory(90.0f);

			switch (m_pxml->m_layout[1].view_mode)
			{
			case CAMVIEW3D_FRONT:
			case CAMVIEW3D_RIGHT:
			case CAMVIEW3D_REAR:
			case CAMVIEW3D_LEFT:

				glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

				generate_and_update_PGSMeshQuad_3D();

				if (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE)
				{
					//renderPGS(0, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->ppm * vcvm);
					renderPGS(1, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->ppm * vcvm);
					//renderPGS(2, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->ppm * vcvm);
					renderPGS(3, m_pgs_curve_color, m_pgs_curve_alpha, m_ppm->ppm * vcvm);
				}
				else
				{
					renderPGS(0, m_pgs_curve_color, 0.20f, m_ppm->ppm * vcvm);
					//renderPGS(1, m_pgs_curve_color, 0.20f, m_ppm->ppm * vcvm);
					renderPGS(2, m_pgs_curve_color, 0.20f, m_ppm->ppm * vcvm);
					//renderPGS(3, m_pgs_curve_color, 0.20f, m_ppm->ppm * vcvm);
				}
				break;
			case CAMVIEW2D_REAR:
			
				glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

				generate_and_update_PGSMeshQuad_2D(m_pxml->m_layout[1].view_mode - CAMVIEW2D_FRONT);

				#if(0)
					if (m_pvehicleSignal->m_trigger.gear == GEAR_REVERSE)
					{
						renderPGS(4, m_pgs_curve_color, m_pgs_curve_alpha, glm::mat4(1.0f));
						renderPGS(5, m_pgs_curve_color, m_pgs_curve_alpha, glm::mat4(1.0f));
					}
					else
					{
						renderPGS(4, m_pgs_curve_color, 0.20f, glm::mat4(1.0f));
						renderPGS(5, m_pgs_curve_color, 0.20f, glm::mat4(1.0f));
					}
				#else
					renderPGS_2D(6, glm::vec3(0.0f, 1.0f, 0.0f), m_pgs_curve_alpha);
					renderPGS_2D(5, glm::vec3(1.0f, 1.0f, 0.0f), m_pgs_curve_alpha);
					renderPGS_2D(4, glm::vec3(1.0f, 0.0f, 0.0f), m_pgs_curve_alpha);
				#endif

				break;
			case CAMVIEW2D_FRONT:
			case CAMVIEW2D_RIGHT:
			case CAMVIEW2D_LEFT:
			case CAMVIEW2D_ADD0:
			case TOPVIEW3D:
				noop;
				break;
			default:
				throw runtime_error("$view_mode wrong");
				break;
			}
		}
		else noop;
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.pgs)
		{
			m_pxml->m_activation.pgs = false;
			runtime_error  err_msg = logger.svm_fatal("C2205201", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}

