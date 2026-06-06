#include <time.h>
#include "svmDGS.hpp"
#include "svmError.hpp"

sanDGS::sanDGS(sanXML* pxml, PM* ppm, TRIGGER *ptrigger)
{
	m_pxml = pxml;
	m_ppm = ppm;
	m_ptrigger = ptrigger;
}

sanDGS::~sanDGS()
{
	m_dgsShader.~sanShader();
}

void sanDGS::initialize()
{
	try
	{
		// for layout 0
		vector<Point3f> layout0_square_pt3d_mm = get_distance_guide_square(m_pxml->m_dgs_parameter.layout0_line_thickness);
		vector<Point3f> layout0_square_pt3d = sanWorld::get_Logic_InGLOBAL(m_pxml, layout0_square_pt3d_mm);
		GLfloat* layout0_square_lut = generate_LUT(layout0_square_pt3d);
		this->generateVAB(layout0_square_lut, (GLuint)layout0_square_pt3d.size(), 3, 0, GL_STATIC_DRAW);
		if (layout0_square_lut != nullptr) free(layout0_square_lut); else noop;

		// for layout 1
		vector<Point3f> layout1_square_pt3d_mm = get_distance_guide_square(m_pxml->m_dgs_parameter.layout1_line_thickness);
		vector<Point3f> layout1_square_pt3d = sanWorld::get_Logic_InGLOBAL(m_pxml, layout1_square_pt3d_mm);
		GLfloat* layout1_square_lut = generate_LUT(layout1_square_pt3d);
		this->generateVAB(layout1_square_lut, (GLuint)layout1_square_pt3d.size(), 3, 0, GL_STATIC_DRAW);
		if (layout1_square_lut != nullptr) free(layout1_square_lut); else noop;
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2106001", __FUNCTION__ + delimiter(string(e.what())));
	}
}



void sanDGS::renderDGS(int vabt_idx, glm::vec3& color, float alpha, glm::mat4& mvp)
{
	sanError::glClearError();

	m_dgsShader.use();
	m_dgsShader.setMat4("mvp", mvp);
	m_dgsShader.setVec3("color", color);
	m_dgsShader.setFloat("alpha", alpha);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBindVertexArray(m_vabt_list[vabt_idx].vaoID);
	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_idx].vnum);
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__);
}



void sanDGS::drawLayout0(glm::mat4& view_matrix)
{
	try
	{
		if (m_pxml->m_activation.dgs)
		{
			glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

			glm::mat4 mvp = m_ppm->opm * view_matrix;
			renderDGS(0, m_layout0_color, m_layout0_alpha, mvp);
		}
		else noop;
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.dgs)
		{
			m_pxml->m_activation.dgs = false;
			runtime_error  err_msg = logger.svm_fatal("C2206101", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
	
}

void sanDGS::drawLayout1(glm::mat4& view_matrix)
{
	try
	{
		switch (m_pxml->m_layout[1].view_mode)
		{
		case CAMVIEW3D_FRONT:
		case CAMVIEW3D_RIGHT:
		case CAMVIEW3D_REAR:
		case CAMVIEW3D_LEFT:
			if (m_pxml->m_activation.dgs)
			{
				glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);
				
				glm::mat4 mvp = m_ppm->ppm * view_matrix;
				renderDGS(1, m_layout1_color, m_layout1_alpha, mvp);
			}
			else noop;
			break;
		case TOPVIEW3D:
		case CAMVIEW2D_FRONT:
		case CAMVIEW2D_RIGHT:
		case CAMVIEW2D_REAR:
		case CAMVIEW2D_LEFT:
		case CAMVIEW2D_ADD0:
			noop;
			break;
		default:
			throw runtime_error("view_mode wrong");
			break;
		}
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.dgs)
		{
			m_pxml->m_activation.dgs = false;
			runtime_error  err_msg = logger.svm_fatal("C2206201", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}


vector<Point3f> sanDGS::get_horizontal_distance_guide_square(float line_thickness)
{
	float full_distance = m_pxml->m_dgs_parameter.monitoring_distance;//mm
	float unit_square_length = m_pxml->m_dgs_parameter.unit_square_length;//mm
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);

	Point3f v0 = vehicle_pt3d_mm[0];
	Point3f v1 = vehicle_pt3d_mm[1];
	Point3f v3 = vehicle_pt3d_mm[3];
	Point3f w0 = v0 + Point3f(-full_distance, full_distance, 0.0f);
	Point3f w3 = v3 + Point3f(full_distance, -full_distance, 0.0f);

	float vehicle_length = v0.y - v1.y;

	int y_side_iteration_num = (int)(full_distance / unit_square_length);	
	int y_center_iteration_num = (int)(vehicle_length / unit_square_length);
	float y_half_center_remain = (vehicle_length - y_center_iteration_num * unit_square_length) / 2.0f;

	int y_total_iteration_num = 2 * y_side_iteration_num + y_center_iteration_num + 2;
	int y_first_index = 0, y_center_index = 0, y_last_index = 0;


	vector<Point3f> lut_pt3d_mm;
	float y = w0.y;
	for (int k = 0; k <= y_total_iteration_num; k++)
	{
		if (y_first_index <= y_side_iteration_num)
		{
			y = w0.y - y_first_index * unit_square_length;
			y_first_index++;
		}
		else if (y_center_index <= y_center_iteration_num)
		{
			y = v0.y - y_half_center_remain -
				(y_center_index <= y_center_iteration_num) * min(y_center_index, y_center_iteration_num) * unit_square_length;
			y_center_index++;
		}
		else
		{
			y = v1.y - y_last_index * unit_square_length;
			y_last_index++;
		}

		vector<Point3f> pt3d_mm;
		pt3d_mm.push_back(Point3f(w0.x, y, 0.0f)); // p0
		pt3d_mm.push_back(Point3f(w0.x, y + line_thickness, 0.0f)); // p1
		pt3d_mm.push_back(Point3f(w3.x, y, 0.0f)); // p2
		pt3d_mm.push_back(Point3f(w3.x, y + line_thickness, 0.0f)); //p3
		
		lut_pt3d_mm.push_back(pt3d_mm[0]);
		lut_pt3d_mm.push_back(pt3d_mm[1]);
		lut_pt3d_mm.push_back(pt3d_mm[2]);
		lut_pt3d_mm.push_back(pt3d_mm[2]);
		lut_pt3d_mm.push_back(pt3d_mm[1]);
		lut_pt3d_mm.push_back(pt3d_mm[3]);
	}

	return lut_pt3d_mm;
}

vector<Point3f> sanDGS::get_vertical_distance_guide_square(float line_thickness)
{
	float full_distance = m_pxml->m_dgs_parameter.monitoring_distance;//mm
	float unit_square_length = m_pxml->m_dgs_parameter.unit_square_length;//mm
	vector<Point3f> vehicle_pt3d_mm = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_MM);

	Point3f v0 = vehicle_pt3d_mm[0];
	Point3f v2 = vehicle_pt3d_mm[2];
	Point3f v3 = vehicle_pt3d_mm[3];
	Point3f w0 = v0 + Point3f(-full_distance, full_distance, 0.0f);
	Point3f w3 = v3 + Point3f(full_distance, -full_distance, 0.0f);

	float vehicle_width = v2.x - v0.x;
	int x_side_iteration_num = (int)(full_distance / unit_square_length);
	int x_center_iteration_num = (int)(vehicle_width / unit_square_length);
	float x_half_center_remain = (vehicle_width - x_center_iteration_num * unit_square_length) / 2.0f;
	int x_total_iteration_num = 2 * x_side_iteration_num + x_center_iteration_num + 2;
	int x_first_index = 0, x_center_index = 0, x_last_index = 0;


	vector<Point3f> lut_pt3d_mm;
	float x = w0.x;
	for (int i = 0; i <= x_total_iteration_num; i++)
	{
		if (x_first_index <= x_side_iteration_num)
		{
			x = w0.x + x_first_index * unit_square_length;
			x_first_index++;
		}
		else if (x_center_index <= x_center_iteration_num)
		{
			x = v0.x + x_half_center_remain +
				(x_center_index <= x_center_iteration_num) * min(x_center_index, x_center_iteration_num) * unit_square_length;
			x_center_index++;
		}
		else
		{
			x = v2.x + x_last_index * unit_square_length;
			x_last_index++;
		}

		float prev_y = w0.y;
		float curr_y = w3.y;

		vector<Point3f> pt3d;
		pt3d.push_back(Point3f(x, prev_y, 0.0f)); // p0
		pt3d.push_back(Point3f(x, curr_y, 0.0f)); // p1
		pt3d.push_back(Point3f(x + line_thickness, prev_y, 0.0f)); // p2
		pt3d.push_back(Point3f(x + line_thickness, curr_y, 0.0f)); //p3

		lut_pt3d_mm.push_back(pt3d[0]);
		lut_pt3d_mm.push_back(pt3d[1]);
		lut_pt3d_mm.push_back(pt3d[2]);
		lut_pt3d_mm.push_back(pt3d[2]);
		lut_pt3d_mm.push_back(pt3d[1]);
		lut_pt3d_mm.push_back(pt3d[3]);	
	}

	return lut_pt3d_mm;
}


vector<Point3f> sanDGS::get_distance_guide_square(float line_thickness)
{
	vector<Point3f> distance_guide_square;

	float full_distance = m_pxml->m_dgs_parameter.monitoring_distance;//mm
	float unit_square_length = m_pxml->m_dgs_parameter.unit_square_length;//mm

	if (0.0f < unit_square_length)
	{
		int times = (int)(full_distance / unit_square_length);
		float remain = full_distance - times * unit_square_length;

		if (0.0f < remain)
			m_pxml->m_dgs_parameter.monitoring_distance = (times + 1) * unit_square_length;
		else noop;
	}
	else noop;

	vector<Point3f> horizontal = get_horizontal_distance_guide_square(line_thickness);
	for (int i = 0; i < (int)horizontal.size(); i++)
		distance_guide_square.push_back(horizontal[i]);


	vector<Point3f> vertical = get_vertical_distance_guide_square(line_thickness);
	for (int i = 0; i < (int)vertical.size(); i++)
		distance_guide_square.push_back(vertical[i]);

	return distance_guide_square;
}


GLfloat* sanDGS::generate_LUT(vector<Point3f>& pt3d)
{
	GLfloat* vtx = (GLfloat*)calloc(pt3d.size() * 3, sizeof(GLfloat));

	if (vtx != nullptr)
	{
		int i = 0;
		for (vector<Point3f>::iterator iter = pt3d.begin(); iter != pt3d.end(); ++iter)
		{
			vtx[i * 3 + 0] = iter->x;
			vtx[i * 3 + 1] = iter->y;
			vtx[i * 3 + 2] = iter->z;
			i++;
		}
	}
	else
		throw runtime_error(__FUNCTION__ + delimiter(string(" $failed to allocate memory")));

	return vtx;
}
