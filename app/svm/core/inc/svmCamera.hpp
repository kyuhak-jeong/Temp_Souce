#ifndef SVMCAMERA_HPP_
#define SVMCAMERA_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmProject.hpp"
#include "svmXML.hpp"

class sanCamera
{
public:
    sanXML* m_pxml;
    vector<Mat> m_xmaps;
    vector<Mat> m_ymaps;

    vector<Mat> m_axmaps;
    vector<Mat> m_aymaps;

    vector<CAMPARAM> m_camparams;
    glm::vec3 m_camera_ori;
    glm::vec3 m_camera_pos;

public:
    sanCamera(sanXML* pxml);
    ~sanCamera();

    void initialize();

    static void cam2world(RCAM_PARAMETERS& rcam, Point2f& pt_2d /* [in] */, Point3f& pt_3d /* [out] */);
    static void world2cam(RCAM_PARAMETERS& rcam, Point3f& pt_3d /* [in] */, Point2f& pt_2d /* [out] */);

    static void createMaps(Mat& mapx, Mat& mapy, int img_width, int img_height, RCAM_PARAMETERS& rcam);

    Mat getK(int camID) { Mat M; m_camparams[camID].K.copyTo(M); return M; } // Get camera matrix
    Mat getDistCoeffs(int camID) { Mat M; m_camparams[camID].distCoeffs.copyTo(M); return M; } // Get distortion coefficients
    Mat getRvec(int camID) { Mat M; m_camparams[camID].rvec.copyTo(M); return M; } // Get Rodrigues rotation vector
    Mat getTvec(int camID) { Mat M; m_camparams[camID].tvec.copyTo(M); return M; } // Get translation vector
    void setRvec(int camID, Mat& rvec) { rvec.copyTo(m_camparams[camID].rvec); }
    void setTvec(int camID, Mat& tvec) { tvec.copyTo(m_camparams[camID].tvec); }

    void calcIntrinsicParameters(int camID);

    void selectColumns(const Mat& src, Mat& dst, const vector<int>& columns);
    static Point2f get_top_of_black_convex(Mat& mask);
   
};

#endif 
