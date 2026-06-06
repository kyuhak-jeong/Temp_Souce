////////////////////////////////////////////////////////////////////////////////////////////////////////
// refer to https://docs.opencv.org/4.5.4/d9/d0c/group__calib3d.html#ga87955f4330d5c20e392b265b7f92f691
////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "svmProject.hpp"


void sanProject::projectPoints(InputArray _opoints,      // object points
                               InputArray _rvec,         // rotation vector(Rodrigues, CV coordiantes)
                               InputArray _tvec,         // translation vector(CV coordinates, Tc)
                               InputArray _cameraMatrix, // intrinsic parameters
                               InputArray _distCoeffs,   // distortion coefficients(4)
                               OutputArray _ipoints,     // image points
                               OutputArray _jacobian,    // Jacobian Matrix
                               double aspectRatio)
{
    try
    {

        Mat opoints = _opoints.getMat();
        int npoints = opoints.checkVector(3), depth = opoints.depth();
        if (npoints < 0) opoints = opoints.t();
        else noop;

        npoints = opoints.checkVector(3);

        // CV_Assert(npoints >= 0 && (depth == CV_32F || depth == CV_64F));
        if ((npoints < 0) || !((depth == CV_32F) || (depth == CV_64F)))
            throw invalid_argument(string("$invalid argument ") + string("%npoints: ") + to_string(npoints) + string(" %depth: " + to_string(depth)));
        else
        {
            if (opoints.cols == 3) opoints = opoints.reshape(3);
            else noop;

            CvMat dpdrot, dpdt, dpdf, dpdc, dpddist;
            CvMat* pdpdrot = 0, * pdpdt = 0, * pdpdf = 0, * pdpdc = 0, * pdpddist = 0;

            //CV_Assert(_ipoints.needed());
            if (!_ipoints.needed())
                throw runtime_error(string("$_ipoints.needed(): ") + string((_ipoints.needed()) ? "true" : "false"));
            else
            {
                _ipoints.create(npoints, 1, CV_MAKETYPE(depth, 2), -1, true);
                Mat imagePoints = _ipoints.getMat();
                CvMat c_imagePoints = cvMat(imagePoints);
                CvMat c_objectPoints = cvMat(opoints);
                Mat cameraMatrix = _cameraMatrix.getMat();

                Mat rvec = _rvec.getMat(), tvec = _tvec.getMat();
                CvMat c_cameraMatrix = cvMat(cameraMatrix);
                CvMat c_rvec = cvMat(rvec), c_tvec = cvMat(tvec);

                double dc0buf[5] = { 0 };
                Mat dc0(5, 1, CV_64F, dc0buf);
                Mat distCoeffs = _distCoeffs.getMat();
                if (distCoeffs.empty()) distCoeffs = dc0;
                else noop;

                CvMat c_distCoeffs = cvMat(distCoeffs);
                int ndistCoeffs = distCoeffs.rows + distCoeffs.cols - 1;

                Mat jacobian;
                if (_jacobian.needed())
                {
                    // image point p = (u, v) = A * G * D * N * E * o
                    // p: image or texel coordianes
                    // A: camera matrix(fx, fy, cx, cy)  , (u, v)
                    // G: 3 x 3 transformation matrix of angular parameters (round_x, round_y), (xg, yg)
                    // D: two distortion functions with coefficients k1,k2,p1,p2,k3,k4,k5,k6,s1,s2,s3,s4,round_x,round_y   (xd, yd)
                    // N: projection division (x', y')
                    // E: extrinsic parameter (x, y, z)
                    // o: 3D vertex coordinates(X, Y, Z)

                    _jacobian.create(npoints * 2, 3 + 3 + 2 + 2 + ndistCoeffs, CV_64F);
                    jacobian = _jacobian.getMat();
                    pdpdrot = &(dpdrot = cvMat(jacobian.colRange(0, 3))); // round_(image point) / round_(Rodrigues vector)
                    pdpdt = &(dpdt = cvMat(jacobian.colRange(3, 6)));     // round_(image point) / round_(translation vector)
                    pdpdf = &(dpdf = cvMat(jacobian.colRange(6, 8)));     // round_(image point) / round_(focal length vector)
                    pdpdc = &(dpdc = cvMat(jacobian.colRange(8, 10)));    // round_(image point) / round_(optical center)
                    pdpddist = &(dpddist = cvMat(jacobian.colRange(10, 10 + ndistCoeffs))); // round_(image point) / round_(distortion coefficients)
                }
                else noop;

                projectPoints2_internal(&c_objectPoints, &c_rvec, &c_tvec, &c_cameraMatrix, &c_distCoeffs,
                    &c_imagePoints, pdpdrot, pdpdt, pdpdf, pdpdc, pdpddist, NULL, aspectRatio);
            }
        }
    }
    catch (exception& e)
    {
        throw runtime_error(__FUNCTION__ + delimiter(string(e.what())));
    }
}


double prev_mx = 0.0;
double prev_my = 0.0;

void sanProject::projectPoints2_internal(const CvMat* objectPoints,     // [in] 3d points (vertices)
                                       const CvMat* r_vec,                  // [in] 3x1 Rodrigues vector or 3x3 rotation matrix (extrinsic parameter's direction):
                                       const CvMat* t_vec,                  // [in] translation (Tc): (tc_x, tc_y, tc_z)
                                       const CvMat* K,                      // [in] 3x3 cameara matrix(intrisic parameter): (fx, fy, cx, cy)
                                       const CvMat* distCoeffs,             // [in] 14_ _Ķ_(k1, k2, p1, p2, k3, k4, k5, k6, s1 ,s2, s3, s5, round_x, round_y)    ==>   total 24 parameters
                                       CvMat* imagePoints,                  // [out] 2D image points(texels)
                                       CvMat* dpdr,                         // [out] with respect to a Rodrigues vector (vx, vy, vz)^T
                                       CvMat* dpdt,                         // [out] with respect to a translation vector (tc_x, tc_y, tc_z)^T
                                       CvMat* dpdf,                         // [out] with respect to focal length vector (fx, fy)^T
                                       CvMat* dpdc,                         // [out] with respect to a optical center (cx, cy) ^T
                                       CvMat* dpdk,                         // [out] with respect to distortion coefficients (k1, k2, p1, p2, k3, k4, k5, k6, s1, s2, s3, s5, tau_x, tau_y)
                                       CvMat* dpdo,                         // [out] with respect to object point(vertex) (X, Y, Z)^T
                                       double aspectRatio)
 {
    Ptr<CvMat> matM; //objectPoints
    Ptr<CvMat> _m;   //imagePoints

    Ptr<CvMat> _dpdr;
    Ptr<CvMat> _dpdt; //t: translation vector [tc_x tc_y tc_z]
    Ptr<CvMat> _dpdc; //c: optical center vector[cx cy]
    Ptr<CvMat> _dpdf; //f: focal length [fx fy]
    Ptr<CvMat> _dpdk; //14 parameters(k1 ~ tauY)
    Ptr<CvMat> _dpdo; // vertex coordinates (X, Y, Z)

    int i, j;
    int count; // the number of points
    int calc_derivatives;

    const CvPoint3D64f* M; // 3D vertex
    CvPoint2D64f* m; // 2D texel

    double r[3];     // 3x1 Rodrigues vector
    double R[9];     // 3x3 rotation matrix
    double dRdr[27]; // Rodrigues vector, Jacobian
    double t[3];     // translation vector(Tc)
    double a[9];     // camera matrix(intrinsic parameter)
    double k[14] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0 }; // (k1,k2,p1,p2,k3,k4,k5,k6,s1,s2,s3,s4,��x,��y)
    double fx, fy;  // focal length
    double cx, cy;  // optical center

    Matx33d matTilt = Matx33d::eye();  //matTilt = 3x3 matProjZ * 3x3 matRotXY

    // ��(matTilt)(0,0)/�ӥ�x
    Matx33d dMatTiltdTauX(0, 0, 0,
                          0, 0, 0,
                          0,-1, 0);

    // ��(matTilt)(0,0)/�ӥ�y
    Matx33d dMatTiltdTauY(0, 0, 0,
                          0, 0, 0,
                          1, 0, 0);

    CvMat _r;
    CvMat _t;
    CvMat _a = cvMat(3, 3, CV_64F, a);
    CvMat _k;
    CvMat matR = cvMat(3, 3, CV_64F, R);
    CvMat _dRdr = cvMat(3, 9, CV_64F, dRdr); //Jacobian of Rotation matrix with respect to Rodrigess vector

    double* dpdr_p = 0; // pointer variable of dpdr
    double* dpdt_p = 0;
    double* dpdk_p = 0;
    double* dpdf_p = 0;
    double* dpdc_p = 0;
    double* dpdo_p = 0;
    int dpdr_step = 0;  // the number of columns of dpdr
    int dpdt_step = 0;
    int dpdk_step = 0;
    int dpdf_step = 0;
    int dpdc_step = 0;
    int dpdo_step = 0;
    bool fixedAspectRatio = aspectRatio > FLT_EPSILON;


    // image point p = (u, v) = A * G * D * N * E * o
    // p: image or texel coordianes(u,v)
    // A: camera matrix(fx, fy, cx, cy)                                                     [x3/z3  y3/z3  1]       ->  [u, v, 1]
    // G: 3 x 3 transformation matrix of angular parameters (��x, ��y),                     [x2     y2     1]       ->  [x3 y3 z3]     ->    [x3/z3  y3/z3  1]
    // D: two distortion functions with coefficients k1,k2,p1,p2,k3,k4,k5,k6,s1,s2,s3,s4    [x1     y1]             ->  [x2 y2]
    // N: projection division                                                               [x0     y0     z0]      ->  [x1 y1  1]
    // E: extrinsic parameter , E=[R|Ts]                                                    [X      Y      Z    1]  ->  [x0 y0 z0]
    // o: 3D vertex coordinates                                                             [X      Y      Z]       ->  [X Y Z  1]
    // 3D vertex point: (X, Y, Z)

    // [u v 1] = A(x3/z3, y3/z3, 1) * G(x2, y2, 1) * D(x1, y1) * N(x0, y0, z0) * E(X, Y, Z, 1) * o(X, Y, Z)

    if (!CV_IS_MAT(objectPoints) || !CV_IS_MAT(r_vec) || !CV_IS_MAT(t_vec) || !CV_IS_MAT(K) ||  /*!CV_IS_MAT(distCoeffs) ||*/ !CV_IS_MAT(imagePoints))
    {
        string msg = string("$One of required arguments is not a valid matrix");
        throw invalid_argument(__FUNCTION__ + delimiter(msg));
    }
    else noop;
        
    int total = objectPoints->rows * objectPoints->cols * CV_MAT_CN(objectPoints->type);

    if (total % 3 != 0) //we have stopped support of homogeneous coordinates because it cause ambiguity in interpretation of the input data
    {
        string msg = string("$1: Homogeneous coordinates are not supported");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }
    else noop;

    count = total / 3; // the number of vetices

    if( CV_IS_CONT_MAT(objectPoints->type) &&  (CV_MAT_DEPTH(objectPoints->type) == CV_32F || CV_MAT_DEPTH(objectPoints->type) == CV_64F)&&
        ((objectPoints->rows == 1 && CV_MAT_CN(objectPoints->type) == 3) || (objectPoints->rows == count && CV_MAT_CN(objectPoints->type)*objectPoints->cols == 3) ||
         (objectPoints->rows == 3 && CV_MAT_CN(objectPoints->type) == 1 && objectPoints->cols == count)))
    {
        matM.reset(cvCreateMat( objectPoints->rows, objectPoints->cols, CV_MAKETYPE(CV_64F,CV_MAT_CN(objectPoints->type)) ));
        cvConvert(objectPoints, matM);
    }
    else
    {
        string msg = string("$2: Homogeneous coordinates are not supported");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }

    if( CV_IS_CONT_MAT(imagePoints->type) && (CV_MAT_DEPTH(imagePoints->type) == CV_32F || CV_MAT_DEPTH(imagePoints->type) == CV_64F) &&
        ((imagePoints->rows == 1 && CV_MAT_CN(imagePoints->type) == 2) ||
         (imagePoints->rows == count && CV_MAT_CN(imagePoints->type)*imagePoints->cols == 2) ||
         (imagePoints->rows == 2 && CV_MAT_CN(imagePoints->type) == 1 && imagePoints->cols == count)))
    {
        _m.reset(cvCreateMat( imagePoints->rows, imagePoints->cols, CV_MAKETYPE(CV_64F,CV_MAT_CN(imagePoints->type)) ));
        cvConvert(imagePoints, _m);
    }
    else
    {
        string msg = string("$3: Homogeneous coordinates are not supported");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }


    M = (CvPoint3D64f*)matM->data.db;
    m = (CvPoint2D64f*)_m->data.db;

    if ((CV_MAT_DEPTH(r_vec->type) != CV_64F && CV_MAT_DEPTH(r_vec->type) != CV_32F) || (((r_vec->rows != 1 && r_vec->cols != 1) || r_vec->rows * r_vec->cols * CV_MAT_CN(r_vec->type) != 3) &&
        ((r_vec->rows != 3 && r_vec->cols != 3) || CV_MAT_CN(r_vec->type) != 1)))
    {
        string msg = string("$Rotation must be represented by 1x3 or 3x1 floating-point rotation vector, or 3x3 rotation matrix");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }
    else noop;


    if( r_vec->rows == 3 && r_vec->cols == 3 ) // 3x3 rotation matrix
    {
        _r = cvMat( 3, 1, CV_64FC1, r );
        Rodrigues2( r_vec, &_r , NULL );
        Rodrigues2( &_r, &matR, &_dRdr );
        cvCopy( r_vec, &matR );
    }
    else  // 3x1 Rodrigues vector
    {
        _r = cvMat( r_vec->rows, r_vec->cols, CV_MAKETYPE(CV_64F,CV_MAT_CN(r_vec->type)), r );
        cvConvert( r_vec, &_r );
        Rodrigues2( &_r, &matR, &_dRdr );
    }

    if ((CV_MAT_DEPTH(t_vec->type) != CV_64F && CV_MAT_DEPTH(t_vec->type) != CV_32F) || (t_vec->rows != 1 && t_vec->cols != 1) || t_vec->rows * t_vec->cols * CV_MAT_CN(t_vec->type) != 3)
    {
        string msg = string("$Translation vector must be 1x3 or 3x1 floating-point vector");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }
    else noop;

    _t = cvMat( t_vec->rows, t_vec->cols, CV_MAKETYPE(CV_64F,CV_MAT_CN(t_vec->type)), t );
    cvConvert( t_vec, &_t );

    if ((CV_MAT_TYPE(K->type) != CV_64FC1 && CV_MAT_TYPE(K->type) != CV_32FC1) || K->rows != 3 || K->cols != 3)
    {
        string msg = string("$Intrinsic parameters must be 3x3 floating-point matrix");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }
    else noop;


    cvConvert( K, &_a );
    fx = a[0]; fy = a[4]; // fx = f * mu,  fy= f * mv (mu: inverse horizon cell size, mv: inverse vetical cell size)
    cx = a[2]; cy = a[5]; // optical center

    if( fixedAspectRatio )
        fx = fy * aspectRatio;

    if( distCoeffs ) // distortion coefficients(14)
    {
        if (!CV_IS_MAT(distCoeffs) || (CV_MAT_DEPTH(distCoeffs->type) != CV_64F &&
            CV_MAT_DEPTH(distCoeffs->type) != CV_32F) || (distCoeffs->rows != 1 && distCoeffs->cols != 1) ||
            (distCoeffs->rows * distCoeffs->cols * CV_MAT_CN(distCoeffs->type) != 4 &&
             distCoeffs->rows * distCoeffs->cols * CV_MAT_CN(distCoeffs->type) != 5 &&
             distCoeffs->rows * distCoeffs->cols * CV_MAT_CN(distCoeffs->type) != 8 &&
             distCoeffs->rows * distCoeffs->cols * CV_MAT_CN(distCoeffs->type) != 12 &&
             distCoeffs->rows * distCoeffs->cols * CV_MAT_CN(distCoeffs->type) != 14))
        {
            string msg = string("$Distortion coefficents error");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        _k = cvMat( distCoeffs->rows, distCoeffs->cols, CV_MAKETYPE(CV_64F,CV_MAT_CN(distCoeffs->type)), k );
        cvConvert( distCoeffs, &_k );
    }

    if( dpdr ) // rotation vector or rotation matrix
    {
        if (!CV_IS_MAT(dpdr) || (CV_MAT_TYPE(dpdr->type) != CV_32FC1 && CV_MAT_TYPE(dpdr->type) != CV_64FC1) || dpdr->rows != count * 2 || dpdr->cols != 3)
        {
            string msg = string("$dp/drot must be 2Nx3 floating-point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if( CV_MAT_TYPE(dpdr->type) == CV_64FC1 )
        {
            _dpdr.reset(cvCloneMat(dpdr));
        }
        else
            _dpdr.reset(cvCreateMat( 2*count, 3, CV_64FC1 ));

        dpdr_p = _dpdr->data.db;
        dpdr_step = _dpdr->step/sizeof(dpdr_p[0]); // 2N, dpdr_step
    }

    if( dpdt ) // translation
    {
        if (!CV_IS_MAT(dpdt) || (CV_MAT_TYPE(dpdt->type) != CV_32FC1 && CV_MAT_TYPE(dpdt->type) != CV_64FC1) || dpdt->rows != count * 2 || dpdt->cols != 3)
        {
            string msg = string("$dp/dT must be 2Nx3 floating-point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if( CV_MAT_TYPE(dpdt->type) == CV_64FC1 )
        {
            _dpdt.reset(cvCloneMat(dpdt));
        }
        else
            _dpdt.reset(cvCreateMat( 2*count, 3, CV_64FC1 ));

        dpdt_p = _dpdt->data.db;
        dpdt_step = _dpdt->step/sizeof(dpdt_p[0]);
    }

    if( dpdf ) // focal length (fx, fy)
    {
        if (!CV_IS_MAT(dpdf) || (CV_MAT_TYPE(dpdf->type) != CV_32FC1 && CV_MAT_TYPE(dpdf->type) != CV_64FC1) || dpdf->rows != count * 2 || dpdf->cols != 2)
        {
            string msg = string("$dp/df must be 2Nx2 floating-point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;


        if( CV_MAT_TYPE(dpdf->type) == CV_64FC1 )
        {
            _dpdf.reset(cvCloneMat(dpdf));
        }
        else
            _dpdf.reset(cvCreateMat( 2*count, 2, CV_64FC1 ));

        dpdf_p = _dpdf->data.db;
        dpdf_step = _dpdf->step/sizeof(dpdf_p[0]);
    }

    if( dpdc ) // optical center (cx, cy)
    {
        if (!CV_IS_MAT(dpdc) || (CV_MAT_TYPE(dpdc->type) != CV_32FC1 && CV_MAT_TYPE(dpdc->type) != CV_64FC1) || dpdc->rows != count * 2 || dpdc->cols != 2)
        {
            string msg = string("$dp/dc must be 2Nx2 floating-point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if( CV_MAT_TYPE(dpdc->type) == CV_64FC1 )
            _dpdc.reset(cvCloneMat(dpdc));
        else
            _dpdc.reset(cvCreateMat( 2*count, 2, CV_64FC1 ));

        dpdc_p = _dpdc->data.db;
        dpdc_step = _dpdc->step/sizeof(dpdc_p[0]);
    }

    if( dpdk ) // distortion coefficients
    {
        if (!CV_IS_MAT(dpdk) || (CV_MAT_TYPE(dpdk->type) != CV_32FC1 && CV_MAT_TYPE(dpdk->type) != CV_64FC1) || dpdk->rows != count * 2 || (dpdk->cols != 14 && dpdk->cols != 12 && dpdk->cols != 8 && dpdk->cols != 5 && dpdk->cols != 4 && dpdk->cols != 2))
        {
            string msg = string("$dp/df must be 2Nx14, 2Nx12, 2Nx8, 2Nx5, 2Nx4 or 2Nx2 floating-point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if (!distCoeffs)
        {
            string msg = string("$distCoeffs is NULL while dpdk is not");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if( CV_MAT_TYPE(dpdk->type) == CV_64FC1 )
            _dpdk.reset(cvCloneMat(dpdk));
        else
            _dpdk.reset(cvCreateMat( dpdk->rows, dpdk->cols, CV_64FC1 ));

        dpdk_p = _dpdk->data.db;
        dpdk_step = _dpdk->step/sizeof(dpdk_p[0]);
    }

    if( dpdo ) // (tex_x, tex_y) <- (wx, wy, wz) ??????
    {
        if (!CV_IS_MAT(dpdo) || (CV_MAT_TYPE(dpdo->type) != CV_32FC1 && CV_MAT_TYPE(dpdo->type) != CV_64FC1) || dpdo->rows != count * 2 || dpdo->cols != count * 3)
        {
            string msg = string("$dp/do must be 2Nx3N floating-point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if( CV_MAT_TYPE( dpdo->type ) == CV_64FC1 )
        {
            _dpdo.reset( cvCloneMat( dpdo ) );
        }
        else
            _dpdo.reset( cvCreateMat( 2 * count, 3 * count, CV_64FC1 ) );

        cvZero(_dpdo);
        dpdo_p = _dpdo->data.db;
        dpdo_step = _dpdo->step / sizeof( dpdo_p[0] );
    }

    calc_derivatives = dpdr || dpdt || dpdf || dpdc || dpdk || dpdo;

   // int temp_count = 0; // for debugging

    for(i = 0; i < count; i++ )
    {
        double X = M[i].x;
        double Y = M[i].y;
        double Z = M[i].z;  //vertex in 3d world space
        double x = R[0]*X + R[1]*Y + R[2]*Z + t[0]; // extrinsic parameter
        double y = R[3]*X + R[4]*Y + R[5]*Z + t[1];
        double z = R[6]*X + R[7]*Y + R[8]*Z + t[2];

        double r2, r4, r6, a1, a2, a3, cdist, icdist2; // for distortion functions
        double xd, yd, xd0, yd0, invProj;

        if (z <= DBL_EPSILON) // in case of upside down,  modified by DGH, 2023/03/26
            z = DBL_EPSILON;
        else noop;
 
        Vec3d vecTilt;  // for angular paramters
        Vec3d dVecTilt;
        Matx22d dMatTilt;
        Vec2d dXdYd;
        double z0 = z;
        z = z ? 1.0 / z : 1.0;
        x *= z;
        y *= z;


        ////////////////////////////////////mapping vertices to image points//////////////////////////////////
        // refer to : https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html#ga87955f4330d5c20e392b265b7f92f691
        // distortion function generation
        r2 = x * x + y * y;  // square of a distance from the optical center(0, 0) to the current point (x,y)
        r4 = r2 * r2;
        r6 = r4 * r2;
        a1 = 2 * x * y;
        a2 = r2 + 2 * x * x;
        a3 = r2 + 2 * y * y;
        cdist = 1 + k[0] * r2 + k[1] * r4 + k[4] * r6;          // k[0]=k1, k[1]=k2, k[2]=p1, k[3]=p2, k[4]=k3
        icdist2 = 1. / (1 + k[5] * r2 + k[6] * r4 + k[7] * r6); // k[5]=k4, k[6]=k5, k[7]=k6

        // distorted horizontal and vertical distances(xd0, yd0) from the center or the origin in the normalized image plane
        xd0 = x * cdist * icdist2 + k[2] * a1 + k[3] * a2 + k[8] * r2 + k[9] * r4;  // k[8] = s1, k[9] = s2
        yd0 = y * cdist * icdist2 + k[2] * a3 + k[3] * a1 + k[10] * r2 + k[11] * r4; // k[10]= s3, k[11]= s4

#if (0)
        double tauX = k[12], tauY = k[13];
        computeTiltProjectionMatrix(tauX, tauY, &matTilt, &dMatTiltdTauX, &dMatTiltdTauY); // additional distortion by projecting onto a tilt plane,  angular parameters
#endif
        vecTilt = matTilt * Vec3d(xd0, yd0, 1);
        invProj = vecTilt(2) ? 1. / vecTilt(2) : 1;
        xd = invProj * vecTilt(0);
        yd = invProj * vecTilt(1);


        //camera matrix
        m[i].x = xd * fx + cx;// fx: focal length
        m[i].y = yd * fy + cy;
 
        //if(3 < count)
        //    printf("=> (%8.3f %8.3f) (%8.1f %8.1f)\n", x, y, m[i].x, m[i].y);


        if( calc_derivatives )
        {
            // [u, v] fish-eye image coordinates, (xd, yd): coordinates of distorted image in normalized image plane
            // u = fx*(x'''/z''') + cx
            // v = fy*(y'''/z''') + cy
            // vecTilt = [x''',y''', z''']^T = matTilt(3x3) * [xd, yd, 1]^T,  matTilt = matProjZ(3x3) * matRotXY(3x3)
            // xd = distortion_func_x(x'/z'; k)
            // yd = distortion_func_y(y'/z'; k)
            // [x' y' z']^T = [R(v): Ts][X Y Z 1]^T
            // R: rotation matrix, v: Rodrigess vector, Ts: Translation vector, (X, Y, Z): vertex coordinates in 3d virtual space

            // F(u, v)=[u, v]^T = Transform(A, G, D, N, E) * [X Y Z 1]^T with 24 parameters
            // A: camera matrix, G: angular parameter matrix, D: distortion function, N: projection division, E: extrinsic matrix,
            // E = [R|Tc], R: Rotation matrix of a Rodrigues vector, and Tc: translation vector

            // round_F(u, v)/round_(param) = [��u/round_(param), round_v/round_(param)] = round_(Transform)/round_(param)


            if (dpdc_p) // image point = (u, v),  ��u/��cx, ��u/��cy, ��v/��cx, ��v/��cy
            {
                dpdc_p[0] = 1; // ��u / ��cx
                dpdc_p[1] = 0; // ��u / ��cy
                dpdc_p[dpdc_step] = 0; // ��v/��cx
                dpdc_p[dpdc_step + 1] = 1; // ��v/��cy

                dpdc_p += dpdc_step * 2;
            }
            else noop;

            if (dpdf_p) // image point = (u, v),  ��u/��fx, ��u/��fy, ��v/��fx, ��v/��fy
            {
                if (fixedAspectRatio)
                {
                    dpdf_p[0] = 0;
                    dpdf_p[1] = xd * aspectRatio;
                    dpdf_p[dpdf_step] = 0;
                    dpdf_p[dpdf_step + 1] = yd;
                }
                else
                {
                    dpdf_p[0] = xd;  // ��u/��fx
                    dpdf_p[1] = 0;   // ��u/��fy
                    dpdf_p[dpdf_step] = 0; //��v/��fx
                    dpdf_p[dpdf_step + 1] = yd; //��v/��fy
                }

                dpdf_p += dpdf_step * 2;
            }
            else noop;


            // matTilt cv::detail::computeTiltProjectionMatrix(tauX, tauY, &matTilt);
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 2; ++col)
                    dMatTilt(row,col) = matTilt(row,col)*vecTilt(2) - matTilt(2,col)*vecTilt(row);

            double invProjSquare = (invProj*invProj);  // invProj = 1.0 / vecTilt[2]
            dMatTilt *= invProjSquare;


            if (dpdk_p) // ��(image point)/��k,   x and y mean x' and y' repectively.
            {
                // with repsect to k1 and k2
                dXdYd = dMatTilt * Vec2d(x * icdist2 * r2, y * icdist2 * r2);  // ��(G*distortion_fun_x(x',y'))/��k1,  ��(G*distortion_fun_y(x',y'))/��k1
                dpdk_p[0] = fx * dXdYd(0);
                dpdk_p[dpdk_step] = fy * dXdYd(1);

                dXdYd = dMatTilt * Vec2d(x * icdist2 * r4, y * icdist2 * r4); // ��(G*distortion_fun_x(x',y'))/��k2,  ��(G*distortion_fun_y(x',y'))/��k2
                dpdk_p[1] = fx * dXdYd(0);
                dpdk_p[dpdk_step + 1] = fy * dXdYd(1);

                if (_dpdk->cols > 2)
                {
                    // with respect to p1 and p2
                    dXdYd = dMatTilt * Vec2d(a1, a3); //a1 = ��(G*distortion_fun_x(x',y'))/��p1 = 2*x'*y', a3 = ��(G*distortion_fun_y(x',y'))/��p1 = r^2 + 2*y'^2
                    dpdk_p[2] = fx * dXdYd(0);
                    dpdk_p[dpdk_step + 2] = fy * dXdYd(1);

                    dXdYd = dMatTilt * Vec2d(a2, a1); //a2 = ��(G*distortion_fun_x(x',y'))/��p2 = r^2 + 2*x'^2, a1 = ��(G*distortion_fun_y(x',y'))/��p2 = 2*x'*y'
                    dpdk_p[3] = fx * dXdYd(0);
                    dpdk_p[dpdk_step + 3] = fy * dXdYd(1);

                    if (_dpdk->cols > 4)
                    {
                        // with respect to k3
                        dXdYd = dMatTilt * Vec2d(x * icdist2 * r6, y * icdist2 * r6);
                        dpdk_p[4] = fx * dXdYd(0);
                        dpdk_p[dpdk_step + 4] = fy * dXdYd(1);

                        if (_dpdk->cols > 5)
                        {
                            // with respect to k4, k5 and k6
                            dXdYd = dMatTilt * Vec2d(x * cdist * (-icdist2) * icdist2 * r2, y * cdist * (-icdist2) * icdist2 * r2);
                            dpdk_p[5] = fx * dXdYd(0);
                            dpdk_p[dpdk_step + 5] = fy * dXdYd(1);
                            dXdYd = dMatTilt * Vec2d(x * cdist * (-icdist2) * icdist2 * r4, y * cdist * (-icdist2) * icdist2 * r4);
                            dpdk_p[6] = fx * dXdYd(0);
                            dpdk_p[dpdk_step + 6] = fy * dXdYd(1);
                            dXdYd = dMatTilt * Vec2d(x * cdist * (-icdist2) * icdist2 * r6, y * cdist * (-icdist2) * icdist2 * r6);
                            dpdk_p[7] = fx * dXdYd(0);
                            dpdk_p[dpdk_step + 7] = fy * dXdYd(1);

                            if (_dpdk->cols > 8)
                            {
                                // with repsect to s1, s2, s3 and s4
                                dXdYd = dMatTilt * Vec2d(r2, 0);//s1
                                dpdk_p[8] = fx * dXdYd(0);
                                dpdk_p[dpdk_step + 8] = fy * dXdYd(1);

                                dXdYd = dMatTilt * Vec2d(r4, 0);//s1
                                dpdk_p[9] = fx * dXdYd(0);
                                dpdk_p[dpdk_step + 9] = fy * dXdYd(1);

                                dXdYd = dMatTilt * Vec2d(0, r2); //s3
                                dpdk_p[10] = fx * dXdYd(0);
                                dpdk_p[dpdk_step + 10] = fy * dXdYd(1);

                                dXdYd = dMatTilt * Vec2d(0, r4); //s4
                                dpdk_p[11] = fx * dXdYd(0);
                                dpdk_p[dpdk_step + 11] = fy * dXdYd(1);

                                if (_dpdk->cols > 12) // ��x and ��y
                                {
                                    // ///////////////////// Scheimpflug principle ////////////////////////////
                                    // matTilt = G(��x,��y) =  matProjZ(��x, ��y) * matRotXY(��x, ��y)
                                    // where matProjZ: z-axis shear and scale matrix, matRotXY: rotation matrix

                                    //                     [ M33(��x, ��y)         0          ?M13(��x, ��y) ]
                                    // matProjZ(��x,��y) = [      0         M33(��x, ��y)     ?M23(��x, ��y) ]
                                    //                     [      0              0                1      ]

                                    //                     [  cos(��y)     sin(��y)*sin(��x)   -sin(��y)*cos(��x) ]
                                    // matRotXY(��x,��y) = [     0           ?cos(��x)            sin(��x)     ]  = Ry(��y) * Rx(��x),
                                    //                     [ ?sin(��y)    -cos(��y)*sin(��x)    cos(��y)*cos(��x) ]
                                    //
                                    // where Ry and Rx are rotation matrices.

                                    // with respect to ��x
                                    dVecTilt = dMatTiltdTauX * Vec3d(xd0, yd0, 1);  // [��G(��x,��y)/�ӥ�x , ��G(��x,��y)/�ӥ�y]^T     xd0 = xd, yd0 = yd
                                    dpdk_p[12] = fx * invProjSquare * (dVecTilt(0) * vecTilt(2) - dVecTilt(2) * vecTilt(0));
                                    dpdk_p[dpdk_step + 12] = fy * invProjSquare * (dVecTilt(1) * vecTilt(2) - dVecTilt(2) * vecTilt(1));

                                    // with respect to ��y
                                    dVecTilt = dMatTiltdTauY * Vec3d(xd0, yd0, 1);
                                    dpdk_p[13] = fx * invProjSquare * (dVecTilt(0) * vecTilt(2) - dVecTilt(2) * vecTilt(0));
                                    dpdk_p[dpdk_step + 13] = fy * invProjSquare * (dVecTilt(1) * vecTilt(2) - dVecTilt(2) * vecTilt(1));
                                }
                            }
                        }
                    }
                }
                dpdk_p += dpdk_step * 2;
            }
            else noop;

            if (dpdt_p)
            {
                // vertex point = (X, Y, Z)^T
                // extrinsic + projection division
                // x = R0*X + R1*Y + R2*Z + tx     ==> extrinsic
                // y = R3*X + R4*Y + R5*Z + ty
                // z = R6*X + R7*Y + R8*Z + tz
                // x' = x/z => ��x'/��t = (��x'/��tx, ��x'/��ty, ��x'/��tz)    ==> projection division
                // y' = y/z => ��y'/��t = (��y'/��tx, ��y'/��ty, ��y'/��tz)
                double dxdt[] = { z, 0, -x * z }; // ����: ���ʿ��� z ������ �����Ͽ���.
                double dydt[] = { 0, z, -y * z };

                for (j = 0; j < 3; j++)  //distortion_fun: (x', y') -- > (xd, yd), xd = distrotion_func1(x', y'), yd = distrotion_func2(x', y')
                {
                    // ��u/��t(= ��mx/��t) = ��(distortion_fun_x(x', y'; k1,k2,p1,p2,k3,k4,k5,k6,s1,s2,s3,s4,��x,��y)) / ��(tx, ty, tz)
                    // ��v/��t(= ��my/��t) = ��(distortion_fun_y(x', y'; k1,k2,p1,p2,k3,k4,k5,k6,s1,s2,s3,s4,��x,��y)) / ��(tx, ty, tz)

                    double dr2dt = 2 * x * dxdt[j] + 2 * y * dydt[j]; // r^2 = x'^2 + y'^2 : radial distortion
                    double dcdist_dt = k[0] * dr2dt + 2 * k[1] * r2 * dr2dt + 3 * k[4] * r4 * dr2dt;  // (1 + k1*r^2 + k2*r^4 + k3*r6)
                    double dicdist2_dt = -icdist2 * icdist2 * (k[5] * dr2dt + 2 * k[6] * r2 * dr2dt + 3 * k[7] * r4 * dr2dt); // 1.0 / (1 + k4*r^2 + k5*r^4 + k6*r6)
                    double da1dt = 2 * (x * dydt[j] + y * dxdt[j]);  // 2x'y' : tangential distortion

                    // ��(distortion_func_x(x', y') / ��t
                    double dmxdt = (dxdt[j] * cdist * icdist2 + x * dcdist_dt * icdist2 + x * cdist * dicdist2_dt + k[2] * da1dt + k[3] * (dr2dt + 4 * x * dxdt[j]) + k[8] * dr2dt + 2 * r2 * k[9] * dr2dt);
                    // ��(distortion_func_y(x', y') / ��t
                    double dmydt = (dydt[j] * cdist * icdist2 + y * dcdist_dt * icdist2 + y * cdist * dicdist2_dt + k[2] * (dr2dt + 4 * y * dydt[j]) + k[3] * da1dt + k[10] * dr2dt + 2 * r2 * k[11] * dr2dt);

                    // angular parameter�� ���� �̺�, ��G/��t = ��(xg, yg) / ��t
                    dXdYd = dMatTilt * Vec2d(dmxdt, dmydt);

                    // camera matrix�� ������ ���� �̺�, ��A/��t = ��(u, v) / ��t
                    dpdt_p[j] = fx * dXdYd(0);
                    dpdt_p[dpdt_step + j] = fy * dXdYd(1);
                }
                dpdt_p += dpdt_step * 2;
            }
            else noop;

            if (dpdr_p)
            {
                // Jacobian(R) = ��R/��r * [X Y Z]^T, R: rotation matrix, r: Rodrigues vector(vx=rx, vy=ry, vz=rz), [X,Y,Z]: vertex
                double dx0dr[] =  // ��R/��rx
                {
                        X * dRdr[0] + Y * dRdr[1] + Z * dRdr[2],
                        X * dRdr[9] + Y * dRdr[10] + Z * dRdr[11],
                        X * dRdr[18] + Y * dRdr[19] + Z * dRdr[20]
                };
                double dy0dr[] = // ��R/��ry
                {
                        X * dRdr[3] + Y * dRdr[4] + Z * dRdr[5],
                        X * dRdr[12] + Y * dRdr[13] + Z * dRdr[14],
                        X * dRdr[21] + Y * dRdr[22] + Z * dRdr[23]
                };
                double dz0dr[] = // ��R/��rz
                {
                        X * dRdr[6] + Y * dRdr[7] + Z * dRdr[8],
                        X * dRdr[15] + Y * dRdr[16] + Z * dRdr[17],
                        X * dRdr[24] + Y * dRdr[25] + Z * dRdr[26]
                };

                for (j = 0; j < 3; j++)
                {
                    // ��D/��r
                    double dxdr = z * (dx0dr[j] - x * dz0dr[j]); // ��(x' = x/z)/��r
                    double dydr = z * (dy0dr[j] - y * dz0dr[j]); // ��(y' = y/z)/��r

                    double dr2dr = 2 * x * dxdr + 2 * y * dydr; // ��(x'^2 + y'^2)/��r

                    double dcdist_dr = (k[0] + 2 * k[1] * r2 + 3 * k[4] * r4) * dr2dr;
                    double dicdist2_dr = -icdist2 * icdist2 * (k[5] + 2 * k[6] * r2 + 3 * k[7] * r4) * dr2dr;

                    double da1dr = 2 * (x * dydr + y * dxdr); // ��(2x'*y') / ��r

                    // ��(distortion_func_x(x', y') / ��r
                    double dmxdr = (dxdr * cdist * icdist2 + x * dcdist_dr * icdist2 + x * cdist * dicdist2_dr +
                        k[2] * da1dr + k[3] * (dr2dr + 4 * x * dxdr) + (k[8] + 2 * r2 * k[9]) * dr2dr);

                    // ��(distortion_func_y(x', y') / ��r
                    double dmydr = (dydr * cdist * icdist2 + y * dcdist_dr * icdist2 + y * cdist * dicdist2_dr +
                        k[2] * (dr2dr + 4 * y * dydr) + k[3] * da1dr + (k[10] + 2 * r2 * k[11]) * dr2dr);

                    // ��G/��r = ��(xg, yg) / ��r
                    dXdYd = dMatTilt * Vec2d(dmxdr, dmydr);

                    // ��A/��r = ��(u, v) / ��r
                    dpdr_p[j] = fx * dXdYd(0);
                    dpdr_p[dpdr_step + j] = fy * dXdYd(1);
                }
                dpdr_p += dpdr_step * 2;
            }
            else noop;

            if (dpdo_p) // object point(vertex)
            {
                // differential of (x', y') the in normalized image plane with respect to vertex o = (X, Y, Z) in world space
                // x = R0*X + R1*Y + R2*Z
                // y = R3*X + R4*Y + R5*Z
                // z = R6*X + R7*Y + R8*Z
                // x' = x/z
                // y' = y/z
                // note that x' = x*z, where z = 1/z at the above
                double dxdo[] = { z * (R[0] - x * z * z0 * R[6]),  // orginally, (1/z)(R[0] - (x/z)R[6])
                                  z * (R[1] - x * z * z0 * R[7]),
                                  z * (R[2] - x * z * z0 * R[8]) };

                double dydo[] = { z * (R[3] - y * z * z0 * R[6]),
                                  z * (R[4] - y * z * z0 * R[7]),
                                  z * (R[5] - y * z * z0 * R[8]) };

                for (j = 0; j < 3; j++)
                {
                    double dr2do = 2 * x * dxdo[j] + 2 * y * dydo[j]; // r^2 = x'^2 + y'^2
                    double dr4do = 2 * r2 * dr2do;
                    double dr6do = 3 * r4 * dr2do;
                    double da1do = 2 * y * dxdo[j] + 2 * x * dydo[j];
                    double da2do = dr2do + 4 * x * dxdo[j];
                    double da3do = dr2do + 4 * y * dydo[j];

                    double dcdist_do = k[0] * dr2do + k[1] * dr4do + k[4] * dr6do;

                    double dicdist2_do = -icdist2 * icdist2 * (k[5] * dr2do + k[6] * dr4do + k[7] * dr6do);

                    // ��(distortion_func_x(x', y') / ��o
                    double dxd0_do = cdist * icdist2 * dxdo[j]
                        + x * icdist2 * dcdist_do + x * cdist * dicdist2_do
                        + k[2] * da1do + k[3] * da2do + k[8] * dr2do
                        + k[9] * dr4do;
                    // ��(distortion_func_y(x', y') / ��o
                    double dyd0_do = cdist * icdist2 * dydo[j]
                        + y * icdist2 * dcdist_do + y * cdist * dicdist2_do
                        + k[2] * da3do + k[3] * da1do + k[10] * dr2do
                        + k[11] * dr4do;

                    // ��G/��o = ��(xg, yg) / ��o
                    dXdYd = dMatTilt * Vec2d(dxd0_do, dyd0_do);

                    // ��A/��o = ��(u, v) / ��o
                    dpdo_p[i * 3 + j] = fx * dXdYd(0);
                    dpdo_p[dpdo_step + i * 3 + j] = fy * dXdYd(1);
                }

                dpdo_p += dpdo_step * 2;
            }
            else noop;
        }
        else noop;
    }


    if (_m != imagePoints) cvConvert(_m, imagePoints);
    else noop;

    if (_dpdr != dpdr) cvConvert(_dpdr, dpdr);
    else noop;

    if (_dpdt != dpdt) cvConvert(_dpdt, dpdt);
    else noop;

    if (_dpdf != dpdf)  cvConvert(_dpdf, dpdf);
    else noop;

    if (_dpdc != dpdc) cvConvert(_dpdc, dpdc);
    else noop;

    if (_dpdk != dpdk)  cvConvert(_dpdk, dpdk);
    else noop;

    if (_dpdo != dpdo) cvConvert(_dpdo, dpdo);
    else noop;
}


int sanProject::Rodrigues2(const CvMat* src, /*[in] 3x1 Rodrigues vector or 3x3 rotation matrix  */
                           CvMat* dst,       /*[out] 3x3 rotation matrix or 3x1 Rodrigues vector */
                           CvMat* jacobian)  /*[out] 9x3 Jacobian matrix or 3x9 Jacobian matrix */
{
    int depth = 0;
    int elem_size = 0;
    int i = 0;
    int k = 0;
    double J[27] = {0.0, };
    CvMat _J = cvMat(3, 9, CV_64F, J);


    if (!CV_IS_MAT(src))
    {
        string msg = string("$src input argument is not a valid matrix");
        throw invalid_argument(__FUNCTION__ + delimiter(msg));
    }
    else noop;

    if (!CV_IS_MAT(dst))
    {
        string msg = string("$dst output argument is not a valid matrix");
        throw invalid_argument(__FUNCTION__ + delimiter(msg));
    }
    else noop;

    depth = CV_MAT_DEPTH(src->type);
    elem_size = CV_ELEM_SIZE(depth);

    if (depth != CV_32F && depth != CV_64F)
    {
        string msg = string("$The matrices must have 32f or 64f data typ");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }
    else noop;

    if (!CV_ARE_DEPTHS_EQ(src, dst))
    {
        string msg = string("$All the matrices must have the same data type");
        throw runtime_error(__FUNCTION__ + delimiter(msg));
    }
    else noop;


    if (jacobian)
    {
        if (!CV_IS_MAT(jacobian))
        {
            string msg = string("$Jacobian is not a valid matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if (!CV_ARE_DEPTHS_EQ(src, jacobian) || CV_MAT_CN(jacobian->type) != 1)
        {
            string msg = string("$Jacobian must have 32fC1 or 64fC1 datatype");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if ((jacobian->rows != 9 || jacobian->cols != 3) && (jacobian->rows != 3 || jacobian->cols != 9))
        {
            string msg = string("$Jacobian must be 3x9 or 9x3");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;
    }
    else noop;

    if (src->cols == 1 || src->rows == 1) // Rodrigues 3x1 matrix or 1x3  matrix (9x3 Jacobian matrix)
    {
        double vx, vy, vz, theta;
        double var_vx, var_vy, var_vz;
        int step = src->rows > 1 ? src->step / elem_size : 1;

        if (src->rows + src->cols * CV_MAT_CN(src->type) - 1 != 3)
        {
            string msg = string("$Input matrix must be 1x3, 3x1 or 3x3");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if (dst->rows != 3 || dst->cols != 3 || CV_MAT_CN(dst->type) != 1)
        {
            string msg = string("$Output matrix must be 3x3, single-channel floating point matrix");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        if (depth == CV_32F)
        {
            vx = src->data.fl[0];
            vy = src->data.fl[step];
            vz = src->data.fl[step * 2];
        }
        else
        {
            vx = src->data.db[0];
            vy = src->data.db[step];
            vz = src->data.db[step * 2];
        }

        // var_v = theta ||v|| ( varv is unit vector.)
        theta = sqrt(vx * vx + vy * vy + vz * vz);

        if (theta < DBL_EPSILON)
        {
            cvSetIdentity(dst);

            if (jacobian)
            {
                memset(J, 0, sizeof(J));
                J[5] = J[15] = J[19] = -1;
                J[7] = J[11] = J[21] = 1;
            }
        }
        else
        {
            const double I[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

            double c = cos(theta);
            double s = sin(theta);
            double c1 = 1. - c;
            double inv_theta = theta ? 1. / theta : 0.;

            var_vx = inv_theta * vx; // var_v = (1/��) * v
            var_vy = inv_theta * vy;
            var_vz = inv_theta * vz;

            // rotaion matrix formula from a Rodrigus vector
            // R(var_v, θ)  = cos(θ) * I + (1 - cos(θ)) * var_v * transpose(var_v) + sin(θ) * [var_v]x
            // refer to : https://ghebook.blogspot.com/2020/08/blog-post.html  expression 12

            //3x3 cross-product matrix [var_x]x = [   0      -var_vz      var_vy
            //                                     var_vz       0        -var_vx
            //                                    -var_vy     var_vx       0       ]
                        
            //var_v * var_v^T = var_v * trans(var_v)
            double rrt[] = { var_vx * var_vx, var_vx * var_vy, var_vx * var_vz,
                             var_vx * var_vy, var_vy * var_vy, var_vy * var_vz,
                             var_vx * var_vz, var_vy * var_vz, var_vz * var_vz };

            //[var_v]x ���
            double _r_x_[] = {   0,      -var_vz,   var_vy,
                                 var_vz,      0,     -var_vx,
                                 -var_vy,    var_vx,      0 };

            double R[9];
            CvMat _R = cvMat(3, 3, CV_64F, R);

            // R = cos(theta)*I + (1 - cos(theta))*r*transpose(r) + sin(theta)*[r_x]
            // where [r_x] is [0 -rz ry; rz 0 -rx; -ry rx 0] as a 3x3 cross-product matrix
            for (k = 0; k < 9; k++)
                R[k] = c * I[k] + c1 * rrt[k] + s * _r_x_[k];

            cvConvert(&_R, dst);

            //Jacobian matrix
            if (jacobian)
            {
                double drrt[] = { var_vx + var_vx, var_vy, var_vz, var_vy, 0, 0, var_vz, 0, 0, 
                                  0, var_vx, 0, var_vx, var_vy + var_vy, var_vz, 0, var_vz, 0, 
                                  0, 0, var_vx, 0, 0, var_vy, var_vx, var_vy, var_vz + var_vz };

                double d_r_x_[] = { 0, 0, 0, 0, 0, -1, 0, 1, 0, 
                                    0, 0, 1, 0, 0, 0, -1, 0, 0, 
                                    0, -1, 0, 1, 0, 0, 0, 0, 0 };

                for (i = 0; i < 3; i++)
                {
                    double var_vi = i == 0 ? var_vx : i == 1 ? var_vy : var_vz;
                    double a0 = -s * var_vi;
                    double a1 = (s - 2 * c1 * inv_theta) * var_vi;
                    double a2 = c1 * inv_theta;
                    double a3 = (c - s * inv_theta) * var_vi;
                    double a4 = s * inv_theta;

                    for (k = 0; k < 9; k++)
                        J[i * 9 + k] = a0 * I[k] + a1 * rrt[k] + a2 * drrt[i * 9 + k] + a3 * _r_x_[k] + a4 * d_r_x_[i * 9 + k];
                }
            }
        }
    }
    else if (src->cols == 3 && src->rows == 3) 
    {
        double rx, ry, rz;
        double theta, s, c;
        double vx=0.0, vy=0.0, vz=0.0;
        double R[9], U[9], V[9], W[3];

        CvMat _R = cvMat(3, 3, CV_64F, R);
        CvMat _U = cvMat(3, 3, CV_64F, U);
        CvMat _V = cvMat(3, 3, CV_64F, V);
        CvMat _W = cvMat(3, 1, CV_64F, W);

        int step = dst->rows > 1 ? dst->step / elem_size : 1;


        if ((dst->rows != 1 || dst->cols * CV_MAT_CN(dst->type) != 3) && (dst->rows != 3 || dst->cols != 1 || CV_MAT_CN(dst->type) != 1))
        {
            string msg = string("$Output matrix must be 1x3 or 3x1");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        cvConvert(src, &_R);

        if (!cvCheckArr(&_R, CV_CHECK_RANGE + CV_CHECK_QUIET, -100, 100))
        {
            cvZero(dst);
            if (jacobian) cvZero(jacobian);
            else noop;

            return 0; // failed  
        }

        // check once again if the matrix is an orthogonal matrix.
        SVD(&_R, &_W, &_U, &_V, CV_SVD_MODIFY_A + CV_SVD_U_T + CV_SVD_V_T); // R = U * W * V
        cvGEMM(&_U, &_V, 1, 0, 0, &_R, CV_GEMM_A_T); // _R = _U * _V

        // p = [ rx ry rz]^T, var_v = (1 / (2*sin(theta))) * p
        rx = R[7] - R[5];
        ry = R[2] - R[6];
        rz = R[3] - R[1];

        s = sqrt((rx * rx + ry * ry + rz * rz) * 0.25); // var_vx^2 + var_vy^2 + var_vz^2 = 1, var_vx = m *rx, var_vy = m*ry, var_vz = rz
        c = (R[0] + R[4] + R[8] - 1) * 0.5; //trace(R) = tr(R) = 2*cos(theta) + 1 = R11 + R22 + R33
        c = c > 1. ? 1. : c < -1. ? -1. : c;
        theta = acos(c);

        if (s < 1e-5)
        {
            double t;

            if (c > 0)
                rx = ry = rz = 0;
            else
            {
                t = (R[0] + 1) * 0.5;
                rx = theta * sqrt(MAX(t, 0.));
                t = (R[4] + 1) * 0.5;
                ry = theta * sqrt(MAX(t, 0.)) * (R[1] < 0 ? -1. : 1.);
                t = (R[8] + 1) * 0.5;
                rz = theta * sqrt(MAX(t, 0.)) * (R[2] < 0 ? -1. : 1.);
            }

            if (jacobian)
            {
                memset(J, 0, sizeof(J));
                if (c > 0)
                {
                    J[5] = J[15] = J[19] = -0.5;
                    J[7] = J[11] = J[21] = 0.5;
                }
                else noop;
            }
            else noop;
        }
        else
        {
            // �Է�: 3x3 rotation matrix R
            // ���: 3x1 Rodriges' vector v = �� * var_v
            // Jacobian Matrix J : �Է¿� ���� ����� ������ȭ�� ��Ÿ���� ��� J_v(R) = ��v/��R ==> ��v/��(vec(R))
            // var_v: ���� ���� ����(ȸ�� ���� ���� ��Ÿ���� ���� ����)
            // ��: ȸ���ؾ� �� ����
            // p=[rx ry rz]^T : �Է����� �־��� ȸ�� ��ķ� ���� ����� ������ ����, rx = R32 - R23, ry =  R13 - R31, rz = R
            double m = 1 / (2 * s);

            if (jacobian)
            {
                double t;
                double dtheta_dtr = -1. / s;    // tr(R) = 2*cos(��)+1, ��tr/�ӥ� = -2*sin(��), �ӥ�/��tr = -1/(2*sin(��))
                double dm_dtheta = -m * c / s; // ��m/�ӥ�
                double d1 = dm_dtheta * (0.5 * dtheta_dtr); //��m/��tr
                double d2 = 0.5 * dtheta_dtr; // ��tr/�ӥ� = -2 sin(��)

                // P = [rx ry rz]^T,  rx, ry, rz: R�� ��Ŀ��� ������ ����
                // var_v = m * P,     ||var_v || = 1
                // v = �� * var_v,     ��^2 = ||var_v||^2
                // var  = [rx, ry, rz, m, ��]^T
                // var1 = [m, ��]^T
                // var2 = [var_vx, var_vy, var_vz, ��]^T,
                // tr   = R11 + R22 + R33 = 2 * cos(��) + 1

                // ��v/��(vec(R))   = (��v/��var) * (��var/��R) = (��v/��var2) * (��var2/��var) * (��var/��var1) * (��var1/�ӥ�) * (�ӥ�/��tr(R)) * (��tr(R)/��R)
                // ��v/��var = (��v/��var2) * (��var2/��var)
                // ��var/��R = (��var/��var1) * (��var1/�ӥ�) * (�ӥ�/��tr(R)) * (��tr(R)/��(vec(R)))

                // ��var/��R
                double dvardR[5 * 9] =
                        {
                                0,  0,  0,  0,  0,  1,  0, -1,  0,
                                0,  0, -1,  0,  0,  0,  1,  0,  0,
                                0,  1,  0, -1,  0,  0,  0,  0,  0,
                                d1, 0,  0,  0, d1,  0,  0,  0,  d1,
                                d2, 0,  0,  0, d2,  0,  0,  0,  d2
                        };

                //(��var2 / ��var)
                double dvar2dvar[] =
                        {
                                m,   0,  0,  rx,  0,
                                0,   m,  0,  ry,  0,
                                0,   0,  m,  rz,  0,
                                0,   0,  0,   0,  1
                        };

                // ���� ���(Rodrigues ȸ������)�� �ǹ��ϹǷ� v�� ����.
                // ��v/��var2
                double dvdvar2[] =
                        {
                                theta,     0,       0,  rx * m,
                                0,     theta,       0,  ry * m,
                                0,         0,   theta,  rz * m
                        };

                CvMat _dvardR = cvMat(5, 9, CV_64FC1, dvardR);
                CvMat _dvar2dvar = cvMat(4, 5, CV_64FC1, dvar2dvar);
                CvMat _dvdvar2 = cvMat(3, 4, CV_64FC1, dvdvar2);
                double t0[3 * 5];
                CvMat _t0 = cvMat(3, 5, CV_64FC1, t0);

                cvMatMul(&_dvdvar2, &_dvar2dvar, &_t0);  // t0(3x5) =  (��v/��var2(3x4)) *  (��var2/��var(4x5))
                cvMatMul(&_t0, &_dvardR, &_J);           // J_R(v)(3x9) = t0(3x5) * ��var/��R(5x9)  --> J(9x3))

                // transpose every row of _J (treat the rows as 3x3 matrices): R�� ����ȭ�Ͽ� ����ǹǷ� �� ���� ����� ��Ī�Ǿ� ��Ÿ���Ƿ� ���������� swp()�ؾ� ��
                CV_SWAP(J[1], J[3], t); CV_SWAP(J[2], J[6], t); CV_SWAP(J[5], J[7], t);
                CV_SWAP(J[10], J[12], t); CV_SWAP(J[11], J[15], t); CV_SWAP(J[14], J[16], t);
                CV_SWAP(J[19], J[21], t); CV_SWAP(J[20], J[24], t); CV_SWAP(J[23], J[25], t);
            }

            vx = (m * theta) * rx; //m = 1 / (2*sin(��))
            vy = (m * theta) * ry;
            vz = (m * theta) * rz;
        }

        if (depth == CV_32F)
        {
            dst->data.fl[0] = (float)vx;
            dst->data.fl[step] = (float)vy;
            dst->data.fl[step * 2] = (float)vz;
        }
        else
        {
            dst->data.db[0] = vx;
            dst->data.db[step] = vy;
            dst->data.db[step * 2] = vz;
        }
    }
    else noop;

    if (jacobian) // Jacobian matrix buffer�� �����Ǿ� ���� �� ������� ����
    {
        if (depth == CV_32F)
        {
            if (jacobian->rows == _J.rows)
                cvConvert(&_J, jacobian);
            else
            {
                float Jf[3 * 9];
                CvMat _Jf = cvMat(_J.rows, _J.cols, CV_32FC1, Jf);
                cvConvert(&_J, &_Jf);
                cvTranspose(&_Jf, jacobian);
            }
        }
        else if (jacobian->rows == _J.rows)
            cvCopy(&_J, jacobian);
        else
            cvTranspose(&_J, jacobian);
    }
    else noop;

    return 1; 
}


void sanProject::SVD(CvArr* aarr, CvArr* warr, CvArr* uarr, CvArr* varr, int flags)
{
    cv::Mat a = cv::cvarrToMat(aarr), w = cv::cvarrToMat(warr), u, v;
    int m = a.rows, n = a.cols, type = a.type(), mn = std::max(m, n), nm = std::min(m, n);

#if(0)
    CV_Assert(w.type() == type &&
              (w.size() == cv::Size(nm, 1)  || w.size() == cv::Size(1, nm) ||
               w.size() == cv::Size(nm, nm) || w.size() == cv::Size(n, m)));
#else
    if ((w.type() != type) || !(w.size() == cv::Size(nm, 1) || w.size() == cv::Size(1, nm) || w.size() == cv::Size(nm, nm) || w.size() == cv::Size(n, m)))
    {
        string msg = string("$w.size() wrong");
        throw out_of_range(__FUNCTION__ + delimiter(msg));
    }
    else noop;
#endif


    cv::SVD svd;

    if (w.size() == cv::Size(nm, 1))
        svd.w = cv::Mat(nm, 1, type, w.ptr());
    else if (w.isContinuous())
        svd.w = w;
    else noop;

    if (uarr)
    {
        u = cv::cvarrToMat(uarr);
        //CV_Assert(u.type() == type);
        if (u.type() != type)
        {
            string msg = string("$u.type() != type");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        svd.u = u;
    }
    else noop;


    if (varr)
    {
        v = cv::cvarrToMat(varr);
        //CV_Assert(v.type() == type);
        if (v.type() != type)
        {
            string msg = string("$v.type() != type");
            throw runtime_error(__FUNCTION__ + delimiter(msg));
        }
        else noop;

        svd.vt = v;
    }
    else noop;

    svd(a, ((flags & CV_SVD_MODIFY_A) ? cv::SVD::MODIFY_A : 0) |
           ((!svd.u.data && !svd.vt.data) ? cv::SVD::NO_UV : 0) |
           ((m != n && (svd.u.size() == cv::Size(mn, mn) ||
                        svd.vt.size() == cv::Size(mn, mn))) ? cv::SVD::FULL_UV : 0));

    if (!u.empty())
    {
        if (flags & CV_SVD_U_T)
            cv::transpose(svd.u, u);
        else if (u.data != svd.u.data)
        {
            //CV_Assert(u.size() == svd.u.size());
            if (u.size() != svd.u.size())
            {
                string msg = string("$u.size() != svd.u.size()");
                throw runtime_error(__FUNCTION__ + delimiter(msg));
            }
            else noop;

            svd.u.copyTo(u);
        }
        else noop;
    }
    else noop;

    if (!v.empty())
    {
        if (!(flags & CV_SVD_V_T))
            cv::transpose(svd.vt, v);
        else if (v.data != svd.vt.data)
        {
            //CV_Assert(v.size() == svd.vt.size());
            if (v.size() != svd.vt.size())
            {
                string msg = string("$v.size() != svd.vt.size()");
                throw runtime_error(__FUNCTION__ + delimiter(msg));
            }
            else noop;

            svd.vt.copyTo(v);
        }
        else noop;
    }
    else noop;

    if (w.data != svd.w.data)
    {
        if (w.size() == svd.w.size())
            svd.w.copyTo(w);
        else
        {
            w = cv::Scalar(0);
            cv::Mat wd = w.diag();
            svd.w.copyTo(wd);
        }
    }
    else noop;
}

void sanProject::computeTiltProjectionMatrix(double tauX,
                                     double tauY,
                                     Matx<double, 3, 3>* matTilt,
                                     Matx<double, 3, 3>* dMatTiltdTauX,
                                     Matx<double, 3, 3>* dMatTiltdTauY,
                                     Matx<double, 3, 3>* invMatTilt)
{
    double cTauX = cos(tauX);
    double sTauX = sin(tauX);
    double cTauY = cos(tauY);
    double sTauY = sin(tauY);
    Matx<double, 3, 3> matRotX = Matx<double, 3, 3>(1, 0, 0, 0, cTauX, sTauX, 0, -sTauX, cTauX);
    Matx<double, 3, 3> matRotY = Matx<double, 3, 3>(cTauY, 0, -sTauY, 0, 1, 0, sTauY, 0, cTauY);
    Matx<double, 3, 3> matRotXY = matRotY * matRotX;
    Matx<double, 3, 3> matProjZ = Matx<double, 3, 3>(matRotXY(2, 2), 0, -matRotXY(0, 2), 0, matRotXY(2, 2), -matRotXY(1, 2), 0, 0, 1);

    if (matTilt) // Matrix for trapezoidal distortion of tilted image sensor
    {
        *matTilt = matProjZ * matRotXY;
    }
    else noop;

    if (dMatTiltdTauX)  // Derivative with respect to tauX
    {
        Matx<double, 3, 3> dMatRotXYdTauX = matRotY * Matx<double, 3, 3>(0, 0, 0, 0, -sTauX, cTauX, 0, -cTauX, -sTauX);
        Matx<double, 3, 3> dMatProjZdTauX = Matx<double, 3, 3>(dMatRotXYdTauX(2, 2), 0, -dMatRotXYdTauX(0, 2), 0, dMatRotXYdTauX(2, 2), -dMatRotXYdTauX(1, 2), 0, 0, 0);
        *dMatTiltdTauX = (matProjZ * dMatRotXYdTauX) + (dMatProjZdTauX * matRotXY);
    }
    else noop;

    if (dMatTiltdTauY) // Derivative with respect to tauY
    {
        Matx<double, 3, 3> dMatRotXYdTauY = Matx<double, 3, 3>(-sTauY, 0, -cTauY, 0, 0, 0, cTauY, 0, -sTauY) * matRotX;
        Matx<double, 3, 3> dMatProjZdTauY = Matx<double, 3, 3>(dMatRotXYdTauY(2, 2), 0, -dMatRotXYdTauY(0, 2), 0, dMatRotXYdTauY(2, 2), -dMatRotXYdTauY(1, 2), 0, 0, 0);
        *dMatTiltdTauY = (matProjZ * dMatRotXYdTauY) + (dMatProjZdTauY * matRotXY);
    }
    else noop;

    if (invMatTilt)
    {
        double inv = 1. / matRotXY(2, 2);
        Matx<double, 3, 3> invMatProjZ = Matx<double, 3, 3>(inv, 0, inv * matRotXY(0, 2), 0, inv, inv * matRotXY(1, 2), 0, 0, 1);
        *invMatTilt = matRotXY.t() * invMatProjZ;
    }
    else noop;
}


