#include "svmCalibMask.hpp"
#include "svmProject.hpp"
#include "svmError.hpp"

sanCalibMask::sanCalibMask(sanXML* pxml, sanCamera* pcameras, sanCalibGrid* pgrids)
{
	m_pxml = pxml;
	m_pcameras = pcameras;
	m_pgrids = pgrids;
}

sanCalibMask::~sanCalibMask()
{
	for (int i = 0; i < (int)m_masks.size(); i++)
		m_masks[i].release();

	if(!m_masks.empty()) m_masks.clear(); else noop;
	vector<Mat>().swap(m_masks);

	if(!m_left_seam_lines.empty()) m_left_seam_lines.clear(); else noop;
	if(!m_right_seam_lines.empty()) m_right_seam_lines.clear(); else noop;

	m_maskShader.~sanShader();
}

void sanCalibMask::initialize()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			//because of the precision
			float* data = (float*) new float[30];
			for (int i = 0; i < 30; i++)
				data[i] = (m_maskLUT[i] * 10000.0f + 0.5f) / 10000.0f;

			sanVABT::generateVAB(data, 6, 3, 2, GL_DYNAMIC_DRAW);

			int vabt_index = getVABTLastIndex();
			generateTexture(GL_TEXTURE0, &m_vabt_list[vabt_index].texID);

			if (data != nullptr) delete[] data; else noop;
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1106001", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}	
}

void sanCalibMask::updateMasks()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			int img_width = m_pcameras->m_xmaps[camID].cols;
			int img_height = m_pcameras->m_xmaps[camID].rows;
			int img_channels = m_masks[camID].channels();
			GLint texID = getTexID(camID);
			sanVABT::updateTexture(GL_TEXTURE0, texID, m_masks[camID].ptr(), img_width, img_height, img_channels, NO_BINDING);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1206301", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
}



void sanCalibMask::renderMasks(int camID)
{	
	try
	{
		m_maskShader.use();
		glDisable(GL_BLEND);

		string str_camID = "(camID[" + to_string(camID) + "])";
		sanError::glClearError();

		glBindVertexArray(getVaoID(camID));
		glBindBuffer(GL_ARRAY_BUFFER, getVboID(camID));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, getTexID(camID));
		m_maskShader.setInt("mask_img", 0);
		glDrawArrays(GL_TRIANGLES, 0, getVnum(camID));

		sanError::glCheckError(str_camID);

		glBindVertexArray(0);
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1206401", __FUNCTION__ + delimiter(string(e.what())));
	}
}


Line sanCalibMask::getAlphaBeta(Point2f p1, Point2f p2)
{
	Line result;
	float denumerator = (p2.x - p1.x);

	if(fabs(denumerator) <= FLT_EPSILON) // If line is x = X, then alpha = INFINITY and beta = X.
	{
		result.alpha = INFINITY;
		result.beta = p1.x;
	}
	else // Otherwise
	{
		result.alpha = (p2.y - p1.y) / denumerator; // inclination
		result.beta = (p1.y * p2.x - p2.y * p1.x) / denumerator; // y-intercept
	}
	return result;
}


Point2f sanCalibMask::getIntersection(Line line1, Line line2)
{
	Point2f result;
	if (line1.alpha == line2.alpha) // If lines are parallel
	{
		result.x = INFINITY;
		result.y = INFINITY;
	}
	else
	{
		if (line1.alpha == INFINITY) // If the first line is x = const
		{
			result.x = (float)line1.beta;
			result.y = (float)(line2.alpha * result.x + line2.beta);
		}
		else
		{
			if (line2.alpha == INFINITY) // If the second line is x = const
			{
				result.x = (float)line2.beta;
				result.y = (float)(line1.alpha * result.x + line1.beta);
			}
			else // Otherwise
			{
				result.x = (float)((line2.beta - line1.beta) / (line1.alpha - line2.alpha));
				result.y = (float)((line1.alpha * line2.beta - line2.alpha * line1.beta) / (line1.alpha - line2.alpha));
			}
		}
	}

	return result;
}

/**************************************************************************************************************
 *
 * @brief  			Calculate masks for 3D BEV.
 *
 * @param  	in		vector<sanCamera*> cameras - vector of sanCamera objects
 * 			in		vector< vector<Point3f> > &seam_points - pointer to the vector containing seam points for all grids
 * 					The grid edge consist of 8 points. Points 1-4 describe the left edge of grid (they are located in II quadrant).
 * 					Points 5-8 describe right edge of grid (they are located in I quadrant).
 * 					- 1st point is located on flat circle base (z = 0). It is the leftmost point with minimum value of y coordinate;
 * 					- 2nd point is located on flat circle base (z = 0). It is the leftmost point of grid which lies on base circle edge;
 * 					- 3rd point is located on bowl edge. It is the last point in first grid column with (z != 0);
 * 					- 4th point is located on bowl edge. It is the leftmost point with maximum value of z coordinate.
 * 					- 5th point is located on bowl edge. It is the rightmost point with maximum value of z coordinate.
 * 					- 6th point is located on bowl edge. It is the last point in last grid column with (z != 0);
 * 					- 7th point is located on flat circle base (z = 0). It is the rightmost point of grid which lies on base circle edge;
 *					- 8th point is located on flat circle base (z = 0). It is the rightmost point with minimum value of y coordinate.
 *			in		float smothing - smothing angle value
 *
 * @return 			-
 *
 * @remarks 		The function calculates masks for 3D BEV. The masks will be used for texture mapping.
 * 					They must be defined for original captured image from camera (with fisheye distortion)
 * 					because the same transformation will be applied on camera frames and masks.
 *
 * 					The procedure of mask calculation:
 *					-	Calculate seams for every two adjacent grids. The seam of two adjacent grids is a line y = a * x + b.
 *						Coefficients a and b have been found from grid intersection. The seam points have been defined for
 *						3D template and then have been projected to an image plane using projecPoints function from OpenCV
 *						library. To use the projectPoints function it is necessary to know extrinsic and intrinsic camera
 *						parameters: camera matrix, distortion coefficients, rotation and translation vectors.
 *					-	Create masks which are limited to seams. All 2D seam points describe a convex polygon which defines
 *						the mask in 2D image. The polygon is filled with white color, and background is filled with black color.
 *					-	Smooth mask edges.
 *
 **************************************************************************************************************/
void sanCalibMask::createMasks()
{
	try
	{
		float smothing_angle = m_pxml->m_grid.smooth_angle;

		if (!m_masks.empty()) m_masks.clear(); else noop;
		if (!m_left_seam_lines.empty()) m_left_seam_lines.clear(); else noop;
		if (!m_right_seam_lines.empty()) m_right_seam_lines.clear(); else noop;

		// Get grids intersection points and  a line equation
		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			string str_camID = "(camID[" + to_string(camID) + "])";
			try
			{
				// ex) if the current camera is the front, the the previsous is the left camera and the the next is the right camera.
				vector<Point3f>& previous_seam_candidators = m_pgrids->m_grids[PREV(camID, SVM_CAMERAS_NUM - 1)].seam_candidators_local;
				vector<Point3f>& current_seam_candidators = m_pgrids->m_grids[camID].seam_candidators_local;
				vector<Point3f>& next_seam_candidators = m_pgrids->m_grids[NEXT(camID, SVM_CAMERAS_NUM - 1)].seam_candidators_local;

				SEAM_LINE left_center_seam_line;
				getSeamLine(current_seam_candidators, previous_seam_candidators, LEFT_SIDE, left_center_seam_line); // intersection with the previous seam candidator point
				m_left_seam_lines.push_back(left_center_seam_line);

				SEAM_LINE right_center_seam_line;
				getSeamLine(current_seam_candidators, next_seam_candidators, RIGHT_SIDE, right_center_seam_line); // intersection with the next seam candidator point
				m_right_seam_lines.push_back(right_center_seam_line);
			}
			catch (exception& e)
			{
				throw runtime_error(str_camID + delimiter(string(e.what())));
			}
		}

		// Calculate seams
		for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
		{
			string str_camID = "(camID[" + to_string(camID) + "])";
			try
			{
				vector<Point3f> seam3d;
				vector<Point3f>& seam_candidators = m_pgrids->m_grids[camID].seam_candidators_local;
				double circle_radius = sqrt(pow(seam_candidators[1].x, 2) + pow(seam_candidators[1].y, 2.0));

				// Left edge(left intersection point of the left seam line: y = alpha * x + beta and the circle: x^2 + y^2 = r^2)
				m_left_seam_lines[camID].ip2.x = (float)((-1.0f * m_left_seam_lines[camID].line.alpha * m_left_seam_lines[camID].line.beta - sqrt(pow(circle_radius, 2) * (1 + pow(m_left_seam_lines[camID].line.alpha, 2)) - pow(m_left_seam_lines[camID].line.beta, 2))) / (1 + pow(m_left_seam_lines[camID].line.alpha, 2)));
				m_left_seam_lines[camID].ip2.y = (float)(sqrt(pow(circle_radius, 2) - pow(m_left_seam_lines[camID].ip2.x, 2.0)));
				double cos_l = m_left_seam_lines[camID].ip2.x / circle_radius;

				// Right edge(right intersection point of the right seam line: y = alpha * x + beta and the circle: x^2 + y^2 = r^2)
				m_right_seam_lines[camID].ip2.x = (float)((-1.0f * m_right_seam_lines[camID].line.alpha * m_right_seam_lines[camID].line.beta + sqrt(pow(circle_radius, 2) * (1 + pow(m_right_seam_lines[camID].line.alpha, 2)) - pow(m_right_seam_lines[camID].line.beta, 2))) / (1 + pow(m_right_seam_lines[camID].line.alpha, 2)));
				m_right_seam_lines[camID].ip2.y = (float)(sqrt(pow(circle_radius, 2) - pow(m_right_seam_lines[camID].ip2.x, 2.0)));
				double cos_r = m_right_seam_lines[camID].ip2.x / circle_radius;

				// left bottom seam
				for (double xx = m_left_seam_lines[camID].ip1.x; xx >= m_left_seam_lines[camID].ip2.x; xx -= SEAM_STEP)
					seam3d.push_back(Point3f((float)xx, (float)(xx * m_left_seam_lines[camID].line.alpha + m_left_seam_lines[camID].line.beta), 0.0f));

				// left vertical seam
				for (double zz = 0; zz < abs(seam_candidators[3].z); zz += SEAM_STEP) //p4
				{
					double new_radius = (circle_radius + sqrt(zz));
					double new_x = new_radius * cos_l;
					double new_y = -1.0 * new_x * tan(acos(cos_l));
					seam3d.push_back(Point3f((float)new_x, (float)new_y, (float)(-zz)));
				}

				// top seam
				for (double cos_idx = cos_l; cos_idx < cos_r; cos_idx += SEAM_STEP)
				{
					double new_radius = (circle_radius + sqrt(abs(seam_candidators[3].z)));
					double new_x = new_radius * cos_idx;
					double new_y = -1.0 * new_x * tan(acos(cos_idx));
					seam3d.push_back(Point3f((float)new_x, (float)new_y, seam_candidators[3].z));
				}

				// right vertical seam
				for (double zz = abs(seam_candidators[4].z); zz > 0; zz -= SEAM_STEP) //p5
				{
					double new_radius = (circle_radius + sqrt(zz));
					double new_x = new_radius * cos_r;
					double new_y = -1.0 * new_x * tan(acos(cos_r));
					seam3d.push_back(Point3f((float)new_x, (float)new_y, (float)(-zz)));
				}

				// right bottom seam
				for (double xx = m_right_seam_lines[camID].ip2.x; xx >= m_right_seam_lines[camID].ip1.x; xx -= SEAM_STEP)
					seam3d.push_back(Point3f((float)xx, (float)(xx * m_right_seam_lines[camID].line.alpha + m_right_seam_lines[camID].line.beta), 0.0f));

				// right-left connection seam
				Line bottom = getAlphaBeta(m_right_seam_lines[camID].ip1, m_left_seam_lines[camID].ip1);
				for (double xx = m_right_seam_lines[camID].ip1.x; xx > m_left_seam_lines[camID].ip1.x; xx -= SEAM_STEP)
					seam3d.push_back(Point3f((float)xx, (float)(bottom.alpha * xx + bottom.beta), 0.0f));


				////////////////////////////////////    to make a mask image /////////////////////////////////////
				vector<Point2f> seam2d_defisheye;
				sanProject::projectPoints(seam3d, m_pcameras->getRvec(camID), m_pcameras->getTvec(camID), m_pcameras->getK(camID), m_pcameras->getDistCoeffs(camID), seam2d_defisheye);

				vector<Point> seam2d_fisheye;
				for (uint j = 0; j < seam2d_defisheye.size(); j++)
				{
					if ((0 < seam2d_defisheye[j].x) && (seam2d_defisheye[j].x < m_pcameras->m_xmaps[camID].cols - 1) &&
						(0 < seam2d_defisheye[j].y) && (seam2d_defisheye[j].y < m_pcameras->m_xmaps[camID].rows - 1))
					{
						seam2d_fisheye.push_back(Point2f(m_pcameras->m_xmaps[camID].at<float>(seam2d_defisheye[j]), m_pcameras->m_ymaps[camID].at<float>(seam2d_defisheye[j])));
					}
					else noop;
				}

				if (0 < seam2d_fisheye.size())  // to make a closed loop
					seam2d_fisheye.push_back(seam2d_fisheye[0]);
				else noop;

				////// save mask seam data
				ofstream outC_seam;
				string file_path_seam = string(_ARRAYS_PATH_) + "/seam" + to_string(camID + 1);
				outC_seam.open(file_path_seam.c_str(), std::ofstream::out | std::ofstream::trunc);

				for (int i = 0; i < (int)seam2d_fisheye.size(); i++)
				{
					outC_seam << seam2d_fisheye[i].x << " " << seam2d_fisheye[i].y << endl;
				}
				outC_seam.close();


				// Create masks which are limited to seams. (base mask)
				Mat mask(m_pcameras->m_xmaps[camID].rows, m_pcameras->m_xmaps[camID].cols, CV_8UC1, Scalar(0));
				m_masks.push_back(mask);

				fillConvexPoly(m_masks[camID], seam2d_fisheye, Scalar(255)); // Draws a filled convex polygon using all seam points

#if (0)
				imshow("testing", m_masks[camID]);
				waitKey(0);
#endif

				smothing_angle = min(0.50f, max(0.00f, smothing_angle));

				// Define left edge of smoothing
				double angle = atan(m_left_seam_lines[camID].line.alpha);
				vector<Point3f> left_points;
				left_points.push_back(Point3f(m_left_seam_lines[camID].ip1.x, m_left_seam_lines[camID].ip1.y, 0.0f));
				//left_points.push_back(Point3f(m_left_seam_lines[camID].ip2.x, (float)(sqrt(pow(circle_radius, 2) - pow(m_left_seam_lines[camID].ip2.x, 2))), 0.0f)); // x^2 + y^2 = r^2, (x,y) on the circle tracjectory
				left_points.push_back(Point3f(m_left_seam_lines[camID].ip2.x, m_left_seam_lines[camID].ip2.y, 0.0f)); // x^2 + y^2 = r^2, (x,y) on the circle tracjectory
				left_points.push_back(seam_candidators[3]); // p4
				smoothMaskEdge(m_masks[camID], Vec2b(0, 255), Vec2d(angle - smothing_angle, angle + smothing_angle), 0.0039, left_points, camID); //original

#if (0)
				imshow("testing", m_masks[camID]);
				waitKey(0);
#endif

				// Define right edge of smoothing
				angle = atan(m_right_seam_lines[camID].line.alpha);
				vector<Point3f> right_points;
				right_points.push_back(Point3f(m_right_seam_lines[camID].ip1.x, m_right_seam_lines[camID].ip1.y, 0));
				//right_points.push_back(Point3f(m_right_seam_lines[camID].ip2.x, (float)(sqrt(pow(circle_radius, 2) - pow(m_right_seam_lines[camID].ip2.x, 2))), 0));
				right_points.push_back(Point3f(m_right_seam_lines[camID].ip2.x, m_right_seam_lines[camID].ip2.y, 0.0f));
				right_points.push_back(seam_candidators[4]); // p5
				smoothMaskEdge(m_masks[camID], Vec2b(255, 0), Vec2d(angle - smothing_angle, angle + smothing_angle), 0.0039, right_points, camID);//original

#if (0)
				imshow("testing", m_masks[camID]);
				waitKey(0);
#endif

			}
			catch (exception& e)
			{
				throw runtime_error(str_camID + delimiter(string(e.what())));
			}
		}

		logger.record_message(logger.svm_inform("--------", "mask creation successful").what());

	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C1206101", __FUNCTION__ + delimiter(string(e.what())));
	}
}


void sanCalibMask::saveMasks()
{
	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)
	{
		string str_camID = "(camID[" + to_string(camID) + "])";
		try
		{
			string full_file_path = string(_MASK_IMAGES_PATH_) + string("/mask") + to_string(camID) + string(".jpg");
			sanFromFile::write_image(full_file_path, m_masks[camID]);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1206201", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
}



/**************************************************************************************************************
 *
 * @brief  			Smooth mask
 *
 * @param  	in/out	Mat &img - mask
 * 			in		Vec2b colors - 	range of colors [left edge color, right edge color]
* 									For left mask edge the color range will be [0, 255] - from black to white.
* 									And for right mask edge the color range will be [255, 0] - from white to black.
 * 			in		Vec2d angles - 	smoothing will be apply in these angles in the left and right direction from mask border.
 * 			in		double angle_step - step of color gradient
 * 			in		vector<Point3f> edge_points - 	vector of template points. It must include 3 points:
 * 													1. The first point of seam (intersection of bottom grid borders, z = 0)
 * 													2. The intersection point of seam line and base circle (z = 0)
 * 													3. The 3rd point of seam points (with maximum value of z coordinate)
 * 			in		sanCamera* camera  - sanCamera object
 *
 * @return 			-
 *
 * @remarks 		The function smoothes the mask left and right edges to obtain a seamless blending of frames. The angle of
 * 					smoothing (2 * SMOTHING_ANGLE) defines the area in which mask edge will be smooth. The original seam divides
 * 					the smoothing angle into two angles with equal measures (angle bisector). The angle based smoothing is applied
 * 					only for flat base. The seam at the bowl side is smoothed with constant width of smoothing.
 *
 **************************************************************************************************************/
void sanCalibMask::smoothMaskEdge(Mat &img, Vec2b colors, Vec2d angles, double angle_step, vector<Point3f> edge_points, int camID)
{
	double color = colors[0];
	double color_step = (double)(colors[1] - colors[0]) * angle_step / (angles[1] - angles[0]);
	double radius = sqrt(pow(edge_points[1].x, 2) + pow(edge_points[1].y, 2));
	double x_start, x_end, angle_cos;

	for(double angle = angles[0]; angle < angles[1]; angle += angle_step)
	{
		double alpha = tan(angle);
		double beta = edge_points[0].y - alpha * edge_points[0].x; // y = ax + b

		if(alpha > 0)
		{
			x_start = (- alpha * beta - sqrt(pow(radius, 2) * (1 + pow(alpha, 2)) - pow(beta, 2))) / (1 + pow(alpha, 2));
			x_end = edge_points[0].x;
			angle_cos = x_start / radius;
		}
		else
		{
			x_start = edge_points[0].x;
			x_end = (- alpha * beta + sqrt(pow(radius, 2) * (1 + pow(alpha, 2)) - pow(beta, 2))) / (1 + pow(alpha, 2));
			angle_cos = x_end / radius;
		}

		vector<Point3f> seam3d;
		vector<Point2f> seam2d, seam;

		// Horizontal part of seam
		for (double xx = x_start; xx <= x_end; xx += SEAM_STEP / 10)
			seam3d.push_back(Point3f((float)xx, (float)(xx * alpha + beta), 0.0f));

		// Vertical part of seam
		for(double zz = 0; zz < abs(edge_points[2].z); zz += SEAM_STEP / 10)
		{
			double new_x = (radius + sqrt(zz)) * angle_cos;
			double new_y = - new_x * tan(acos(angle_cos));
			seam3d.push_back(Point3f((float)new_x, (float)new_y, (float)(-zz)));
		}

		sanProject::projectPoints(seam3d, m_pcameras->getRvec(camID), m_pcameras->getTvec(camID), m_pcameras->getK(camID), m_pcameras->getDistCoeffs(camID), seam2d);

		int i = 0;
		for(uint j = 0; j < seam2d.size(); j++)
		{
			if ((seam2d[j].x < m_pcameras->m_xmaps[camID].cols - 1) && (seam2d[j].y < m_pcameras->m_xmaps[camID].rows - 1) && (seam2d[j].x > 0) && (seam2d[j].y > 0))
			{
				seam.push_back(Point2f(m_pcameras->m_xmaps[camID].at<float>(seam2d[j]), m_pcameras->m_ymaps[camID].at<float>(seam2d[j])));
				circle(img, seam[i], 1, color, 1);
				i++;
			}
			else
				noop;
		}
		color += color_step;
	}
}


/**************************************************************************************************************
 *
 * @brief  			Get intersection of two polygons
 *
 * @param  	in		vector<Point3f> &polygon1 - first convex polygon points. The polygon is described with 8 points:
 * 					1st point - left bottom vertex, 4th point - left top vertex, 5th point - right top vertex,
 * 					8th point - right bottom point. The polygon must be convex.
 * 							 4 ____ 5
 *		 					3 /    \ 6
 * 							 |      |
 * 							2 \____/ 7
 *		 					 1      8
 * 			in		vector<Point3f> &polygon2 - second convex polygon points. The polygon is described with 8 points:
 * 					1st point - left bottom vertex, 4th point - left top vertex, 5th point - right top vertex,
 * 					8th point - right bottom point. The polygon must be convex.
 * 			out		Seam &seam - polygons intersection (2 points + line that goes through them)
 * 			in		int rotation - direction of rotation for the second polygon. If rotation < 0, then the second polygon
 * 					is rotated on 90 degrees angle to the left. Otherwise it is rotated on 90 degrees angle to the right.
 *
 * @return 			-
 *
 * @remarks 		The function search for two convex polygons intersection. The first intersection point is searched
 * 					between polygons bottom sides 1-2, 1-8, 7-8. And the second intersection point is searched between
 * 					top sides 4-5. The polygon intersection is calculated after second polygon points have been rotated
 * 					according to the rotation value.
 *
 **************************************************************************************************************/
void sanCalibMask::getSeamLine(vector<Point3f>& curr_polygon, vector<Point3f>& polygon2, int left_or_right /* left: < 0, right: else*/, SEAM_LINE& seam_line /*[out]*/)
{
	try
	{
		bool intersection = false;

		// Search for the first intersection point of input polygons
		int pnt_1 = (int)(curr_polygon.size() - 2); // 7th seam candidator point

		while ((pnt_1 < (int)curr_polygon.size()) && (!intersection)) // pnt_1(index) can be only 6 and 7 (7th and 8th polygon points)
		{
			int pnt_2 = 1; // 2nd seam candidator point
			while ((pnt_2 >= 0) && (!intersection)) // pnt_2 can be only 1 and 0 (1st and 2nd polygon points)
			{
				Point2f p1_1, p1_2, p2_1, p2_2;
				if (left_or_right == LEFT_SIDE)  // representation: the current: p, the previous: p', the next: p'',  point index: (p1, p2, ..., p8)
				{
					// current polygon(openCV CS 3D vertices), p2/p1
					p1_1 = Point2f(curr_polygon[pnt_2].x, curr_polygon[pnt_2].y);  //p2           
					int prev_idx = (int)PREV(pnt_2, (int)curr_polygon.size() - 1);
					p1_2 = Point2f(curr_polygon[prev_idx].x, curr_polygon[prev_idx].y);//p1

					// 2nd polygon(openCV CS 3D vertices), p8'/p1'  (Note that it made the two axes exchanged.)
					// rotate -90 degree along Z based on openCV, that is, the points are placed on the left.
					p2_1 = Point2f(polygon2[pnt_1].y, -1.0f * polygon2[pnt_1].x); //p7'
					int next_idx = (int)NEXT(pnt_1, (int)polygon2.size() - 1);
					p2_2 = Point2f(polygon2[next_idx].y, -1.0f * polygon2[next_idx].x); //p8'
				}
				else if (left_or_right == RIGHT_SIDE)// Rotate polygon2 points to the right
				{
					// current polygon(openCV CS 3D vertices), p8/p1
					p1_1 = Point2f(curr_polygon[pnt_1].x, curr_polygon[pnt_1].y); //p7
					int next_idx = (int)NEXT(pnt_1, (int)curr_polygon.size() - 1);
					p1_2 = Point2f(curr_polygon[next_idx].x, curr_polygon[next_idx].y); //p8

					// 2nd polygon(openCV CS 3D vertices), p2'' and p1'' (Note that it made the two axes exchanged.)
					// rotate +90 degree along Z based on open CV, that is, the points are placed in the right.
					p2_1 = Point2f(-1.0f * polygon2[pnt_2].y, polygon2[pnt_2].x); //p2''
					int prev_idx = (int)PREV(pnt_2, (int)polygon2.size() - 1);
					p2_2 = Point2f(-1.0f * polygon2[prev_idx].y, polygon2[prev_idx].x); //p1''
				}
				else
					noop;

				Line line1 = getAlphaBeta(p1_1, p1_2); // ex) a straight line which passes through point p2 and p1
				Line line2 = getAlphaBeta(p2_1, p2_2); // ex) a straight line which passes through point p7' and p8'
				seam_line.ip1 = getIntersection(line1, line2);// intersection point on the flat circle

				// Check if intersection point lays on the minimum size of the inside of a 4-point polygon
				if ((seam_line.ip1.x > min(p2_1.x, p2_2.x)) && (seam_line.ip1.y > min(p2_1.y, p2_2.y)) &&  // min corner point of a rectange composed of p7' and p8'
					(seam_line.ip1.x < max(p2_1.x, p2_2.x)) && (seam_line.ip1.y < max(p2_1.y, p2_2.y)) &&  // max corner point of a rectange composed of p7' and p8'
					(seam_line.ip1.x > min(p1_1.x, p1_2.x)) && (seam_line.ip1.y > min(p1_1.y, p1_2.y)) &&  // min corner point of a rectange composed of p2 and p1
					(seam_line.ip1.x < max(p1_1.x, p1_2.x)) && (seam_line.ip1.y < max(p1_1.y, p1_2.y)))    // max corner point of a rectange composed of p2 and p1
				{
					intersection = true;
					double distance2 = 0.0;
					if (left_or_right == LEFT_SIDE) // left
					{
						// max(p1^2, p8'^2)
						double p1_2_distance_sqaure = pow(curr_polygon[0].y, 2) + pow(curr_polygon[0].x, 2);
						double p2_2_distance_sqaure = pow(polygon2[polygon2.size() - 1].y, 2) + pow(polygon2[polygon2.size() - 1].x, 2);
						distance2 = max(p1_2_distance_sqaure, p2_2_distance_sqaure);
					}
					else if (left_or_right == RIGHT_SIDE) // right
					{
						// max(p8^2, p1''^2)
						double p1_2_distance_square = pow(curr_polygon[curr_polygon.size() - 1].y, 2) + pow(curr_polygon[curr_polygon.size() - 1].x, 2);
						double p2_2_distance_square = pow(polygon2[0].y, 2) + pow(polygon2[0].x, 2);
						distance2 = max(p1_2_distance_square, p2_2_distance_square);
					}
					else
						noop;

					// Check if intersection point belongs to the 1st and 2nd grid which don't cover whole polygons area
					double intersection_point_distance_square = seam_line.ip1.x * seam_line.ip1.x + seam_line.ip1.y * seam_line.ip1.y;
					if (distance2 > intersection_point_distance_square) // replace the existing intersection point as the intersection point of x^2 + y^2 = d^2 and y = alpha * x 
					{
						double alpha = seam_line.ip1.y / seam_line.ip1.x;

						if (seam_line.ip1.x < 0.0f) // intersection point of  x^2 + y^2 = d^2 and y= alpha * x (straight line through the origin (0, 0))
							seam_line.ip1.x = (float)(-1.0f * sqrt(distance2 / (1.0f + alpha * alpha)));
						else
							seam_line.ip1.x = (float)(1.0f * sqrt(distance2 / (1.0f + alpha * alpha)));

						seam_line.ip1.y = (float)(alpha * seam_line.ip1.x);
					}
					else
						noop;
				}
				pnt_2--; // 1st seam point
			}
			pnt_1++; // 8th seam candidator point
		}

		// Search for the second intersection point of input polygons
		Point2f p1_4, p1_5;
		Point2f p2_4, p2_5;
		p1_4 = Point2f(curr_polygon[3].x, curr_polygon[3].y); //p4
		p1_5 = Point2f(curr_polygon[4].x, curr_polygon[4].y); //p5

		if (left_or_right == LEFT_SIDE)
		{
			p2_4 = Point2f(polygon2[3].y, -1.0f * polygon2[3].x); //p4'
			p2_5 = Point2f(polygon2[4].y, -1.0f * polygon2[4].x); //p5'
		}
		else if (left_or_right == RIGHT_SIDE)
		{
			p2_4 = Point2f(-1.0f * polygon2[3].y, polygon2[3].x); //p4''
			p2_5 = Point2f(-1.0f * polygon2[4].y, polygon2[4].x); //p5''
		}
		else
			noop;

		// Calculate polygon sides intersection
		Line line1 = getAlphaBeta(p1_4, p1_5);
		Line line2 = getAlphaBeta(p2_4, p2_5);

		seam_line.ip2 = getIntersection(line1, line2);
		seam_line.line = getAlphaBeta(seam_line.ip1, seam_line.ip2);



		// SEAM_LINE seam_line;
		// double r = sqrt(pow(curr_polygon[1].x, 2) + pow(curr_polygon[1].y, 2.0));

		// if (left_or_right == LEFT_SIDE)
		// {
		// 	seam_line.ip1 = Point2f(curr_polygon[0].x, curr_polygon[0].y);
		// 	seam_line.line.alpha = 1.0f;
		// 	seam_line.line.beta = seam_line.ip1.y - seam_line.line.alpha * seam_line.ip1.x;
		// 	float la = (float)seam_line.line.alpha;
		// 	float lb = (float)seam_line.line.beta;

		// 	seam_line.ip2.x = (float)((-la*lb - sqrt(pow(r, 2) * (1 + pow(la, 2)) - pow(lb, 2))) / (1 + pow(la, 2)));
		// 	seam_line.ip2.y = (float)(-sqrt(pow(r, 2) - pow(seam_line.ip2.x, 2)));
		// }
		// else
		// {
		// 	seam_line.ip1 = Point2f(curr_polygon[7].x, curr_polygon[7].y);
		// 	seam_line.line.alpha = -1.0f;
		// 	seam_line.line.beta = seam_line.ip1.y - seam_line.line.alpha * seam_line.ip1.x;
		// 	float la = (float)seam_line.line.alpha;
		// 	float lb = (float)seam_line.line.beta;

		// 	seam_line.ip2.x = (float)((-la*lb + sqrt(pow(r, 2) * (1 + pow(la, 2)) - pow(lb, 2))) / (1 + pow(la, 2)));
		// 	seam_line.ip2.y = (float)(-sqrt(pow(r, 2) - pow(seam_line.ip2.x, 2)));
		// }
	}
	catch (exception& e)
	{
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}
