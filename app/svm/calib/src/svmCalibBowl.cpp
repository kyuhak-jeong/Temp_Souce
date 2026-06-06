#include "svmCalibBowl.hpp"
#include "svmWorld.hpp"
#include "svmError.hpp"

sanCalibBowl::sanCalibBowl(sanXML* pxml, sanCalibFisheye* pfisheye, sanCamera* pcameras, sanCalibGrid* pgrids, sanCalibMask* pmasks)
{
	m_pxml = pxml;
	m_pfisheye = pfisheye;
	m_pcameras = pcameras;
	m_pgrids = pgrids;
	m_pmasks = pmasks;
}

sanCalibBowl::~sanCalibBowl()
{
	if (!m_all_luts.empty()) m_all_luts.clear(); else noop;
	if (!m_overlap_luts.empty()) m_overlap_luts.clear(); else noop;
	if (!m_nonoverlap_luts.empty()) m_nonoverlap_luts.clear(); else noop;

	m_bowlShader.~sanShader();
}

void sanCalibBowl::initialize()
{
	for (int camID = 0; camID < 2 * SVM_CAMERAS_NUM; camID++)   // for drawing fisheye(source) images
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			generateVAB(NULL, 0, 3, 2, GL_STATIC_DRAW);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1107001", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
}


void sanCalibBowl::updateLUTs()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			float img_width = (float)m_pcameras->m_xmaps[camID].cols;
			float img_height = (float)m_pcameras->m_xmaps[camID].rows;

			// overlap
			int vabt_list_index_overlap = 2 * camID + 0;
			int vnum = (int)m_overlap_luts[camID].size();
			float* overlap_data = (float*)new float[vnum * 5];

			for (int i = 0, k = 0; i < vnum; i++)
			{
				vector<Point3f> local_pt3d;
				local_pt3d.push_back(Point3f(m_overlap_luts[camID][i][0], m_overlap_luts[camID][i][1], m_overlap_luts[camID][i][2]));
				vector<Point3f> global_pt3d = sanWorld::LOCAL_to_GL(camID, local_pt3d);

				overlap_data[k + 0] = global_pt3d[0].x;
				overlap_data[k + 1] = global_pt3d[0].y;
				overlap_data[k + 2] = global_pt3d[0].z;
				overlap_data[k + 3] = m_overlap_luts[camID][i][3] / img_width;
				overlap_data[k + 4] = m_overlap_luts[camID][i][4] / img_height;
				k += 5;
			}

			setVnum(vabt_list_index_overlap, vnum);
			updateVAB(vabt_list_index_overlap, overlap_data, vnum, 3, 2, GL_DYNAMIC_DRAW);

			if (overlap_data != nullptr) delete[] overlap_data;
			else noop;

			// nonoverlap
			int vabt_list_index_nonoverlap = 2 * camID + 1;
			vnum = (int)m_nonoverlap_luts[camID].size();
			float* nonoverlap_data = (float*)new float[vnum * 5];

			for (int i = 0, k = 0; i < vnum; i++)
			{
				vector<Point3f> local_pt3d;
				local_pt3d.push_back(Point3f(m_nonoverlap_luts[camID][i][0], m_nonoverlap_luts[camID][i][1], m_nonoverlap_luts[camID][i][2]));
				vector<Point3f> global_pt3d = sanWorld::LOCAL_to_GL(camID, local_pt3d);

				nonoverlap_data[k + 0] = global_pt3d[0].x;
				nonoverlap_data[k + 1] = global_pt3d[0].y;
				nonoverlap_data[k + 2] = global_pt3d[0].z;
				nonoverlap_data[k + 3] = m_nonoverlap_luts[camID][i][3] / img_width;
				nonoverlap_data[k + 4] = m_nonoverlap_luts[camID][i][4] / img_height;
				k += 5;
			}

			setVnum(vabt_list_index_nonoverlap, vnum);
			updateVAB(vabt_list_index_nonoverlap, nonoverlap_data, vnum, 3, 2, GL_DYNAMIC_DRAW);

			if (nonoverlap_data != nullptr) delete[] nonoverlap_data;
			else noop;
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1207201", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
}



void sanCalibBowl::renderBowl()
{
	try
	{
		#if defined(_WIN32) || defined(_WIN64)
			glEnable(GL_BLEND);
		#else
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		#endif

		m_bowlShader.use();
		#if(0)
			float scale = 0.2f;
			glm::mat4 mv = glm::scale(glm::mat4(1.0f), scale * glm::vec3(1.0f, 1.0f, 1.0f));  // view_matrix * scale_matrix
		#else
			glm::mat4 mv = glm::scale(glm::mat4(1.0f), glm::vec3(0.3f, 0.5f, 0.5f));
			mv = glm::rotate(mv, (float)M_PI / 2.0f, glm::vec3(0.0f, 0.0f, 1.0f));
		#endif

		glm::vec4 compColor = glm::vec4(0.0f);

		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			// if(camID == 2) compColor = glm::vec4(0.1f, 0.1f, 0.1f, 0.0f);

			string str_camID = "(camID[" + to_string(camID) + "])";
			sanError::glClearError();

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, m_pfisheye->getTexID(camID));
			m_bowlShader.setInt("src_img", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, m_pmasks->getTexID(camID));
			m_bowlShader.setInt("msk_img", 1);
			m_bowlShader.setMat4("mv", mv);
			m_bowlShader.setVec4("compensate", compColor);

			// overlap
			int vabt_idx_overlap = camID * 2 + 0;
			glBindVertexArray(getVaoID(vabt_idx_overlap));
			glBindBuffer(GL_ARRAY_BUFFER, getVboID(vabt_idx_overlap));
			glDrawArrays(GL_TRIANGLES, 0, getVnum(vabt_idx_overlap));

			// nonoverlap
			int vabt_idx_nonoverlap = camID * 2 + 1;
			glBindVertexArray(getVaoID(vabt_idx_nonoverlap));
			glBindBuffer(GL_ARRAY_BUFFER, getVboID(vabt_idx_nonoverlap));
			glDrawArrays(GL_TRIANGLES, 0, getVnum(vabt_idx_nonoverlap));

			sanError::glCheckError(str_camID);
		}

		glBindVertexArray(0);
		glDisable(GL_BLEND);
	}
	catch (exception& e)
	{	
		throw logger.svm_fatal("C1207401", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibBowl::getLUTs()
{
	if (!m_all_luts.empty()) m_all_luts.clear(); else noop;
	if (!m_overlap_luts.empty()) m_overlap_luts.clear(); else noop;
	if (!m_nonoverlap_luts.empty()) m_nonoverlap_luts.clear(); else noop;

	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			createLUT(camID); // with openCV coordinates
			splitLUT(camID);
			saveLUT(camID);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1207101", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}

	logger.record_message(logger.svm_inform("--------", "LUT creation successful - overal steps successful").what());
}

void sanCalibBowl::createLUT(int camID) // for defisheye
{
	try
	{
		float img_width = (float)m_pcameras->m_xmaps[camID].cols;
		float img_height = (float)m_pcameras->m_xmaps[camID].rows;
		vector<Point3f>& v3d = m_pgrids->m_grids[camID].v3d;  // GL Space
		vector<Point2f>& p2d = m_pgrids->m_grids[camID].p2d;  // abnormalized defisheye image points
		int actual_arcs_num = m_pgrids->m_grids[camID].actual_arcs_num;
		int points_num_per_arc = 2 * (m_pgrids->m_grids[camID].actual_circle_steps_num + m_pgrids->m_grids[camID].actual_parabola_steps_num);

		vector<array<float, 5>> tmp_lut;

		for (int arc_index = 0; arc_index < actual_arcs_num; arc_index++)
		{
			for (int i = 0; i < points_num_per_arc - 2; i += 2)
			{
				/**************************** Get triangles for I quadrant of template **********************************
				 *   						     p0 _  p2
				 *   Triangles orientation: 		| /|		1st triangle (p0 -> p1 -> p2)
				 *   								|/_|		2nd triangle (p2 -> p1 -> p3)
				 *   							 p1    p3
				 *******************************************************************************************************/

				int idx = arc_index * points_num_per_arc + i;
				Point2f p0 = p2d[idx + 0];
				Point2f p1 = p2d[idx + 1];
				Point2f p2 = p2d[idx + 2];
				Point2f p3 = p2d[idx + 3];

				if ((round(p1.x) < img_width) && (round(p1.y) < img_height) && (p1.x >= 0) && (p1.y >= 0) &&
					(round(p2.x) < img_width) && (round(p2.y) < img_height) && (p2.x >= 0) && (p2.y >= 0))
				{
					Point3f v1 = v3d[idx + 1]; //vertex
					Point3f v2 = v3d[idx + 2]; //vertex
					Point2f t1 = Point2f(m_pcameras->m_xmaps[camID].at<float>(p1), m_pcameras->m_ymaps[camID].at<float>(p1)); // abnormalized fisheye image(texel)
					Point2f t2 = Point2f(m_pcameras->m_xmaps[camID].at<float>(p2), m_pcameras->m_ymaps[camID].at<float>(p2)); // abnormalized fisheye image(texel)

					// 1st triangle (p0 -> p1 -> p2)
					if ((round(p0.x) < img_width) && (round(p0.y) < img_height) && (p0.x >= 0) && (p0.y >= 0))
					{
						Point3f v0 = v3d[idx + 0];
						Point2f t0 = Point2f(m_pcameras->m_xmaps[camID].at<float>(p0), m_pcameras->m_ymaps[camID].at<float>(p0)); // abnormalized fisheye image(texel)

						tmp_lut.push_back(array<float, 5>{v0.x, v0.y, v0.z, t0.x, t0.y});
						tmp_lut.push_back(array<float, 5>{v1.x, v1.y, v1.z, t1.x, t1.y});
						tmp_lut.push_back(array<float, 5>{v2.x, v2.y, v2.z, t2.x, t2.y});
					}
					else noop;

					// 2nd triangle (p2 -> p1 -> p3)
					if ((round(p3.x) < img_width) && (round(p3.y) < img_height) && (p3.x >= 0) && (p3.y >= 0))
					{
						Point3f v3 = v3d[idx + 3];
						Point2f t3 = Point2f(m_pcameras->m_xmaps[camID].at<float>(p3), m_pcameras->m_ymaps[camID].at<float>(p3)); // abnormalized fisheye image(texel)

						tmp_lut.push_back(array<float, 5>{v2.x, v2.y, v2.z, t2.x, t2.y});
						tmp_lut.push_back(array<float, 5>{v1.x, v1.y, v1.z, t1.x, t1.y});
						tmp_lut.push_back(array<float, 5>{v3.x, v3.y, v3.z, t3.x, t3.y});
					}
					else noop;

				}
				else noop;
			}
		}

		if (!tmp_lut.empty()) m_all_luts.push_back(tmp_lut);
		else noop;
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibBowl::splitLUT(int camID) // overlapped and nonoverlapped
{
	try
	{
		int img_width = m_pmasks->m_masks[camID].cols;
		int img_height = m_pmasks->m_masks[camID].rows;

		vector<array<float, 5>> overlap_lut;
		vector<array<float, 5>> nonoverlap_lut;

		for (int i = 0; i < (int)m_all_luts[camID].size() - 2; i += 3)
		{
			uint pixels_sum = 0;
			int i0 = i + 0, i1 = i + 1, i2 = i + 2;
			float u0 = m_all_luts[camID][i0][3];  // abnormalized fisheye image point  p0
			float v0 = m_all_luts[camID][i0][4];
			float u1 = m_all_luts[camID][i1][3];  // abnormalized fisheye image point  p1
			float v1 = m_all_luts[camID][i1][4];
			float u2 = m_all_luts[camID][i2][3];  // abnormalized fisheye image point  p2
			float v2 = m_all_luts[camID][i2][4];

			// about one triangle (3 points)
			if ((u0 < img_width) && (v0 < img_height) && (u0 > 0) && (v0 > 0))
				pixels_sum += m_pmasks->m_masks[camID].at<uchar>((int)v0, (int)u0); // (y, x)
			else noop;

			if ((u1 < img_width) && (v1 < img_height) && (u1 > 0) && (v1 > 0))
				pixels_sum += m_pmasks->m_masks[camID].at<uchar>((int)v1, (int)u1);
			else noop;

			if ((u2 < img_width) && (v2 < img_height) && (u2 > 0) && (v2 > 0))
				pixels_sum += m_pmasks->m_masks[camID].at<uchar>((int)v2, (int)u2);
			else  noop;

			if (pixels_sum == 3 * 255)   // nonoverlap
			{
				nonoverlap_lut.push_back(m_all_luts[camID][i0]);
				nonoverlap_lut.push_back(m_all_luts[camID][i1]);
				nonoverlap_lut.push_back(m_all_luts[camID][i2]);
			}
			else if (pixels_sum != 0)   // overlap
			{
				overlap_lut.push_back(m_all_luts[camID][i0]);
				overlap_lut.push_back(m_all_luts[camID][i1]);
				overlap_lut.push_back(m_all_luts[camID][i2]);
			}
			else noop;
		}

		if (!overlap_lut.empty()) m_overlap_luts.push_back(overlap_lut);
		else noop;

		if (!nonoverlap_lut.empty()) m_nonoverlap_luts.push_back(nonoverlap_lut);
		else noop;

#if (0)
		cout << "split camID[" << camID << "] : " << m_all_luts[camID].size() << "," << m_overlap_luts[camID].size() << ", " << m_nonoverlap_luts[camID].size() << endl;
#endif

	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

int sanCalibBowl::saveLUT(int camID) // overlapped and nonoverlapped
{
	try
	{
		float img_width = (float)m_pcameras->m_xmaps[camID].cols;
		float img_height = (float)m_pcameras->m_xmaps[camID].rows;

		// 1. rotate the bowl direction from front-back to up-down (openCV-based X-axis -90)
		// 2. place the bowl into the world space ( front, right, rear and left , openCV Y-axis rotation)
		// 3. convert openCV coordinates to openGL coordinates

		std::string dir = std::string(_ARRAYS_PATH_);
		if(!std::filesystem::is_directory(dir)) 
		{
			#if defined(_WIN32) || defined(_WIN64)
				bool status = _mkdir(dir.c_str());
			#else
				bool status = mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
			#endif
			if (status != 0 && errno != EEXIST) 
			{
				throw runtime_error(std::string("Error creating directory: " + dir));
			}
		}

		ofstream outC_overlap;
		string file_path_overlap = dir + "/array" + to_string(camID + 1) + "1";
		outC_overlap.open(file_path_overlap.c_str(), std::ofstream::out | std::ofstream::trunc);

		// overlapped grids 
		for (int i = 0; i < (int)m_overlap_luts[camID].size(); i++)
		{
			float tx = m_overlap_luts[camID][i][3] / img_width;
			float ty = m_overlap_luts[camID][i][4] / img_height;

			vector<Point3f> local_pt3d;
			local_pt3d.push_back(Point3f(m_overlap_luts[camID][i][0], m_overlap_luts[camID][i][1], m_overlap_luts[camID][i][2]));
			vector<Point3f> global_pt3d = sanWorld::LOCAL_to_GL(camID, local_pt3d);
			outC_overlap << global_pt3d[0].x << " " << global_pt3d[0].y << " " << global_pt3d[0].z << " " << tx << " " << ty << endl;
		}
		outC_overlap.close();

		// non-overlapped grids
		ofstream outC_nonoverlap;
		string file_path_nonoverlap = dir + "/array" + to_string(camID + 1) + "2";
		outC_nonoverlap.open(file_path_nonoverlap.c_str(), std::ofstream::out | std::ofstream::trunc);

		for (int i = 0; i < (int)m_nonoverlap_luts[camID].size(); i++)
		{
			float tx = m_nonoverlap_luts[camID][i][3] / img_width;
			float ty = m_nonoverlap_luts[camID][i][4] / img_height;

			vector<Point3f> local_pt3d;
			local_pt3d.push_back(Point3f(m_nonoverlap_luts[camID][i][0], m_nonoverlap_luts[camID][i][1], m_nonoverlap_luts[camID][i][2]));
			vector<Point3f> global_pt3d = sanWorld::LOCAL_to_GL(camID, local_pt3d);
			outC_nonoverlap << global_pt3d[0].x << " " << global_pt3d[0].y << " " << global_pt3d[0].z << " " << tx << " " << ty << endl;
		}
		outC_nonoverlap.close();

		return(0);
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}
 


