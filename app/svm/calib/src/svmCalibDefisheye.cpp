#include "svmCalibDefisheye.hpp"

sanCalibDefisheye::sanCalibDefisheye(sanXML* pxml, sanCamera* pcameras, sanCalibFisheye* pfisheyes)
{
	m_pxml = pxml;
	m_pcameras = pcameras;
	m_pfisheyes = pfisheyes;
}

sanCalibDefisheye::~sanCalibDefisheye()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		if(m_defisheyeLUT[camID] != nullptr)
		{
			free(m_defisheyeLUT[camID]);
			m_defisheyeLUT[camID] = nullptr;
		}
		else noop;
	}

	for (int i = 0; i < (int)m_defisheye_images.size(); i++)
		m_defisheye_images[i].release();

	if (!m_defisheye_images.empty()) m_defisheye_images.clear(); else noop;
	vector<Mat>().swap(m_defisheye_images);

	m_defisheyeShader.~sanShader();
}

void sanCalibDefisheye::initialize()
{
	createDefisheyeLUT();

	if(!std::filesystem::is_directory(std::string(_OUTPUTS_PATH_))) 
	{
		#if defined(_WIN32) || defined(_WIN64)
			bool status = _mkdir(std::string(_OUTPUTS_PATH_).c_str());
		#else
			bool status = mkdir(std::string(_OUTPUTS_PATH_).c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		#endif
		if (status != 0 && errno != EEXIST) 
		{
			throw runtime_error(std::string("Error creating directory: " + std::string(_OUTPUTS_PATH_)));
		}
	}

	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string str_camID = "(camID[" + to_string(camID) + "])";
		try
		{
			generateVAB(m_defisheyeLUT[camID], m_vertices_num[camID], 3, 2, GL_STATIC_DRAW);

			Mat defisheye_image;
			if((int)m_pfisheyes->m_fisheye_images.size() > camID && (int)m_pcameras->m_xmaps.size() > camID && (int)m_pcameras->m_ymaps.size() > camID)
				remap(m_pfisheyes->m_fisheye_images[camID], defisheye_image, m_pcameras->m_xmaps[camID], m_pcameras->m_ymaps[camID], cv::INTER_LINEAR);
			else
				throw runtime_error(string("$camID out of range"));

			m_defisheye_images.push_back(defisheye_image);

			std::string dir = std::string(_PERSPECTIVE_PATH_);
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

			string full_file_path = dir + "/defisheye" + to_string(camID) + ".bmp";
			sanFromFile::write_image(full_file_path, m_defisheye_images[camID]);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1103001", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}

	for (int camID = 0; camID < ADD_CAMERAS_NUM; camID++)
	{
		string str_camID = "(additional camID[" + to_string(camID) + "])";
		try
		{
			Mat defisheye_image;
			if ((int)m_pfisheyes->m_additional_fisheye_images.size() > camID && (int)m_pcameras->m_axmaps.size() > camID && (int)m_pcameras->m_aymaps.size() > camID)
				remap(m_pfisheyes->m_additional_fisheye_images[camID], defisheye_image, m_pcameras->m_axmaps[camID], m_pcameras->m_aymaps[camID], cv::INTER_LINEAR);
			else
				throw runtime_error(string("$additional camID out of range"));

			std::string dir = std::string(_PERSPECTIVE_PATH_);
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

			string full_file_path = dir + "/additional_defisheye" + to_string(camID) + ".bmp";
			sanFromFile::write_image(full_file_path, defisheye_image);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1103001", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
}

void sanCalibDefisheye::createDefisheyeLUT(int density)
{
	try
	{
		int rowNum = 1;                                             // fixed 1
		int colNum = 1;    // ceil up
		float rowNorm = 1.0f / rowNum;
		float colNorm = 1.0f / colNum;
		int rowIdx = 0;
		int colIdx = 0;
		Point2f top_left_point = Point2f((colIdx * colNorm - 0.5f) * 2.0f, -(rowIdx * rowNorm - 0.5f) * 2.0f);

		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			string str_camID = "(camID[" + to_string(camID) + "])";

			int rows = 0, cols = 0;
			int vertices_num = 0;
			Mat xmap, ymap;
			if ((int)m_pcameras->m_xmaps.size() > camID && (int)m_pcameras->m_ymaps.size() > camID)
			{
				m_pcameras->m_xmaps[camID].copyTo(xmap);
				m_pcameras->m_ymaps[camID].copyTo(ymap);
			}
			else
				throw runtime_error(str_camID + delimiter(string("$camID out of range")));

			rows = xmap.rows / density;
			cols = xmap.cols / density;
			vertices_num = 3 * 2 * (rows - 1) * (cols - 1); // 3: number of points making up a triangle

			float* vertices = (float*)calloc((size_t)vertices_num * 5, sizeof(float)); // 5: (x, y, z, u, v)
			if (vertices == nullptr)
				throw runtime_error(str_camID + delimiter(string("$failed to allocate memory")));
			else
			{
				float x_norm = (1.0f / (float)xmap.cols); // to normalize x coordinate into 0.0 ~ 1.0
				float y_norm = (1.0f / (float)xmap.rows); // to normalize y coordinate into 0.0 ~ 1.0

				int k = 0;
				for (int row = 0; row < rows - 1; row++)
				{
					for (int col = 0; col < cols - 1; col++)
					{
						/****************************************** Get triangles *********************************************
						 *   							  v1 _  v3
						 *   Triangles orientation: 		| /|		1 triangle (v1-v2-v3)
						 *   								|/_|		2 triangle (v3-v2-v4)
						 *   							  v2   v4
						 *******************************************************************************************************/
						 // Vertices (2D xy point, image coordinate based value)
						Point2f v1 = Point2f((float)((col + 0) * density), (float)((row + 0) * density));  // v1
						Point2f v2 = Point2f((float)((col + 0) * density), (float)((row + 1) * density));
						Point2f v3 = Point2f((float)((col + 1) * density), (float)((row + 0) * density));
						Point2f v4 = Point2f((float)((col + 1) * density), (float)((row + 1) * density));

						// Texels( uv point )
						Point2f p1 = Point2f(xmap.at<float>(v1), ymap.at<float>(v1));
						Point2f p2 = Point2f(xmap.at<float>(v2), ymap.at<float>(v2));
						Point2f p3 = Point2f(xmap.at<float>(v3), ymap.at<float>(v3));
						Point2f p4 = Point2f(xmap.at<float>(v4), ymap.at<float>(v4));

						if ((p2.x > 0) && (p2.y > 0) && (p2.x < xmap.cols) && (p2.y < xmap.rows) &&	// Check if p2 belongs to the input frame
							(p3.x > 0) && (p3.y > 0) && (p3.x < xmap.cols) && (p3.y < xmap.rows))	// Check if p4 belongs to the input frame
						{
							// Save triangle points to the output file

							/*******************************************************************************************************
							 *   							  v1 _	v3
							 *   2 triangle (v4-v2-v3): 		| /
							 *   								|/
							 *   							  v2
							 *******************************************************************************************************/
							if ((p1.x > 0) && (p1.y > 0) && (p1.x < xmap.cols) && (p1.y < xmap.rows))	// Check if p3 belongs to the input frame)
							{
								//(v1, p1)
								vertices[k + 0] = top_left_point.x + v1.x * x_norm * 2.0f * colNorm;
								vertices[k + 1] = top_left_point.y - v1.y * y_norm * 2.0f * rowNorm;
								vertices[k + 2] = 0.0f;
								vertices[k + 3] = p1.x * x_norm;
								vertices[k + 4] = p1.y * y_norm;

								//(v2, p2)
								vertices[k + 5] = top_left_point.x + v2.x * x_norm * 2.0f * colNorm;
								vertices[k + 6] = top_left_point.y - v2.y * y_norm * 2.0f * rowNorm;
								vertices[k + 7] = 0.0f;
								vertices[k + 8] = p2.x * x_norm;
								vertices[k + 9] = p2.y * y_norm;

								//(v3, p3)
								vertices[k + 10] = top_left_point.x + v3.x * x_norm * 2.0f * colNorm;
								vertices[k + 11] = top_left_point.y - v3.y * y_norm * 2.0f * rowNorm;
								vertices[k + 12] = 0.0f;
								vertices[k + 13] = p3.x * x_norm;
								vertices[k + 14] = p3.y * y_norm;

								k += 15;
							}

							/*******************************************************************************************************
							 *   							  		v3
							 *   1 triangle (v4-v1-v2): 		  /|
							 *   								 /_|
							 *   							  v2   v4
							 *******************************************************************************************************/
							if ((p4.x >= 0) && (p4.y >= 0) && (p4.x < xmap.cols) && (p4.y < xmap.rows))	// Check if p1 belongs to the input frame
							{
								//(v3, p3)
								vertices[k + 0] = top_left_point.x + v3.x * x_norm * 2.0f * colNorm;
								vertices[k + 1] = top_left_point.y - v3.y * y_norm * 2.0f * rowNorm;
								vertices[k + 2] = 0.0f;
								vertices[k + 3] = p3.x * x_norm;
								vertices[k + 4] = p3.y * y_norm;

								//(v2, p2)
								vertices[k + 5] = top_left_point.x + v2.x * x_norm * 2.0f * colNorm;
								vertices[k + 6] = top_left_point.y - v2.y * y_norm * 2.0f * rowNorm;
								vertices[k + 7] = 0.0f;
								vertices[k + 8] = p2.x * x_norm;
								vertices[k + 9] = p2.y * y_norm;

								//(v4, p4)
								vertices[k + 10] = top_left_point.x + v4.x * x_norm * 2.0f * colNorm;
								vertices[k + 11] = top_left_point.y - v4.y * y_norm * 2.0f * rowNorm;
								vertices[k + 12] = 0.0f;
								vertices[k + 13] = p4.x * x_norm;
								vertices[k + 14] = p4.y * y_norm;

								k += 15;
							}
						}
					}
				}

				m_defisheyeLUT[camID] = vertices;
				m_vertices_num[camID] = vertices_num;
			}
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibDefisheye::renderDefisheyes(int camID)
{
	try
	{
		m_defisheyeShader.use();
		glDisable(GL_BLEND);

		string str_camID = "(camID[" + to_string(camID) + "])";
		sanError::glClearError();

		glBindVertexArray(getVaoID(camID));
		glBindBuffer(GL_ARRAY_BUFFER, getVboID(camID));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_pfisheyes->m_vabt_list[camID].texID);
		m_defisheyeShader.setInt("src_img", 0);
		glDrawArrays(GL_TRIANGLES, 0, getVnum(camID));

		sanError::glCheckError(str_camID);

		glBindVertexArray(0);
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1203001", __FUNCTION__ + delimiter(string(e.what())));
	}
}
