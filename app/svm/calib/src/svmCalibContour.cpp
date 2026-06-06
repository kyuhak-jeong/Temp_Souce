#include "svmCalibContour.hpp"
#include "svmError.hpp"
#include "svmWorld.hpp"

sanCalibContour::sanCalibContour(sanXML* pxml, sanCamera* pcameras, sanCalibDefisheye* pdefisheyes, sanTextRenderer* ptextRenderer)
{
	m_pxml = pxml;
	m_pcameras = pcameras;
	m_pdefisheyes = pdefisheyes;
	m_ptextRenderer = ptextRenderer;

	m_selected_point_idx = 0;
	m_is_vabt_manifying_generated = false;
}


sanCalibContour::~sanCalibContour()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		if(m_contourLUT[camID] != nullptr)
		{
			free(m_contourLUT[camID]);
			m_contourLUT[camID] = nullptr;
		}
		else noop;
	}

	if(m_magnifyingLUT != nullptr)
	{
		free(m_magnifyingLUT);
		m_magnifyingLUT = nullptr;
	}
	else noop;

	if(m_magnifyingTexLUT != nullptr)
	{
		free(m_magnifyingTexLUT);
		m_magnifyingTexLUT = nullptr;
	}
	else noop;

	m_contourShader.~sanShader();
}


void sanCalibContour::initialize()
{
	try
	{

	} 
	catch (exception& e)
	{
		throw logger.svm_fatal("C1104001", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibContour::renderContours(int camID, bool render_magnify, GLuint glDrawingType)
{
	try
	{
		sanError::glClearError();

		m_contourShader.use();
		glDisable(GL_BLEND);
		
		string str_camID = "(camID[" + to_string(camID) + "])";
		

		glBindVertexArray(getVaoID(camID+1));
		glBindBuffer(GL_ARRAY_BUFFER, getVboID(camID+1));

		#if defined(_WIN32) || defined(_WIN64)
			if (glDrawingType == GL_POINTS)	glPointSize(3.0f);
			else noop;
		#else
			if (glDrawingType == GL_LINES)	glLineWidth(1.0f);
			else noop;
		#endif

		glDrawArrays(glDrawingType, 0, getVnum(camID+1));
		glBindVertexArray(0);

		// maginifer
		if(render_magnify)
		{
			glBindVertexArray(getVaoID(0));
			glBindBuffer(GL_ARRAY_BUFFER, getVboID(0));
	
			#if defined(_WIN32) || defined(_WIN64)
				if (glDrawingType == GL_POINTS)	glPointSize(3.0f);
				else noop;
			#else
				if (glDrawingType == GL_LINES)	glLineWidth(2.0f);
				else noop;
			#endif
			glDrawArrays(glDrawingType, 0, getVnum(0));
			glBindVertexArray(0);
		}

		// text: point index
		if(1)
		{
			glm::mat4 mvp_cv = glm::ortho(0.0f, m_pxml->m_resolution.display.width, m_pxml->m_resolution.display.height, 0.0f);
			float scale_cv = 0.25;
			glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f);

			for(int i = 0; i < CONTROL_POINTS_NUM; i++)
			{
				Point2f pt = Point2f((float)(m_feature_pts[camID][i].x), (float)(m_feature_pts[camID][i].y));
				m_ptextRenderer->renderText(to_mystring(i+1, 0, 1), glm::vec3(pt.x + 5.0f, pt.y + 5.0f, 0.0f), TEXT_RENDER_MODE::TEXT_CV, mvp_cv, scale_cv, color);
			}
		}

		sanError::glCheckError(str_camID);

	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1204301", __FUNCTION__+ delimiter(string(e.what())));
	}
}

void sanCalibContour::updateContours(int camID)
{
	try
	{
		if(!m_is_vabt_manifying_generated)
		{
			generateVAB(nullptr, 0, 3, 0, GL_DYNAMIC_DRAW);
			m_is_vabt_manifying_generated = true;
		}

		createContourLUT(camID);

		if (!m_is_vabt_generated[camID]) generateVAB(m_contourLUT[camID], (GLuint)m_vertices_num[camID], 3, 0, GL_DYNAMIC_DRAW);
		else updateVAB(camID + 1, m_contourLUT[camID], (GLuint)m_vertices_num[camID], 3, 0, GL_DYNAMIC_DRAW);
		m_is_vabt_generated[camID] = true;


		createMagnifyingLUT(camID, m_feature_pts[camID][m_selected_point_idx], 96, 54);
		updateVAB(0, m_magnifyingLUT, 2 * 4, 3, 0, GL_DYNAMIC_DRAW);
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1204201", __FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibContour::createContourLUT(int camID)
{
	try
	{
		// need to clear old memory when updating new data
		if(m_contourLUT[camID] != nullptr) free(m_contourLUT[camID]); else noop;

		int rowNum = 1;
		int colNum = 1;
		float rowNorm = 1.0f / rowNum;
		float colNorm = 1.0f / colNum;
		int rowIdx = 0;
		int colIdx = 0;
		Point2f top_left_point = Point2f((colIdx * colNorm - 0.5f) * 2.0f, -(rowIdx * rowNorm - 0.5f) * 2.0f);

		string str_camID = "(camID[" + to_string(camID) + "])";
		// int vertices_num = 2 * CONTROL_POINTS_NUM;																		// each line needs 2 points
		int vertices_num = 2 * CONTROL_POINTS_NUM * 2 + (int)(m_pxml->m_calibration_type == AUTO_CALIBRATION) * 2 * 4;		// each line needs 2 points
		float* vertices = (float*)calloc((size_t)vertices_num * 3, sizeof(float));	// 3: (x, y, z)

		if (vertices == nullptr)
			throw runtime_error(str_camID + delimiter(string("$failed to allocate memory")));
		else
		{
			float x_norm = 1.0f / (float)m_pcameras->m_xmaps[camID].cols;
			float y_norm = 1.0f / (float)m_pcameras->m_xmaps[camID].rows;
			float segment_length = 15.0f;

			int k = 0;

			for (int i = 0; i < CONTROL_POINTS_NUM; i++)
			{
				// int next_i = 4 * (int)(i / 4) + ((i + 1) % 4);

				// // first point(x, y, z)
				// vertices[6 * i + 0] = top_left_point.x + m_feature_pts[camID][i].x * x_norm * 2.0f * colNorm;
				// vertices[6 * i + 1] = top_left_point.y - m_feature_pts[camID][i].y * y_norm * 2.0f * rowNorm;
				// vertices[6 * i + 2] = 0.0f;

				// // second point(x, y, z)
				// vertices[6 * i + 3] = top_left_point.x + m_feature_pts[camID][next_i].x * x_norm * 2.0f * colNorm;
				// vertices[6 * i + 4] = top_left_point.y - m_feature_pts[camID][next_i].y * y_norm * 2.0f * rowNorm;
				// vertices[6 * i + 5] = 0.0f;


				vertices[12 * i + 0] = top_left_point.x + (m_feature_pts[camID][i].x + segment_length) * x_norm * 2.0f * colNorm;
				vertices[12 * i + 1] = top_left_point.y - m_feature_pts[camID][i].y * y_norm * 2.0f * rowNorm;
				vertices[12 * i + 2] = 0.0f;

				vertices[12 * i + 3] = top_left_point.x + (m_feature_pts[camID][i].x - segment_length) * x_norm * 2.0f * colNorm;
				vertices[12 * i + 4] = top_left_point.y - m_feature_pts[camID][i].y * y_norm * 2.0f * rowNorm;
				vertices[12 * i + 5] = 0.0f;

				vertices[12 * i + 6] = top_left_point.x + m_feature_pts[camID][i].x * x_norm * 2.0f * colNorm;
				vertices[12 * i + 7] = top_left_point.y - (m_feature_pts[camID][i].y + segment_length) * y_norm * 2.0f * rowNorm;
				vertices[12 * i + 8] = 0.0f;

				vertices[12 * i + 9] = top_left_point.x + m_feature_pts[camID][i].x * x_norm * 2.0f * colNorm;
				vertices[12 * i + 10] = top_left_point.y - (m_feature_pts[camID][i].y - segment_length) * y_norm * 2.0f * rowNorm;
				vertices[12 * i + 11] = 0.0f;

				k += 12;
			}

			// draw ROI region
			if(m_pxml->m_calibration_type == AUTO_CALIBRATION)
			{
				// top left
				vertices[k+0] = top_left_point.x + m_pxml->m_contour.roi_start_x * x_norm * 2.0f * colNorm;
				vertices[k+1] = top_left_point.y - m_pxml->m_contour.roi_start_y * y_norm * 2.0f * rowNorm;
				vertices[k+2] = 0.0f;
				// top right
				vertices[k+3] = top_left_point.x + (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) * x_norm * 2.0f * colNorm;
				vertices[k+4] = top_left_point.y - m_pxml->m_contour.roi_start_y * y_norm * 2.0f * rowNorm;
				vertices[k+5] = 0.0f;

				// top right
				vertices[k+6] = top_left_point.x + (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) * x_norm * 2.0f * colNorm;
				vertices[k+7] = top_left_point.y - m_pxml->m_contour.roi_start_y * y_norm * 2.0f * rowNorm;
				vertices[k+8] = 0.0f;
				// bot right
				vertices[k+9] = top_left_point.x + (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) * x_norm * 2.0f * colNorm;
				vertices[k+10] = top_left_point.y - (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) * y_norm * 2.0f * rowNorm;
				vertices[k+11] = 0.0f;

				// bot right
				vertices[k+12] = top_left_point.x + (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) * x_norm * 2.0f * colNorm;
				vertices[k+13] = top_left_point.y - (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) * y_norm * 2.0f * rowNorm;
				vertices[k+14] = 0.0f;
				// bot left
				vertices[k+15] = top_left_point.x + m_pxml->m_contour.roi_start_x * x_norm * 2.0f * colNorm;
				vertices[k+16] = top_left_point.y - (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) * y_norm * 2.0f * rowNorm;
				vertices[k+17] = 0.0f;

				// bot left
				vertices[k+18] = top_left_point.x + m_pxml->m_contour.roi_start_x * x_norm * 2.0f * colNorm;
				vertices[k+19] = top_left_point.y - (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) * y_norm * 2.0f * rowNorm;
				vertices[k+20] = 0.0f;
				// top left
				vertices[k+21] = top_left_point.x + m_pxml->m_contour.roi_start_x * x_norm * 2.0f * colNorm;
				vertices[k+22] = top_left_point.y - m_pxml->m_contour.roi_start_y * y_norm * 2.0f * rowNorm;
				vertices[k+23] = 0.0f;
			}

			m_contourLUT[camID] = vertices;
			m_vertices_num[camID] = vertices_num;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

void sanCalibContour::createMagnifyingLUT(int camID, XY center, float width, float height)
{
	try
	{
		// need to clear old memory when updating new data
		if(m_magnifyingLUT != nullptr) free(m_magnifyingLUT); else noop;
		if(m_magnifyingTexLUT != nullptr) free(m_magnifyingTexLUT); else noop;
		
		int rowNum = 1;
		int colNum = 1;
		float rowNorm = 1.0f / rowNum;
		float colNorm = 1.0f / colNum;
		int rowIdx = 0;
		int colIdx = 0;
		Point2f top_left_point = Point2f((colIdx * colNorm - 0.5f) * 2.0f, -(rowIdx * rowNorm - 0.5f) * 2.0f);

		string str_camID = "(camID[" + to_string(camID) + "])";
		int vertices_num = 2 * 4; 											// each line needs 2 points
		float* vertices = (float*)calloc((size_t)vertices_num * 3, sizeof(float));	// 3: (x, y, z)
		float* texCoords = (float*)calloc((size_t)(4 * 2), sizeof(float));

		if (vertices == nullptr || texCoords == nullptr)
			throw runtime_error(str_camID + delimiter(string("$failed to allocate memory")));
		else
		{
			float x_norm = 1.0f / (float)m_pcameras->m_xmaps[camID].cols;
			float y_norm = 1.0f / (float)m_pcameras->m_xmaps[camID].rows;

			float x_min = (center.x - width / 2.0f) < 0.0f ? 0.0f : (center.x - width / 2);
			float x_max = (center.x + width / 2.0f) > (float)(m_pcameras->m_xmaps[camID].cols - 1.0f) ? (float)(m_pcameras->m_xmaps[camID].cols - 1.0f) : (center.x + width / 2.0f);
			float y_min = (center.y - height / 2.0f) < 0.0f ? 0.0f : (center.y - height / 2);
			float y_max = (center.y + height / 2.0f) > (float)(m_pcameras->m_xmaps[camID].rows - 1.0f) ? (float)(m_pcameras->m_xmaps[camID].rows - 1.0f) : (center.y + height / 2.0f);

			// top left
			vertices[0] = top_left_point.x + x_min * x_norm * 2.0f * colNorm;
			vertices[1] = top_left_point.y - y_min * y_norm * 2.0f * rowNorm;
			vertices[2] = 0.0f;
			// top right
			vertices[3] = top_left_point.x + x_max * x_norm * 2.0f * colNorm;
			vertices[4] = top_left_point.y - y_min * y_norm * 2.0f * rowNorm;
			vertices[5] = 0.0f;

			// top right
			vertices[6] = top_left_point.x + x_max * x_norm * 2.0f * colNorm;
			vertices[7] = top_left_point.y - y_min * y_norm * 2.0f * rowNorm;
			vertices[8] = 0.0f;
			// bot right
			vertices[9] = top_left_point.x + x_max * x_norm * 2.0f * colNorm;
			vertices[10] = top_left_point.y - y_max * y_norm * 2.0f * rowNorm;
			vertices[11] = 0.0f;

			// bot right
			vertices[12] = top_left_point.x + x_max * x_norm * 2.0f * colNorm;
			vertices[13] = top_left_point.y - y_max * y_norm * 2.0f * rowNorm;
			vertices[14] = 0.0f;
			// bot left
			vertices[15] = top_left_point.x + x_min * x_norm * 2.0f * colNorm;
			vertices[16] = top_left_point.y - y_max * y_norm * 2.0f * rowNorm;
			vertices[17] = 0.0f;

			// bot left
			vertices[18] = top_left_point.x + x_min * x_norm * 2.0f * colNorm;
			vertices[19] = top_left_point.y - y_max * y_norm * 2.0f * rowNorm;
			vertices[20] = 0.0f;
			// top left
			vertices[21] = top_left_point.x + x_min * x_norm * 2.0f * colNorm;
			vertices[22] = top_left_point.y - y_min * y_norm * 2.0f * rowNorm;
			vertices[23] = 0.0f;


			m_magnifyingLUT = vertices;


			#if(1)

				// //======== Texture coordinate for MRT =======
				// /*
				// *   	v4 _  v2
				// *   	  | /|
				// *   	  |/_|
				// *   	v3    v1
				// */

				// bot right
				texCoords[0] = x_max * x_norm;
				texCoords[1] = 1.0f - y_max * y_norm;

				// top right
				texCoords[2] = x_max * x_norm;
				texCoords[3] = 1.0f - y_min * y_norm;

				// bot left
				texCoords[4] = x_min * x_norm;
				texCoords[5] = 1.0f - y_max * y_norm;
				
				// top left
				texCoords[6] = x_min * x_norm;
				texCoords[7] = 1.0f - y_min * y_norm;

			#else

				//======== Texture coordinate for MRT =======
				/*
				*   	v3 _  v1
				*   	  | /|
				*   	  |/_|
				*   	v4    v2
				*/

				// top right
				texCoords[0] = x_max * x_norm;
				texCoords[1] = 1.0f - y_min * y_norm;

				// bot right
				texCoords[2] = x_max * x_norm;
				texCoords[3] = 1.0f - y_max * y_norm;

				// top left
				texCoords[4] = x_min * x_norm;
				texCoords[5] = 1.0f - y_min * y_norm;
				
				// bot left
				texCoords[6] = x_min * x_norm;
				texCoords[7] = 1.0f - y_max * y_norm;

			#endif

			m_magnifyingTexLUT = texCoords;
		}
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}

bool sanCalibContour::getFeaturePoints(int camID)
{
	string str_camID = "(camID[" + to_string(camID) + "])";

	bool ret = false;

	switch (m_pxml->m_calibration_type)
	{
	case MANUAL_CALIBRATION:
		ret = getFeaturePointsFromSetting(camID);
		break;
	case AUTO_CALIBRATION:
		ret = extractFeaturePointsFromDefisheye(camID);
		if(ret)
			saveFeaturePoints(camID);
		else
			ret = getFeaturePointsFromSetting(camID);
		break;
	default:
		cout << "$calibration type[" + to_string(m_pxml->m_calibration_type) + string("] wrong") << endl;
		break;
	}

	logger.record_message(logger.svm_inform("--------", str_camID + "feature extraction step successful").what());

	return ret;
}

void sanCalibContour::saveFeaturePoints(int camID)
{
	for (int i = 0; i < CONTROL_POINTS_NUM; i++)
	{
		m_pxml->m_feature_pts[camID][i].x = m_feature_pts[camID][i].x;
		m_pxml->m_feature_pts[camID][i].y = m_feature_pts[camID][i].y;
	}
}

bool sanCalibContour::getFeaturePointsFromSetting(int camID)
{
	for (int i = 0; i < CONTROL_POINTS_NUM; i++)
	{
		m_feature_pts[camID][i].x = m_pxml->m_feature_pts[camID][i].x;
		m_feature_pts[camID][i].y = m_pxml->m_feature_pts[camID][i].y;
	}
	return true;
}

bool sanCalibContour::extractFeaturePointsFromDefisheye(int camID)
{
	Mat& defisheye_img = m_pdefisheyes->m_defisheye_images[camID];

	std::vector<cv::Point> square_centers = getSquareCenters(defisheye_img);
	std::vector<cv::Point> feature_pts;
	bool ret = postProcessSquareCenters(square_centers, feature_pts);

	for(int i = 0; i < CONTROL_POINTS_NUM; i++)
	{
		m_feature_pts[camID][i].x = (float)feature_pts[i].x;
		m_feature_pts[camID][i].y = (float)feature_pts[i].y;
	}

	return ret;
}

std::vector<cv::Point> sanCalibContour::getSquareCenters(const Mat& img)
{

#if 1

	cv::Mat hsv, lab;
	cv::cvtColor(img, hsv, CV_BGR2HSV);
	cv::cvtColor(img, lab, CV_BGR2Lab);

	std::vector<cv::Scalar> blue_ranges, red_ranges;
	cv::Mat blue_mask, red_mask;

	if(1)
	{
		// HSV color space
		blue_ranges.push_back(cv::Scalar(60, 40, 30));
		blue_ranges.push_back(cv::Scalar(160, 255, 255));
		
		red_ranges.push_back(cv::Scalar(150, 40, 30));
		red_ranges.push_back(cv::Scalar(179, 255, 255));

		cv::inRange(hsv, blue_ranges[0], blue_ranges[1], blue_mask);
		cv::inRange(hsv, red_ranges[0], red_ranges[1], red_mask);
	}
	else
	{
		// CIELab color space
		blue_ranges.push_back(cv::Scalar(0, 100, 0));
		blue_ranges.push_back(cv::Scalar(255, 140, 120));

		red_ranges.push_back(cv::Scalar(0, 140, 120));
		red_ranges.push_back(cv::Scalar(255, 255, 150));

		cv::inRange(lab, blue_ranges[0], blue_ranges[1], blue_mask);
		cv::inRange(lab, red_ranges[0], red_ranges[1], red_mask);
	}

	cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)); // Adjust kernel size
	cv::morphologyEx(blue_mask, blue_mask, cv::MORPH_OPEN, kernel);   // Erode then Dilate
	cv::morphologyEx(blue_mask, blue_mask, cv::MORPH_CLOSE, kernel);  // Dilate then Erode
	cv::morphologyEx(red_mask, red_mask, cv::MORPH_OPEN, kernel);   // Erode then Dilate
	cv::morphologyEx(red_mask, red_mask, cv::MORPH_CLOSE, kernel);  // Dilate then Erode

	std::vector<std::vector<cv::Point>> contours_blue, contours_red;
	cv::findContours(blue_mask, contours_blue, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
	cv::findContours(red_mask, contours_red, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);


	std::vector<cv::Point> centers_blue, centers_red;
	for (const auto& contour : contours_blue)
	{
		double area = cv::contourArea(contour);
		if (area > m_pxml->m_contour.contour_max_area) continue;

		// Approximate contour with a polygon
		std::vector<cv::Point> approx_curve;
		cv::approxPolyDP(contour, approx_curve, cv::arcLength(contour, true) * 0.02, true);

		if (approx_curve.size() >= 4)
		{
			cv::Rect boundRect = cv::boundingRect(approx_curve);
			cv::Point center = cv::Point((int)(boundRect.x + boundRect.width / 2.0), (int)(boundRect.y + boundRect.height / 2.0));

			if(	center.y < m_pxml->m_contour.roi_start_y ||
				center.y > (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) || 
				center.x < m_pxml->m_contour.roi_start_x ||
				center.x > (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) ) 
				continue;

			centers_blue.push_back(center);
			
			if(DEBUGGING_CONTOUR) cv::circle(img, center, 2, cv::Scalar(255, 0, 0), 2);
		}
	}

	for (const auto& contour : contours_red)
	{
		double area = cv::contourArea(contour);
		if (area > m_pxml->m_contour.contour_max_area) continue;

		// Approximate contour with a polygon
		std::vector<cv::Point> approx_curve;
		cv::approxPolyDP(contour, approx_curve, cv::arcLength(contour, true) * 0.02, true);

		if (approx_curve.size() >= 4)
		{
			cv::Rect boundRect = cv::boundingRect(approx_curve);
			cv::Point center = cv::Point((int)(boundRect.x + boundRect.width / 2.0), (int)(boundRect.y + boundRect.height / 2.0));

			if(	center.y < m_pxml->m_contour.roi_start_y ||
				center.y > (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) || 
				center.x < m_pxml->m_contour.roi_start_x ||
				center.x > (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) ) 
				continue;

			centers_red.push_back(center);
			
			if(DEBUGGING_CONTOUR) cv::circle(img, center, 2, cv::Scalar(0, 0, 255), 2);
		}
	}

	std::vector<cv::Point> centers;
	float dist_thres = 30.0f;
	float size_thres = 30.0f;

	std::vector<cv::Point> temp_blue_centers;
	temp_blue_centers.assign(centers_blue.begin(), centers_blue.end());

	for(const auto& red : centers_red)
	{
		float min_dist = std::numeric_limits<float>::max();
		int closest_idx = -1;

		for (int j = 0; j < (int)temp_blue_centers.size(); j++)
		{
			const Point& blue = temp_blue_centers[j];

			if(	blue.y < (red.y - size_thres) || blue.y > (red.y + size_thres) || blue.x < (red.x - size_thres) || blue.x > (red.x + size_thres) )
				continue;

			float current_dist = (float)std::sqrt((red.x - blue.x) * (red.x - blue.x) + (red.y - blue.y) * (red.y - blue.y));

			if (current_dist < min_dist)
			{
				min_dist = current_dist;
				closest_idx = j;
			}
		}

		if(closest_idx != -1 && min_dist < dist_thres)
		{
			cv::Point center = cv::Point((red.x + temp_blue_centers[closest_idx].x) / 2, (red.y + temp_blue_centers[closest_idx].y) / 2);
			centers.push_back(center);
			temp_blue_centers.erase(temp_blue_centers.begin() + closest_idx);
			if(DEBUGGING_CONTOUR) cv::circle(img, center, 2, cv::Scalar(0, 255, 0), 2);
		}
	}

	if(DEBUGGING_CONTOUR)
	{
		cv::rectangle(img, cv::Rect(m_pxml->m_contour.roi_start_x, m_pxml->m_contour.roi_start_y, m_pxml->m_contour.roi_width, m_pxml->m_contour.roi_height), cv::Scalar(0, 0, 255));
		cv::rectangle(blue_mask, cv::Rect(m_pxml->m_contour.roi_start_x, m_pxml->m_contour.roi_start_y, m_pxml->m_contour.roi_width, m_pxml->m_contour.roi_height), cv::Scalar(255, 255, 255));
		cv::rectangle(red_mask, cv::Rect(m_pxml->m_contour.roi_start_x, m_pxml->m_contour.roi_start_y, m_pxml->m_contour.roi_width, m_pxml->m_contour.roi_height), cv::Scalar(255, 255, 255));
		
		std::vector<cv::Point> default_centers = loadDefaultCenters();
		for(int i = 0; i < (int)default_centers.size(); i++)
		{
			cv::circle(img, default_centers[i], 2, cv::Scalar(0, 255, 255), 2);
		}

		// imshow("blue mask", blue_mask);
		// imshow("red mask", red_mask);
		imshow("rgb", img);
		// imshow("hsv", hsv);
		// imshow("lab", lab);
		waitKey(0);
		destroyAllWindows();
	}

	return centers;

#else

	cv::Mat hsv;
	cv::cvtColor(img, hsv, CV_BGR2HSV);

	std::vector<cv::Scalar> blue_ranges;
	blue_ranges.push_back(cv::Scalar(80, 50, 50));  // Lower bound (H, S, V)
	blue_ranges.push_back(cv::Scalar(160, 255, 255)); // Upper bound (H, S, V)

	cv::Mat blue_mask;
	cv::inRange(hsv, blue_ranges[0], blue_ranges[1], blue_mask);

	cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)); // Adjust kernel size
	cv::morphologyEx(blue_mask, blue_mask, cv::MORPH_OPEN, kernel);   // Erode then Dilate
	cv::morphologyEx(blue_mask, blue_mask, cv::MORPH_CLOSE, kernel);  // Dilate then Erode

	std::vector<std::vector<cv::Point>> contours_blue;
	cv::findContours(blue_mask, contours_blue, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);

	std::vector<cv::Point> centers_blue;
	for (const auto& contour : contours_blue)
	{
		double area = cv::contourArea(contour);
		if (area > m_pxml->m_contour.contour_max_area) continue;

		// Approximate contour with a polygon
		std::vector<cv::Point> approx_curve;
		cv::approxPolyDP(contour, approx_curve, cv::arcLength(contour, true) * 0.02, true);

		if (approx_curve.size() >= 4)
		{
			cv::Rect boundRect = cv::boundingRect(approx_curve);
			cv::Point center = cv::Point((int)(boundRect.x + boundRect.width / 2.0), (int)(boundRect.y + boundRect.height / 2.0));

			if(	center.y < m_pxml->m_contour.roi_start_y ||
				center.y > (m_pxml->m_contour.roi_start_y + m_pxml->m_contour.roi_height) || 
				center.x < m_pxml->m_contour.roi_start_x ||
				center.x > (m_pxml->m_contour.roi_start_x + m_pxml->m_contour.roi_width) ) 
				continue;

			centers_blue.push_back(center);
			
			if(DEBUGGING_CONTOUR) cv::circle(img, center, 5, cv::Scalar(255, 0, 0), 2);
			
		}
	}

	if(DEBUGGING_CONTOUR)
	{
		cv::rectangle(img, cv::Rect(m_pxml->m_contour.roi_start_x, m_pxml->m_contour.roi_start_y, m_pxml->m_contour.roi_width, m_pxml->m_contour.roi_height), cv::Scalar(0, 0, 255));
		cv::rectangle(blue_mask, cv::Rect(m_pxml->m_contour.roi_start_x, m_pxml->m_contour.roi_start_y, m_pxml->m_contour.roi_width, m_pxml->m_contour.roi_height), cv::Scalar(255, 255, 255));
		
		std::vector<cv::Point> default_centers = loadDefaultCenters();
		for(int i = 0; i < (int)default_centers.size(); i++)
		{
			cv::circle(img, default_centers[i], 5, cv::Scalar(0, 255, 255), 2);
		}

		// imshow("blue mask", blue_mask);
		imshow("org", img);
		waitKey(0);
		destroyAllWindows();
	}

	return centers_blue;

#endif
}


bool sanCalibContour::postProcessSquareCenters(std::vector<cv::Point> centers /*in*/, std::vector<cv::Point>& sorted_centers /*out*/)
{
	bool ret = false;
	sorted_centers = loadDefaultCenters();

	int num_centers = (int)centers.size();
	std::vector<cv::Point> temp_centers;
	temp_centers.assign(centers.begin(), centers.end());

	if(num_centers > 0)
	{
		for(int i = 0; i < (int)sorted_centers.size(); i++)
		{
			if(i > num_centers) break;

			int idx = i; //(i < CONTROL_POINTS_NUM / 2) ? i : ((CONTROL_POINTS_NUM / 2) + CONTROL_POINTS_NUM - i - 1);

			cv::Point curr_pt = sorted_centers[idx];

			// find closest point from centers with current point
			float min_dist = std::numeric_limits<float>::max();
			int closest_idx = -1;

			for (int j = 0; j < (int)temp_centers.size(); j++)
			{
				const Point& p2 = temp_centers[j];

				if(	p2.y < (curr_pt.y - m_pxml->m_contour.roi_height / 2.0f) ||
					p2.y > (curr_pt.y + m_pxml->m_contour.roi_height / 2.0f) ||
					p2.x < (curr_pt.x - m_pxml->m_contour.roi_width / 3.0f) ||
					p2.x > (curr_pt.x + m_pxml->m_contour.roi_width / 3.0f) )
				continue;


				float current_dist = (float)std::sqrt((curr_pt.x - p2.x) * (curr_pt.x - p2.x) + (curr_pt.y - p2.y) * (curr_pt.y - p2.y));

				if (current_dist < min_dist)
				{
					min_dist = current_dist;
					closest_idx = j;
				}
			}

			if(closest_idx != -1)
			{
				sorted_centers[idx] = temp_centers[closest_idx];
				temp_centers.erase(temp_centers.begin() + closest_idx);
			}

		}

		ret = true;
		printf("Auto detected %d points\n", num_centers);
	}
	else
	{
		printf("Failed to auto detect\n");
	}

	return ret;
}

std::vector<cv::Point> sanCalibContour::loadDefaultCenters()
{
	std::vector<cv::Point> sorted_centers(CONTROL_POINTS_NUM);

	// sorted_centers[0].x = 7 * m_pxml->m_resolution.image.width / 32;
	// sorted_centers[0].y = 12 * m_pxml->m_resolution.image.height / 16;
	// sorted_centers[1].x = 10 * m_pxml->m_resolution.image.width / 32;
	// sorted_centers[1].y = 10 * m_pxml->m_resolution.image.height / 16;
	// sorted_centers[2].x = 13 * m_pxml->m_resolution.image.width / 32;
	// sorted_centers[2].y = 8 * m_pxml->m_resolution.image.height / 16;

	// sorted_centers[3].x = 25 * m_pxml->m_resolution.image.width / 32;
	// sorted_centers[3].y = 12 * m_pxml->m_resolution.image.height / 16;
	// sorted_centers[4].x = 22 * m_pxml->m_resolution.image.width / 32;
	// sorted_centers[4].y = 10 * m_pxml->m_resolution.image.height / 16;
	// sorted_centers[5].x = 19 * m_pxml->m_resolution.image.width / 32;
	// sorted_centers[5].y = 8 * m_pxml->m_resolution.image.height / 16;


	sorted_centers[0].x = m_pxml->m_contour.roi_start_x + 2 * m_pxml->m_contour.roi_width / 16;
	sorted_centers[0].y = m_pxml->m_contour.roi_start_y + 3 * m_pxml->m_contour.roi_height / 4;
	sorted_centers[1].x = m_pxml->m_contour.roi_start_x + 3 * m_pxml->m_contour.roi_width / 16;
	sorted_centers[1].y = m_pxml->m_contour.roi_start_y + 2 * m_pxml->m_contour.roi_height / 4;
	sorted_centers[2].x = m_pxml->m_contour.roi_start_x + 4 * m_pxml->m_contour.roi_width / 16;
	sorted_centers[2].y = m_pxml->m_contour.roi_start_y + 1 * m_pxml->m_contour.roi_height / 4;

	sorted_centers[3].x = m_pxml->m_contour.roi_start_x + 14 * m_pxml->m_contour.roi_width / 16;
	sorted_centers[3].y = m_pxml->m_contour.roi_start_y + 3 * m_pxml->m_contour.roi_height / 4;
	sorted_centers[4].x = m_pxml->m_contour.roi_start_x + 13 * m_pxml->m_contour.roi_width / 16;
	sorted_centers[4].y = m_pxml->m_contour.roi_start_y + 2 * m_pxml->m_contour.roi_height / 4;
	sorted_centers[5].x = m_pxml->m_contour.roi_start_x + 12 * m_pxml->m_contour.roi_width / 16;
	sorted_centers[5].y = m_pxml->m_contour.roi_start_y + 1 * m_pxml->m_contour.roi_height / 4;

	return sorted_centers;
}
