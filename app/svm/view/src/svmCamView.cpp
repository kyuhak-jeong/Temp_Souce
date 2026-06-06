#include "svmCamView.hpp"

sanCamView::sanCamView(sanXML* pxml, sanCamera* pcameras, PM* ppm)
{
	m_pxml = pxml;
	m_pcameras = pcameras;
	m_ppm = ppm;
	m_square_size = 4;
}

sanCamView::~sanCamView()
{
	m_camviewShader.~sanShader();
	m_camviewSeamShader.~sanShader();
}

void sanCamView::initialize(GLuint* ppitxID[], GLuint* ppmtxID[], GLuint* ppaitxID[])
{
	try
	{
		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			string str_camID = "(camID[" + to_string(camID) + "])";
			
			try
			{
				GLfloat* plut = nullptr;
				int lut_lines_num = 0;

				if ((int)m_pcameras->m_xmaps.size() > camID && (int)m_pcameras->m_ymaps.size() > camID)
				{
					createDefisheyeLUT(m_pxml->m_rcam[camID], m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], m_square_size, &plut, &lut_lines_num);
					generateVAB(plut, lut_lines_num, 3, 2, GL_STATIC_DRAW);
					if (plut != nullptr) free(plut); else noop;

					GLfloat* plut_seam = createSeamLUT(camID, m_pxml->m_rcam[camID].flipx, m_pxml->m_mask_seam[camID]);
					generateVAB(plut_seam, (GLuint)m_pxml->m_mask_seam[camID].size(), 3, 0, GL_STATIC_DRAW);
					if (plut_seam != nullptr) free(plut_seam); else noop;

					m_vabt_list[2 * camID].pitxID = ppitxID[camID];
					m_vabt_list[2 * camID].pmtxID = ppmtxID[camID];
				}
				else
					throw runtime_error(string("$camID out of range"));
			}
			catch (exception& e)
			{
				throw runtime_error(str_camID + delimiter(string(e.what())));
			}
		}


		for (int camID = 0; camID < ADD_CAMERAS_NUM; camID++)
		{
			string str_camID = "(additional camID[" + to_string(camID) + "])";

			try
			{
				GLfloat* plut = nullptr;
				int lut_lines_num = 0;

				if ((int)m_pcameras->m_axmaps.size() > camID && (int)m_pcameras->m_aymaps.size() > camID)
				{
					createDefisheyeLUT(m_pxml->m_acam[camID], m_pcameras->m_axmaps[camID], m_pcameras->m_aymaps[camID], m_square_size, &plut, &lut_lines_num);
					generateVAB(plut, lut_lines_num, 3, 2, GL_STATIC_DRAW);
					if (plut != nullptr) free(plut); else noop;

					m_vabt_list[2 * SVM_CAMERAS_NUM + camID].pitxID = ppaitxID[camID];
				}
				else
					throw runtime_error(string("$camID out of range"));
			}
			catch (exception& e)
			{
				throw runtime_error(str_camID + delimiter(string(e.what())));
			}
		}

	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2103001", __FUNCTION__ + delimiter(string(e.what())));
	}
}


void sanCamView::update_RCamViewLUT(int rcamID)
{
	if(rcamID < SVM_CAMERAS_NUM)
	{
		int total_vertex_num = 0;
		GLfloat* camViewLUT = nullptr;
		createDefisheyeLUT(m_pxml->m_rcam[rcamID], m_pcameras->m_xmaps[rcamID], m_pcameras->m_ymaps[rcamID], m_square_size, &camViewLUT, &total_vertex_num);
		updateVAB(2*rcamID+0, camViewLUT, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (camViewLUT != nullptr) free(camViewLUT); else noop;
	
		GLfloat* seamLUT = createSeamLUT(rcamID, m_pxml->m_rcam[rcamID].flipx, m_pxml->m_mask_seam[rcamID]);
		updateVAB(2*rcamID+1, seamLUT, (GLuint)m_pxml->m_mask_seam[rcamID].size(), 3, 0, GL_DYNAMIC_DRAW);
		if (seamLUT != nullptr) free(seamLUT); else noop;
	}
}

void sanCamView::update_ACamViewLUT(int acamID)
{
	if(acamID < ADD_CAMERAS_NUM)
	{
		int total_vertex_num = 0;
		GLfloat* acamViewLUT = nullptr;
		createDefisheyeLUT(m_pxml->m_acam[acamID], m_pcameras->m_axmaps[acamID], m_pcameras->m_aymaps[acamID], m_square_size, &acamViewLUT, &total_vertex_num);
		updateVAB(2*SVM_CAMERAS_NUM+acamID, acamViewLUT, (GLuint)total_vertex_num, 3, 2, GL_DYNAMIC_DRAW);
		if (acamViewLUT != nullptr) free(acamViewLUT); else noop;
	}
}


// GLfloat* sanCamView::createSeamLUT(int camID, bool flipx, vector<Point2f>& pt2d)
// {
// 	float xflip = ((float)flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1
// 	vector<Point2f> defisheye_pt2d = sanWorld::fisheye_to_defisheye(camID, m_pxml, pt2d);
// 	vector<Point2f> defisheye_pt2d_normalized = sanWorld::normalize_points_in_defisheye(camID, m_pxml, defisheye_pt2d);

// 	GLfloat* vtx = (GLfloat*)calloc(pt2d.size() * 3, sizeof(GLfloat));

// 	if (vtx != nullptr)
// 	{
// 		for (int i = 0; i < (int)pt2d.size(); i++)
// 		{
// 			vtx[3 * i + 0] = xflip * defisheye_pt2d_normalized[i].x;
// 			vtx[3 * i + 1] = -defisheye_pt2d_normalized[i].y;
// 			vtx[3 * i + 2] = 0.0f;
// 		}
// 	}
// 	else
// 		throw runtime_error(__FUNCTION__ + delimiter(string(" $failed to allocate memory")));

// 	return vtx;
// }


GLfloat* sanCamView::createSeamLUT(int camID, bool flipx, vector<Point2f>& pt2d)
{
	float vertex_xnorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].cols - (m_pxml->m_rcam[camID].camview_offset.hleft + m_pxml->m_rcam[camID].camview_offset.hright));
	float vertex_ynorm = 1.0f / (float)(m_pcameras->m_xmaps[camID].rows - (m_pxml->m_rcam[camID].camview_offset.vtop + m_pxml->m_rcam[camID].camview_offset.vbot));

	float xflip = ((float)flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1
	vector<Point2f> defisheye_pt2d = sanWorld::fisheye_to_defisheye(camID, m_pxml, pt2d);

	GLfloat* vtx = (GLfloat*)calloc(pt2d.size() * 3, sizeof(GLfloat));

	if (vtx != nullptr)
	{
		for (int i = 0; i < (int)pt2d.size(); i++)
		{
			vtx[3 * i + 0] = xflip * ((defisheye_pt2d[i].x - m_pxml->m_rcam[camID].camview_offset.hleft) * vertex_xnorm - 0.5f) * 2.0f;
			vtx[3 * i + 1] = -((defisheye_pt2d[i].y - m_pxml->m_rcam[camID].camview_offset.vtop) * vertex_ynorm - 0.5f) * 2.0f;
			vtx[3 * i + 2] = 0.0f;
		}
	}
	else
		throw runtime_error(__FUNCTION__ + delimiter(string(" $failed to allocate memory")));

	return vtx;
}


void sanCamView::createDefisheyeLUT(RCAM_PARAMETERS& camparam, Mat& xmap, Mat& ymap, int square_size, GLfloat** pplut/*[out]*/, int* plut_lines_num/*[out]*/)
{
	// float xflip = ((float)m_pxml->m_rcam[camID].flipx - 0.5f) * (-2.0f);		// convert from 0 or 1 into -1 or 1
	float xflip = (float)camparam.flipx;							// 0 or 1

	int lut_lines_num = 0;
	int defisheye_offset_x_left = (int)camparam.camview_offset.hleft;
	int defisheye_offset_x_right = (int)camparam.camview_offset.hright;
	int defisheye_offset_y_top = (int)camparam.camview_offset.vtop;
	int defisheye_offset_y_bot = (int)camparam.camview_offset.vbot;

	if (square_size < 1) square_size = 1;
	else noop;

	int row_num = (xmap.rows - (defisheye_offset_y_top + defisheye_offset_y_bot)) / square_size; // y
	int col_num = (xmap.cols - (defisheye_offset_x_left + defisheye_offset_x_right)) / square_size; // x

	lut_lines_num = 3 * 2 * (row_num - 1) * (col_num - 1); // 3: number of points making up a triangle,  triangle_num = 2 * rectangle_num

	GLfloat* plut = (GLfloat*)calloc((size_t)lut_lines_num * 5, sizeof(GLfloat));  // 5: (x, y, z, u, v)
	if (plut == nullptr)
		throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));
	else
	{

		float vertex_xnorm = 1.0f / (float)(xmap.cols - (defisheye_offset_x_left + defisheye_offset_x_right)); // after clipping left and right, then normalizing
		float vertex_ynorm = 1.0f / (float)(xmap.rows - (defisheye_offset_y_top + defisheye_offset_y_bot)); // fater clipping top and bottom, then normalizing

		float texture_xnorm = 1.0f / (float)xmap.cols; // to normalize x coordinate into 0.0 ~ 1.0
		float texture_ynorm = 1.0f / (float)xmap.rows; // to normalize y coordinate into 0.0 ~ 1.0

		int k = 0;
		for (int row_idx = 0; row_idx < row_num - 1; row_idx++) // y
		{
			for (int col_idx = 0; col_idx < col_num - 1; col_idx++) // x
			{
				int x = defisheye_offset_x_left + col_idx * square_size;
				int y = defisheye_offset_y_top + row_idx * square_size;


				/****************************************** Get triangles ************************
				 *   							  v1 _  v3
				 *   Triangles orientation: 		| /|		1 triangle (v1-v2-v3)
				 *   								|/_|		2 triangle (v3-v2-v4)
				 *   							  v2   v4
				 *********************************************************************************/

				 // Vertices (2D xy point, image coordinate based value)
				Point2f v1 = Point2f((float)(x), (float)(y));
				Point2f v2 = Point2f((float)(x), (float)(y + square_size));
				Point2f v3 = Point2f((float)(x + square_size), (float)(y));
				Point2f v4 = Point2f((float)(x + square_size), (float)(y + square_size));

				// Texels( st point )
				Point2f p1 = Point2f(xmap.at<float>(v1), ymap.at<float>(v1));
				Point2f p2 = Point2f(xmap.at<float>(v2), ymap.at<float>(v2));
				Point2f p3 = Point2f(xmap.at<float>(v3), ymap.at<float>(v3));
				Point2f p4 = Point2f(xmap.at<float>(v4), ymap.at<float>(v4));

				//Point2f p1 = v1;
				//Point2f p2 = v2;
				//Point2f p3 = v3;
				//Point2f p4 = v4;

				if ((p2.x > 0) && (p2.y > 0) && (p2.x < xmap.cols) && (p2.y < xmap.rows) &&
					(p3.x > 0) && (p3.y > 0) && (p3.x < xmap.cols) && (p3.y < xmap.rows))
				{
					// Save triangle points to the output file

					/*****************************************************************************
					 *   							  v1 _	v3
					 *   2 triangle (v1-v2-v3): 		| /
					 *   								|/
					 *   							  v2
					 ******************************************************************************/
					if ((p1.x > 0) && (p1.y > 0) && (p1.x < xmap.cols) && (p1.y < xmap.rows))	// Check if p3 belongs to the input frame)
					{
						//(v1, p1)
						plut[k + 0] = ((v1.x - defisheye_offset_x_left) * vertex_xnorm - 0.5f) * 2.0f;   // normalize a vertex from [0 ~ +1] into [ -1 ~ +1] 
						plut[k + 1] = -((v1.y - defisheye_offset_y_top) * vertex_ynorm - 0.5f) * 2.0f;
						plut[k + 2] = 0.0f;
						plut[k + 3] = xflip * 1.0f + (1.0f - 2 * xflip) * (p1.x * texture_xnorm);    // normalize a texture into [0 ~ +1]
						plut[k + 4] = (p1.y * texture_ynorm);

						//(v2, p2)
						plut[k + 5] = ((v2.x - defisheye_offset_x_left) * vertex_xnorm - 0.5f) * 2.0f;
						plut[k + 6] = -((v2.y - defisheye_offset_y_top) * vertex_ynorm - 0.5f) * 2.0f;
						plut[k + 7] = 0.0f;
						plut[k + 8] = xflip * 1.0f + (1.0f - 2 * xflip) * (p2.x * texture_xnorm);
						plut[k + 9] = (p2.y * texture_ynorm);

						//(v3, p3)
						plut[k + 10] = ((v3.x - defisheye_offset_x_left) * vertex_xnorm - 0.5f) * 2.0f;
						plut[k + 11] = -((v3.y - defisheye_offset_y_top) * vertex_ynorm - 0.5f) * 2.0f;
						plut[k + 12] = 0.0f;
						plut[k + 13] = xflip * 1.0f + (1.0f - 2 * xflip) * (p3.x * texture_xnorm);
						plut[k + 14] = (p3.y * texture_ynorm);

						k += 15;
					}

					/*****************************************************************************
					 *   							  		v3
					 *   1 triangle (v3-v2-v4): 		  /|
					 *   								 /_|
					 *   							  v2   v4
					 *****************************************************************************/
					if ((p4.x >= 0) && (p4.y >= 0) && (p4.x < xmap.cols) && (p4.y < xmap.rows))
					{
						//(v3, p3)
						plut[k + 0] = ((v3.x - defisheye_offset_x_left) * vertex_xnorm - 0.5f) * 2.0f;
						plut[k + 1] = -((v3.y - defisheye_offset_y_top) * vertex_ynorm - 0.5f) * 2.0f;
						plut[k + 2] = 0.0f;
						plut[k + 3] = xflip * 1.0f + (1.0f - 2 * xflip) * (p3.x * texture_xnorm);
						plut[k + 4] = (p3.y * texture_ynorm);

						//(v2, p2)
						plut[k + 5] = ((v2.x - defisheye_offset_x_left) * vertex_xnorm - 0.5f) * 2.0f;
						plut[k + 6] = -((v2.y - defisheye_offset_y_top) * vertex_ynorm - 0.5f) * 2.0f;
						plut[k + 7] = 0.0f;
						plut[k + 8] = xflip * 1.0f + (1.0f - 2 * xflip) * (p2.x * texture_xnorm);
						plut[k + 9] = (p2.y * texture_ynorm);

						//(v4, p4)
						plut[k + 10] = ((v4.x - defisheye_offset_x_left) * vertex_xnorm - 0.5f) * 2.0f;
						plut[k + 11] = -((v4.y - defisheye_offset_y_top) * vertex_ynorm - 0.5f) * 2.0f;
						plut[k + 12] = 0.0f;
						plut[k + 13] = xflip * 1.0f + (1.0f - 2 * xflip) * (p4.x * texture_xnorm);
						plut[k + 14] = (p4.y * texture_ynorm);

						k += 15;
					}
				}
			}
		}

		*pplut = plut;
		*plut_lines_num = lut_lines_num;
	}
}

void sanCamView::renderCamView(GLuint camID)
{	
	sanError::glClearError();

	glm::mat4 mvp = glm::mat4(1.0f);

	m_camviewShader.use();
	m_camviewShader.setMat4("mvp", mvp);
	int vabt_list_index = 2 * camID + 0;

	glBindVertexArray(m_vabt_list[vabt_list_index].vaoID);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *m_vabt_list[vabt_list_index].pitxID);
	glUniform1i(glGetUniformLocation(m_camviewShader.getProgram(), "img"), 0);
	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_list_index].vnum);

	sanError::glCheckError(__FUNCTION__ + string(": step 1"));
	
#if(1) // ROI LINE
	m_camviewSeamShader.use();
	m_camviewSeamShader.setMat4("mvp", mvp);
	m_camviewSeamShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f)); // original is 1.0f, 1.0f, 0.0f Color Changed
	m_camviewSeamShader.setFloat("alpha", 1.0f);
	vabt_list_index = 2 * camID + 1;

	glBindVertexArray(m_vabt_list[vabt_list_index].vaoID);
	glDrawArrays(GL_POINTS, 0, m_vabt_list[vabt_list_index].vnum);

	sanError::glCheckError(__FUNCTION__ + string(": step 2"));
#endif

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D,0);
	glUseProgram(0);

}

void sanCamView::renderAdditionalCamView(GLuint acamID)
{
	sanError::glClearError();

	m_camviewShader.use();
	m_camviewShader.setMat4("mvp", glm::mat4(1.0f));
	int vabt_list_index = 2 * SVM_CAMERAS_NUM + acamID;

	glBindVertexArray(m_vabt_list[vabt_list_index].vaoID);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *m_vabt_list[vabt_list_index].pitxID);
	glUniform1i(glGetUniformLocation(m_camviewShader.getProgram(), "img"), 0);
	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_list_index].vnum);
	
	sanError::glCheckError(__FUNCTION__ + string(": additional camview"));

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

void sanCamView::drawLayout1(bool updateLUT)
{
	try
	{
		glDisable(GL_BLEND);

		switch (m_pxml->m_layout[1].view_mode)
		{
		case TOPVIEW3D:
		case CAMVIEW3D_FRONT:
		case CAMVIEW3D_RIGHT:
		case CAMVIEW3D_REAR:
		case CAMVIEW3D_LEFT:
			break;
		case CAMVIEW2D_FRONT:
		case CAMVIEW2D_RIGHT:
		case CAMVIEW2D_REAR:
		case CAMVIEW2D_LEFT:
		{
			GLuint camID = m_pxml->m_layout[1].view_mode - CAMVIEW2D_FRONT;
			glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

			if(updateLUT) update_RCamViewLUT(camID);
			renderCamView(camID);
			break;
		}
		case CAMVIEW2D_ADD0:
		{
			GLuint acamID = m_pxml->m_layout[1].view_mode - CAMVIEW2D_ADD0;
			glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

			if(updateLUT) update_ACamViewLUT(acamID);
			renderAdditionalCamView(acamID);
			break;
		}
		default:
			string msg = string("$view_mode[") + to_string(m_pxml->m_layout[1].view_mode) + string("] wrong");
			throw runtime_error(msg);
			break;
		}
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2203001", __FUNCTION__ + delimiter(string(e.what())));
	}
}
