#include "svmCamera.hpp"
#include "svmOpt.hpp"

sanCamera::sanCamera(sanXML* pxml)
{
	m_pxml = pxml;	
}

sanCamera::~sanCamera()
{
	for (int i = 0; i < (int)m_xmaps.size(); i++)
		m_xmaps[i].release();

	for (int i = 0; i < (int)m_ymaps.size(); i++)
		m_ymaps[i].release();

	for (int i = 0; i < (int)m_axmaps.size(); i++)
		m_axmaps[i].release();

	for (int i = 0; i < (int)m_aymaps.size(); i++)
		m_aymaps[i].release();

	m_xmaps.clear();
	vector<Mat>().swap(m_xmaps);
	m_ymaps.clear();
	vector<Mat>().swap(m_ymaps);
	m_axmaps.clear();
	vector<Mat>().swap(m_axmaps);
	m_aymaps.clear();
	vector<Mat>().swap(m_aymaps);
	m_camparams.clear();
	vector<CAMPARAM>().swap(m_camparams);
}


void sanCamera::initialize()
{
	int img_width = (int)m_pxml->m_resolution.image.width;
	int img_height = (int)m_pxml->m_resolution.image.height;

	for (int camID = 0; camID < SVM_CAMERAS_NUM; camID++)  // creation of maps about all cameras
	{
		string str_camID = "(camID[" + to_string(camID) + "])";

		try
		{
			Mat xmap, ymap;
			if ((int)(sizeof(m_pxml->m_rcam) / sizeof(*m_pxml->m_rcam)) > camID)
				sanCamera::createMaps(xmap, ymap, img_width, img_height, m_pxml->m_rcam[camID]);
			else
				throw runtime_error(string("$camID out of range"));
			m_xmaps.push_back(xmap);
			m_ymaps.push_back(ymap);

			CAMPARAM cam_param;
			cam_param.K = Mat(3, 3, CV_64F, Scalar(0.0f));
			cam_param.distCoeffs = Mat(4, 1, CV_64F, Scalar(0.0f));
			cam_param.rvec = Mat(3, 1, CV_64F, Scalar(0.0f));
			cam_param.tvec = Mat(3, 1, CV_64F, Scalar(0.0f));
			m_camparams.push_back(cam_param);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1101201", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}

	for (int camID = 0; camID < ADD_CAMERAS_NUM; camID++)
	{
		string str_camID = "(additional camID[" + to_string(camID) + "])";
		try
		{
			Mat xmap, ymap;
			if ((int)(sizeof(m_pxml->m_acam) / sizeof(*m_pxml->m_acam)) > camID)
				sanCamera::createMaps(xmap, ymap, img_width, img_height, m_pxml->m_acam[camID]);
			else
				throw runtime_error(string("$camID out of range"));
			m_axmaps.push_back(xmap);
			m_aymaps.push_back(ymap);
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C1101201", __FUNCTION__ + str_camID + delimiter(string(e.what())));
		}
	}
}


void sanCamera::cam2world(RCAM_PARAMETERS& rcam, Point2f& pt_2d /* [in] */, Point3f& pt_3d /* [out] */)
{
	double invdet = 1.0 / (rcam.aff[0] - rcam.aff[1] * rcam.aff[2]);			// 1/det(A), where A = [aff[0],aff[1];aff[2],1]
	double xp = invdet * ((pt_2d.x - rcam.cx) - rcam.aff[1] * (pt_2d.y - rcam.cy));
	double yp = invdet * (-rcam.aff[2] * (pt_2d.x - rcam.cx) + rcam.aff[0] * (pt_2d.y - rcam.cy));

	double r = sqrt(xp * xp + yp * yp);											//distance [pixels] of  the point from the image center
	double zp = rcam.pol[0];
	double r_i = 1.0;

	for (int i = 1; i < POLY_COEF_MAX_NUM; i++)
	{
		r_i *= r;
		zp += r_i * rcam.pol[i];
	}

	//normalize to unit norm
	double invnorm = 1.0 / sqrt(xp * xp + yp * yp + zp * zp);

	pt_3d.x = (float)(invnorm * xp);
	pt_3d.y = (float)(invnorm * yp);
	pt_3d.z = (float)(invnorm * zp);
}

void sanCamera::world2cam(RCAM_PARAMETERS& rcam, Point3f& pt_3d /* [in] */, Point2f& pt_2d /* [out] */)
{
	double norm = sqrt(pt_3d.x * pt_3d.x + pt_3d.y * pt_3d.y);
	double theta = atan(pt_3d.z / norm);

	double t_i;
	double invnorm;
	double rho, x, y;
	int i;

	if (norm != 0.0)
	{
		invnorm = 1.0 / norm;
		rho = rcam.invpol[0];
		t_i = 1.0;

		for (i = 1; i < INV_POLY_COEF_MAX_NUM; i++)
		{
			t_i *= theta;
			rho += t_i * rcam.invpol[i];
		}

		x = pt_3d.x * invnorm * rho;
		y = pt_3d.y * invnorm * rho;

		pt_2d.x = (float)(x * rcam.aff[0] + y * rcam.aff[1] + rcam.cx);
		pt_2d.y = (float)(x * rcam.aff[2] + y + rcam.cy);
	}
	else
	{
		pt_2d.x = (float)rcam.cx;
		pt_2d.y = (float)rcam.cy;
	}
}

void sanCamera::createMaps(Mat& mapx, Mat& mapy, int img_width, int img_height, RCAM_PARAMETERS& rcam)  // Defisheye(source) --> Fisheye(destination)
{
	if ((img_width < 0) || (img_height < 0))
	{
		string msg = "$invalid argument " + string("img_width: ") + to_string(img_width) + string(", img_height: ") + to_string(img_height);
		throw invalid_argument(__FUNCTION__ + delimiter(msg));
	}
	else
	{
		mapx.create(img_height, img_width, CV_32FC1);
		mapy.create(img_height, img_width, CV_32FC1);

		Point3f pt3d;
		Point2f pt2d;

		for (int row = 0; row < img_height; row++)
		{
			for (int col = 0; col < img_width; col++)
			{
				pt3d.x = (float)(col - rcam.cx);
				pt3d.y = (float)(row - rcam.cy);
				pt3d.z = (float)(-img_width / rcam.sf);
				world2cam(rcam, pt3d, pt2d);
				mapx.at<float>(row, col) = (float)pt2d.x;
				mapy.at<float>(row, col) = (float)pt2d.y;
			}
		}
	}
}


Point2f sanCamera::get_top_of_black_convex(Mat& mask /* defisheye*/)
{
	int mask_width = (int)mask.cols;
	int mask_height = (int)mask.rows;

	// in the lower part of a defisheye image
	int extreme_x = mask_width / 2;
	int extreme_y = mask_height / 2;
	for (int y = mask_height - 1; y > (mask_height / 2); y--)
	{
		unsigned char* pointer_row = mask.ptr<unsigned char>(y);

		for (int x = mask_width/10; x < (mask_width - mask_width/10); x++)
		{
			if (pointer_row[x] < (unsigned char)255)
			{
				extreme_x = x;
				extreme_y = y;
			}
			else noop;
		}	
	}

	Point2f pt2d;
	pt2d.x = max(0.0f, min((float)extreme_x, (float)(mask_width - 1)));
	pt2d.y = max(0.0f, min((float)extreme_y-1, (float)(mask_height - 1)));

	return pt2d;
}

void sanCamera::selectColumns(const Mat& src, Mat& dst, const vector<int>& columns)
{
	if (columns.empty())
	{
		string msg = "$invalid argument %columns size: " + to_string((int)columns.size());
		throw invalid_argument(__FUNCTION__ + delimiter(msg));
	}
	else 
	{
		auto limits = minmax_element(columns.begin(), columns.end());

		if ((*limits.first >= 0) && (*limits.second < src.cols))
		{
			int cols = int(columns.size());
			dst.create(src.rows, cols, src.type());
			for (int i = 0; i < cols; ++i)
			{
				src.col(columns[i]).copyTo(dst.col(i));
			}
		}
		else noop;
	}
}


void sanCamera::calcIntrinsicParameters(int camID)
{
	try
	{
		cv::Mat K(3, 3, CV_64F, cv::Scalar(0)); // camera matrix

		K.at<double>(0, 0) = m_pxml->m_resolution.image.width / m_pxml->m_rcam[camID].sf;		K.at<double>(0, 1) = 0.0;																K.at<double>(0, 2) = m_pxml->m_rcam[camID].cx;
		K.at<double>(1, 0) = 0.0;																K.at<double>(1, 1) = m_pxml->m_resolution.image.width / m_pxml->m_rcam[camID].sf;		K.at<double>(1, 2) = m_pxml->m_rcam[camID].cy;
		K.at<double>(2, 0) = 0.0;																K.at<double>(2, 1) = 0.0;																K.at<double>(2, 2) = 1.0;

		K.copyTo(m_camparams[camID].K);
	}
	catch (exception& e)
	{	
		throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
	}
}
