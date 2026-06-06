#ifndef SVMPROJECT_HPP_
#define SVMPROJECT_HPP_

#include <cctype>
#include "svmCore.hpp"


class sanProject
{
public:
    static void projectPoints(InputArray _opoints,
                              InputArray _rvec,
                              InputArray _tvec,
                              InputArray _cameraMatrix,
                              InputArray _distCoeffs,
                              OutputArray _ipoints,
                              OutputArray _jacobian = noArray(),
                              double aspectRatio CV_DEFAULT(0.0));

    static void projectPoints2_internal(const CvMat* objectPoints, const CvMat* r_vec, const CvMat* t_vec, const CvMat* A, const CvMat* distCoeffs, CvMat* imagePoints,
                                        CvMat* dpdr CV_DEFAULT(NULL),
                                        CvMat* dpdt CV_DEFAULT(NULL),
                                        CvMat* dpdf CV_DEFAULT(NULL),
                                        CvMat* dpdc CV_DEFAULT(NULL),
                                        CvMat* dpdk CV_DEFAULT(NULL),
                                        CvMat* dpdo CV_DEFAULT(NULL),
                                        double aspectRatio CV_DEFAULT(0.0));

    static int Rodrigues2(const CvMat* src, CvMat* dst, CvMat* jacobian);

    static void SVD(CvArr* aarr, CvArr* warr, CvArr* uarr, CvArr* varr, int flags);

    static void computeTiltProjectionMatrix(double tauX,
                                            double tauY,
                                            Matx<double, 3, 3>* matTilt = 0,
                                            Matx<double, 3, 3>* dMatTiltdTauX = 0,
                                            Matx<double, 3, 3>* dMatTiltdTauY = 0,
                                            Matx<double, 3, 3>* invMatTilt = 0);
};

#endif 
