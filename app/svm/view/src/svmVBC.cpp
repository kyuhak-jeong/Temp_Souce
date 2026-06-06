#include "svmShaderString.hpp"
#include "svmWorld.hpp"
#include "svmCamera.hpp"
#include "svmVBC.hpp"
#include "svmError.hpp"
#include "svmCamvec.hpp"

sanVBC::sanVBC(sanXML* pxml, PM *ppm)
{
	m_pxml = pxml;
	m_ppm  = ppm;

	for (int i = 0; i < SVM_CAMERAS_NUM; i++)
	{
		m_vehicle_bottom_color_list.push_back(m_bottom_default_color);
		m_vehicle_bottom_color_alpha_list.push_back(m_bottom_default_color_alpha);
	}
}

sanVBC::~sanVBC()
{
	if (!m_expanded_vehicle_box_on_viewport.empty())
	{
		m_expanded_vehicle_box_on_viewport.clear();
		vector<Point2f>().swap(m_expanded_vehicle_box_on_viewport);
	}
	else noop;

	if (!m_vehicle_bottom_color_list.empty())
	{
		m_vehicle_bottom_color_list.clear();
		vector<glm::vec3>().swap(m_vehicle_bottom_color_list);
	}
	else noop;

	if (!m_vehicle_bottom_color_alpha_list.empty())
	{
		m_vehicle_bottom_color_alpha_list.clear();
		vector<float>().swap(m_vehicle_bottom_color_alpha_list);
	}
	else noop;

	m_vbcShader.~sanShader();
}




void sanVBC::initialize()
{
	try
	{
		int bottomMeshNum = 0;
		GLfloat* pbottomMesh = generate_expanded_vehicle_box_mesh(bottomMeshNum);
		generateVAB(pbottomMesh, bottomMeshNum, 3, 0, GL_STATIC_DRAW);
		if (pbottomMesh != nullptr) free(pbottomMesh);
		else noop;

		m_expanded_vehicle_box_on_viewport = get_expanded_vehicle_box_on_viewport();
	
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2104001", __FUNCTION__ + delimiter(string(e.what())));
	}
}

vector<Point2f> sanVBC::get_expanded_vehicle_box_on_viewport()
{
	vector<Point3f> vehicle_box = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_LOGIC); // normalized coordinates on XY plane in GL space
	sanWorld::apply_expanded_distance_to_vehicle_box(m_pxml, vehicle_box, VEHICLE_BOX_UNIT_LOGIC);

	glm::mat4 view_matrix = VM4x4(m_pxml->m_vcam[10].pos, m_pxml->m_vcam[10].ori);
	vector<Point2f> NDC = sanWorld::GL_to_NDC(m_ppm->opm * view_matrix, vehicle_box);

	vector<Point2f> vehicle_box_on_viewport;
	for (int i = 0; i < (int)NDC.size(); i++) //  NDC(-1 ~ 1) -> Window Viewport(0 ~ size)
	{
		// NDC --> Window coordinate system(1920x1080)
		float Window_x = ( 1.0f * (NDC[i].x) * m_pxml->m_layout[0].width + m_pxml->m_layout[0].width) / 2.0f + m_pxml->m_layout[0].x;
		float Window_y = (-1.0f * (NDC[i].y) * m_pxml->m_layout[0].height + m_pxml->m_layout[0].height) / 2.0f + m_pxml->m_layout[0].y;
		vehicle_box_on_viewport.push_back(Point2f(Window_x, Window_y));
	}

	return vehicle_box_on_viewport;
}


GLfloat* sanVBC::generate_expanded_vehicle_box_mesh(int& total_vertex_num)
{
	vector<Point3f> vehicle_box = sanWorld::get_vehicle_box_InGLOBAL(m_pxml, VEHICLE_BOX_UNIT_LOGIC);
	sanWorld::apply_expanded_distance_to_vehicle_box(m_pxml, vehicle_box, VEHICLE_BOX_UNIT_LOGIC);

	total_vertex_num = 6;
	GLfloat* vtx = (GLfloat*)calloc((size_t)(total_vertex_num * 3), sizeof(GLfloat));

	if (vtx != nullptr)
	{
		// need to be slightly smaller than the actual rectangle
		vtx[0] = vehicle_box[0].x; vtx[1] = vehicle_box[0].y; vtx[2] = vehicle_box[0].z; //v0
		vtx[3] = vehicle_box[1].x; vtx[4] = vehicle_box[1].y; vtx[5] = vehicle_box[1].z; //v1
		vtx[6] = vehicle_box[2].x; vtx[7] = vehicle_box[2].y; vtx[8] = vehicle_box[2].z; //v2
		vtx[9] = vehicle_box[2].x; vtx[10] = vehicle_box[2].y; vtx[11] = vehicle_box[2].z; //v2
		vtx[12] = vehicle_box[1].x; vtx[13] = vehicle_box[1].y; vtx[14] = vehicle_box[1].z; //v1
		vtx[15] = vehicle_box[3].x; vtx[16] = vehicle_box[3].y; vtx[17] = vehicle_box[3].z; //v3
	}
	else
		throw runtime_error(__FUNCTION__ + string("$failed to allocate memory"));

	return vtx;
}


void sanVBC::renderVBC(glm::vec3 color, float alpha, glm::mat4 vcvm)
{
	sanError::glClearError();

	m_vbcShader.use();
	m_vbcShader.setMat4("mvp", vcvm);
	m_vbcShader.setVec3("color", color);
	m_vbcShader.setFloat("alpha", alpha);

	#if (1)
		glDisable(GL_BLEND);
	#else
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
	#endif

	glBindVertexArray(m_vabt_list[0].vaoID);
	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[0].vnum);
	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__);
}


// esseo dma gpu copy cpu
#if 0
void downloadTexture(GLuint textureID, int width, int height)
{
    glBindTexture(GL_TEXTURE_2D, textureID);

    // PBO
    GLuint pbo;
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, width * height * 4, nullptr, GL_STREAM_READ);

    // PBO
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    // PBO CPU
    void* data = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
    if (data)
	{
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    }

    // PBO
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // PBO
    glDeleteBuffers(1, &pbo);
}
#endif

void sanVBC::drawLayout0(glm::mat4& vcvm)
{
	static clock_t vbc_previous_clock = clock();

	try
	{
		glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

		if (m_pxml->m_activation.vbc)
		{
			static bool first_time_run = true;
			clock_t vbc_current_clock = clock();
			float delta_clock_sec = (float)((vbc_current_clock - vbc_previous_clock) / (float)CLOCKS_PER_SEC);

			if (m_bottom_color_update_time_sec < delta_clock_sec || first_time_run)
			{
				first_time_run = false;
				int sx = (int)max(m_pxml->m_layout[0].x + 1, (int)m_expanded_vehicle_box_on_viewport[0].x - 20);
				int sy = (int)max(m_pxml->m_layout[0].y + 1, (int)m_expanded_vehicle_box_on_viewport[0].y - 20);
				int img_width = min(m_pxml->m_layout[0].width - 1, (int)(m_expanded_vehicle_box_on_viewport[2].x - m_expanded_vehicle_box_on_viewport[0].x) + 40);
				int img_height = min(m_pxml->m_layout[0].height - 1, (int)(m_expanded_vehicle_box_on_viewport[1].y - m_expanded_vehicle_box_on_viewport[0].y) + 40);
				unsigned char* pcolor = (unsigned char*)calloc((size_t)img_width * (size_t)img_height * 4, sizeof(unsigned char));

				if (pcolor != nullptr)
				{
					glReadPixels((GLint)sx, (GLint)sy, img_width, img_height, GL_RGBA, GL_UNSIGNED_BYTE, pcolor); // so slow

					// FRONT COLOR
					int px = 0, py = 10;
					float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f, sum_a = 0.0f;
					for (px = 0; px < img_width; px++)
					{
						int pos = (py * img_width + px) * 4;
						sum_r += (pcolor[pos + 0] / 255.f);
						sum_g += (pcolor[pos + 1] / 255.f);
						sum_b += (pcolor[pos + 2] / 255.f);
						sum_a += (pcolor[pos + 3] / 255.f);
					}
					m_vehicle_bottom_color_list[CAMID_FRONT] = glm::vec3(sum_r / img_width, sum_g / img_width, sum_b / img_width);
					m_vehicle_bottom_color_alpha_list[CAMID_FRONT] = sum_a / img_width;

					// RIGTH COLOR
					px = img_width - 10; py = 0;
					sum_r = 0.0f; sum_g = 0.0f; sum_b = 0.0f; sum_a = 0.0f;
					for (py = 0; py < img_height; py++)
					{
						int pos = (py * img_width + px) * 4;
						sum_r += (pcolor[pos + 0] / 255.f);
						sum_g += (pcolor[pos + 1] / 255.f);
						sum_b += (pcolor[pos + 2] / 255.f);
						sum_a += (pcolor[pos + 3] / 255.f);
					}
					m_vehicle_bottom_color_list[CAMID_RIGHT] = glm::vec3(sum_r / img_height, sum_g / img_height, sum_b / img_height);
					m_vehicle_bottom_color_alpha_list[CAMID_RIGHT] = sum_a / img_height;

					// REAR COLOR
					px = 0; py = img_height - 10;
					sum_r = 0.0f; sum_g = 0.0f; sum_b = 0.0f; sum_a = 0.0f;
					for (px = 0; px < img_width; px++)
					{
						int pos = (py * img_width + px) * 4;
						sum_r += (pcolor[pos + 0] / 255.f);
						sum_g += (pcolor[pos + 1] / 255.f);
						sum_b += (pcolor[pos + 2] / 255.f);
						sum_a += (pcolor[pos + 3] / 255.f);
					}
					m_vehicle_bottom_color_list[CAMID_REAR] = glm::vec3(sum_r / img_width, sum_g / img_width, sum_b / img_width);
					m_vehicle_bottom_color_alpha_list[CAMID_REAR] = sum_a / img_width;

					// LEFT COLOR
					px = 10; py = 0;
					sum_r = 0.0f; sum_g = 0.0f; sum_b = 0.0f; sum_a = 0.0f;
					for (py = 0; py < img_height; py++)
					{
						int pos = (py * img_width + px) * 4;
						sum_r += (pcolor[pos + 0] / 255.0f);
						sum_g += (pcolor[pos + 1] / 255.0f);
						sum_b += (pcolor[pos + 2] / 255.0f);
						sum_a += (pcolor[pos + 3] / 255.0f);
					}
					m_vehicle_bottom_color_list[CAMID_LEFT] = glm::vec3(sum_r / img_height, sum_g / img_height, sum_b / img_height);
					m_vehicle_bottom_color_alpha_list[CAMID_LEFT] = sum_a / img_height;

					if (pcolor != nullptr) free(pcolor); else noop;
				}
				else
				{
					for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
					{
						m_vehicle_bottom_color_list[camID] = m_bottom_default_color;
						m_vehicle_bottom_color_alpha_list[camID] = 1.0f;
					}
				}
				vbc_previous_clock = vbc_current_clock;
			}
			else noop;
		}
		else
		{
			for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
			{
				m_vehicle_bottom_color_list[camID] = m_bottom_default_color;
				m_vehicle_bottom_color_alpha_list[camID] = 1.0f;
			}
		}

		renderVBC(m_vehicle_bottom_color_list[CAMID_FRONT], 1.0f, m_ppm->opm * vcvm);
	}
	catch (exception& e)
	{
		if (m_pxml->m_activation.vbc)
		{
			m_pxml->m_activation.vbc = false;
			runtime_error  err_msg = logger.svm_fatal("C2204101", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}



void sanVBC::drawLayout1(glm::mat4& vcvm)
{
	try
	{
		glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

		switch (m_pxml->m_layout[1].view_mode)
		{
		case CAMVIEW3D_FRONT:
			renderVBC(m_vehicle_bottom_color_list[CAMID_FRONT], m_vehicle_bottom_color_alpha_list[CAMID_FRONT], m_ppm->ppm * vcvm);
			break;
		case CAMVIEW3D_RIGHT:
			renderVBC(m_vehicle_bottom_color_list[CAMID_RIGHT], m_vehicle_bottom_color_alpha_list[CAMID_RIGHT], m_ppm->ppm * vcvm);
			break;
		case CAMVIEW3D_REAR:
			renderVBC(m_vehicle_bottom_color_list[CAMID_REAR], m_vehicle_bottom_color_alpha_list[CAMID_REAR], m_ppm->ppm * vcvm);
			break;
		case CAMVIEW3D_LEFT:
			renderVBC(m_vehicle_bottom_color_list[CAMID_LEFT], m_vehicle_bottom_color_alpha_list[CAMID_LEFT], m_ppm->ppm * vcvm);
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
		if (m_pxml->m_activation.vbc)
		{
			m_pxml->m_activation.vbc = false;
			runtime_error  err_msg = logger.svm_fatal("C2204201", __FUNCTION__ + delimiter(string(e.what())) + string(". Then it was automatically disable."));
			logger.record_message(err_msg.what());
		}
		else noop;
	}
}


