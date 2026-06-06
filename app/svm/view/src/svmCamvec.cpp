#include "svmCamvec.hpp"
#include <memory>

#ifdef __cplusplus
extern "C"
{
	void ludcmp_float(float** LU, float** A, int n, int* indx, float* d);
	void back_sub_float(float** LU, int n, int* indx, float* b);
	void invMat(float A[], int A_rows, int A_cols, float invA[], int invA_rows, int invA_cols);
}
#endif


glm::mat3 rotate_around_axis(glm::vec3 rotation_vec /*unit vector*/, float rotation_ang /* radian */)  // Rodrigues
{
	float th = rotation_ang;
	float Rx = rotation_vec.x;
	float Ry = rotation_vec.y;
	float Rz = rotation_vec.z;
	float c = (float)cos(th);
	float s = (float)sin(th);
	float c1 = 1.0f - c;

	// Rodrigues
	// var_v = v / ||v|| wheren v is a rotation vector and ||v|| is �� as the norm of v
	// R(var_v, ��)  = cos(��) * I + (1 - cos(��)) * var_v * transpose(var_v) + sin(��) * [var_v]x  where var_v is unit rotation vector and �� is rotation angle
	// refer to : https://ghebook.blogspot.com/2020/08/blog-post.html  expression 12

	glm::mat3 R(0.0f);
	R[0][0] = c + Rx * Rx * c1;          R[1][0] =     Rx * Ry * c1 - Rz * s;  R[2][0] =     Rx * Rz * c1 + Ry * s;
	R[0][1] =     Ry * Rx * c1 + Rz * s; R[1][1] = c + Ry * Ry * c1;           R[2][1] =     Ry * Rz * c1 - Rx * s;
	R[0][2] =     Rz * Rx * c1 - Ry * s; R[1][2] =     Rz * Ry * c1 + Rx * s;  R[2][2] = c + Rz * Rz * c1;

	return R;
}



glm::mat4 LookAt(glm::vec3 camera_position/*[in]*/, glm::vec3 target_position/*[in]*/)
{
	glm::vec3 reference_up(0.0f, 1.0f, 0.0f);
	glm::vec3 camForward = glm::normalize(target_position - camera_position); // -Z axis

	glm::vec3 camRight(1.0f, 0.0f, 0.0f); // X axis
	float angle = glm::degrees(acos(glm::dot(camForward, reference_up)));
	if (FLT_EPSILON < fabs(angle) && FLT_EPSILON < fabs(angle - 180.0f)) // parrell to Y-axis
		camRight = glm::normalize(glm::cross(camForward, reference_up));

	glm::vec3 camUp = glm::normalize(glm::cross(camRight, camForward)); // Y axis
	glm::mat4 view_matrix(1.0f);

#if (1)
	//	 	  [right.x     right.y      right.z]
	//  Rwc = [up.x        up.y            up.z]
	//	 	  [front.x     front.y      front.z]	
	view_matrix[0][0] = camRight.x;      view_matrix[1][0] = camRight.y;	 view_matrix[2][0] = camRight.z;
	view_matrix[0][1] = camUp.x;   	     view_matrix[1][1] = camUp.y; 	     view_matrix[2][1] = camUp.z;
	view_matrix[0][2] = -camForward.x;   view_matrix[1][2] = -camForward.y;	 view_matrix[2][2] = -camForward.z;

	//                    [right.x     right.y      right.z][twx]
	//  Tc = -Rwc * Tw = -[up.x        up.y            up.z][twy]
	//                    [front.x     front.y      front.z][twz]
	view_matrix[3][0] = -(camRight.x    * camera_position.x +  camRight.y   * camera_position.y +  camRight.z   * camera_position.z);
	view_matrix[3][1] = -(camUp.x       * camera_position.x +  camUp.y      * camera_position.y +  camUp.z      * camera_position.z);
	view_matrix[3][2] = -(-camForward.x * camera_position.x + -camForward.y * camera_position.y + -camForward.z * camera_position.z);
#else
	view_matrix[0][0] = camRight.x;   view_matrix[1][0] = camUp.x;	 view_matrix[2][0] = -camForward.x;
	view_matrix[0][1] = camRight.y;   view_matrix[1][1] = camUp.y; 	 view_matrix[2][1] = -camForward.y;
	view_matrix[0][2] = camRight.z;   view_matrix[1][2] = camUp.z;	 view_matrix[2][2] = -camForward.z;

	view_matrix[3][0] = -1.0f * (camRight.x * camera_position.x + camUp.x * camera_position.y - camForward.x * camera_position.z);
	view_matrix[3][1] = -1.0f * (camRight.y * camera_position.y + camUp.y * camera_position.y - camForward.y * camera_position.z);
	view_matrix[3][2] = -1.0f * (camRight.z * camera_position.z + camUp.z * camera_position.y - camForward.z * camera_position.z);
#endif

	return view_matrix;
}


glm::vec3 get_intersection_with_ground(glm::vec3 camera_pos, glm::vec3 camera_dir) /* extrinsic parameter */
{
	glm::mat4 view_matrix = VM4x4(camera_pos, camera_dir);
	glm::vec3 direction_vec = glm::normalize(glm::vec3(-view_matrix[0][2], -view_matrix[1][2], -view_matrix[2][2])); //  camera forward vector
	float angle = glm::degrees(glm::acos(glm::dot(direction_vec, glm::vec3(0.0f, 0.0f, -1.0f)))); // when -90/90, parralel to the ground 

	glm::vec3 intersection(0.0f, 0.0f, 0.0f); // intersection
	if (FLT_EPSILON < fabs(fabs(angle) - 90.0f)) // reference ground(XY plane of Z=0)
	{
		intersection.x = camera_pos.x - direction_vec.x * camera_pos.z / direction_vec.z;
		intersection.y = camera_pos.y - direction_vec.y * camera_pos.z / direction_vec.z;
		intersection.z = 0.0f;
	}
	else noop;

	return intersection;
}


glm::mat4 VM4x4(glm::vec3 vcamPos/*[in]*/, glm::vec3 vcamDir/*[in] in degree*/)  // extrinsic parameter
{
	double b00 = 0.0, b01 = 0.0, b02 = 0.0;
	double b10 = 0.0, b11 = 0.0, b12 = 0.0;
	double b20 = 0.0, b21 = 0.0, b22 = 0.0;
	double twx, twy, twz;
	double tcx, tcy, tcz;
	double thy, thx, thz;
	const double correction_num = 10000000.0;

	twx = round((double)vcamPos[0] * correction_num) / correction_num;
	twy = round((double)vcamPos[1] * correction_num) / correction_num;
	twz = round((double)vcamPos[2] * correction_num) / correction_num;
	thx = round((double)vcamDir[0] * correction_num * (double)M_PI / 180.0) / correction_num;
	thy = round((double)vcamDir[1] * correction_num * (double)M_PI / 180.0) / correction_num;
	thz = round((double)vcamDir[2] * correction_num * (double)M_PI / 180.0) / correction_num;

	// OpenGL Coordinate System.
	// refer to http://www.opengl-tutorial.org/kr/intermediate-tutorials/tutorial-17-quaternions/  opengl-tutorial Tutorial 17: Rotations
	// https://en.wikipedia.org/wiki/Euler_angles
	// OpenGL rotation order(intrinsic rotation): Z-> Y'-> X''
	// Rwc = Rx(-��x) * Ry(-��y) * Rz(-��z) = transpose(Rx(��x)) * transpose(Ry(��y) * transpose(Rz(��z)) = transpose(Rz(��z) * Ry(��y) * Rx(��x))
	b00 = cos(thz) * cos(thy);                                   b01 = cos(thy) * sin(thz);                                   b02 = -sin(thy);
	b10 = cos(thz) * sin(thy) * sin(thx) - cos(thx) * sin(thz);  b11 = cos(thz) * cos(thx) + sin(thz) * sin(thy) * sin(thx);  b12 = cos(thy) * sin(thx);
	b20 = sin(thz) * sin(thx) + cos(thz) * cos(thx) * sin(thy);  b21 = cos(thx) * sin(thz) * sin(thy) - cos(thz) * sin(thx);  b22 = cos(thy) * cos(thx);

	// Xc = Rwc * Xw + Tc;  Tc = -Rwc * Tw
	tcx = -1.0 * (b00 * twx + b01 * twy + b02 * twz);
	tcy = -1.0 * (b10 * twx + b11 * twy + b12 * twz);
	tcz = -1.0 * (b20 * twx + b21 * twy + b22 * twz);

	glm::mat4 Vwc(0);
	Vwc[0][0] = (float)b00;	Vwc[1][0] = (float)b01;	Vwc[2][0] = (float)b02;  Vwc[3][0] = (float)tcx;
	Vwc[0][1] = (float)b10;	Vwc[1][1] = (float)b11;	Vwc[2][1] = (float)b12;  Vwc[3][1] = (float)tcy;
	Vwc[0][2] = (float)b20;	Vwc[1][2] = (float)b21;	Vwc[2][2] = (float)b22;  Vwc[3][2] = (float)tcz;
	Vwc[0][3] = 0.0f;		Vwc[1][3] = 0.0f;		Vwc[2][3] = 0.0f;		 Vwc[3][3] = 1.0f;

	return Vwc;
}


glm::mat4 invVM4x4(glm::vec3 vcamPos/*[in]*/, glm::vec3 vcamDir/*[in] in degree*/) /* extrinsic parameter*/
{
	double b00 = 0.0, b01 = 0.0, b02 = 0.0;
	double b10 = 0.0, b11 = 0.0, b12 = 0.0;
	double b20 = 0.0, b21 = 0.0, b22 = 0.0;
	double twx, twy, twz;
	double thy, thx, thz;
	const double correction_num = 10000000.0;

	twx = round((double)vcamPos[0] * correction_num) / correction_num;
	twy = round((double)vcamPos[1] * correction_num) / correction_num;
	twz = round((double)vcamPos[2] * correction_num) / correction_num;
	thx = round((double)vcamDir[0] * correction_num * (double)M_PI / 180.0) / correction_num;
	thy = round((double)vcamDir[1] * correction_num * (double)M_PI / 180.0) / correction_num;
	thz = round((double)vcamDir[2] * correction_num * (double)M_PI / 180.0) / correction_num;

	// axis rotation, OpenGL Coordinate System.
	// refer to http://www.opengl-tutorial.org/kr/intermediate-tutorials/tutorial-17-quaternions/  opengl-tutorial Tutorial 17: Rotations
	// https://en.wikipedia.org/wiki/Euler_angles
	// rotation order : Z -> Y' -> X'
	// Rwc = Rx(-��x) * Ry(-��y) * Rz(-��z) = transpose(Rx(��x)) * transpose(Ry(��y) * transpose(Rz(��z)) = transpose(Rz(��z) * Ry(��y) * Rx(��x))
	//	    [cos(thz)*cos(thy)                                 cos(thy)*sin(thz)                                  -sin(thy)         ]
	//    = [cos(thz)*sin(thy)*sin(thx) - cos(thx)*sin(thz)    sin(thz)*sin(thy)*sin(thx) + cos(thz)*cos(thx)      cos(thy)*sin(thx)]
	//	    [cos(thz)*cos(thx)*sin(thy) + sin(thz)*sin(thx)    cos(thx)*sin(thz)*sin(thy) - cos(thz)*sin(thx)      cos(thy)*cos(thx)]
	 
    // Rcw = inv(Rwc) = transpose(Rwc)	
	b00 = cos(thz) * cos(thy);	b01 = cos(thz) * sin(thy) * sin(thx) - cos(thx) * sin(thz);  b02 = sin(thz) * sin(thx) + cos(thz) * cos(thx) * sin(thy);
	b10 = cos(thy) * sin(thz);	b11 = cos(thz) * cos(thx) + sin(thz) * sin(thy) * sin(thx);  b12 = cos(thx) * sin(thz) * sin(thy) - cos(thz) * sin(thx);
	b20 = -sin(thy);			    b21 = cos(thy) * sin(thx);									           b22 = cos(thy) * cos(thx);

	glm::mat4 Vcw(0);
	Vcw[0][0] = (float)b00;	Vcw[1][0] = (float)b01;	Vcw[2][0] = (float)b02;  Vcw[3][0] = (float)twx;
	Vcw[0][1] = (float)b10;	Vcw[1][1] = (float)b11;	Vcw[2][1] = (float)b12;  Vcw[3][1] = (float)twy;
	Vcw[0][2] = (float)b20;	Vcw[1][2] = (float)b21;	Vcw[2][2] = (float)b22;  Vcw[3][2] = (float)twz;
	Vcw[0][3] = 0.0f;		Vcw[1][3] = 0.0f;		Vcw[2][3] = 0.0f;		 Vcw[3][3] = 1.0f;

	return Vcw;
}

void VM4x4toEP(glm::mat4 Vwc/* [in]*/, glm::vec3& camPos/*[out]*/, glm::vec3& camDir /* [out] */) // from 4x4 view matrix to extrinsic parameter
{
	double b00, b01, b02;
	double b10, b11, b12;
	double b20, b21, b22;
	double tcx, tcy, tcz;
	double twx, twy, twz;
	double thx, thy, thz, detA;

	// OpenGL Coordinate System.
	// refer to http://www.opengl-tutorial.org/kr/intermediate-tutorials/tutorial-17-quaternions/  opengl-tutorial Tutorial 17: Rotations
	// https://en.wikipedia.org/wiki/Euler_angles
	// OpenGL rotation order(intrinsic rotation): Z-> Y'-> X''
	// Rwc = Rx(-��x) * Ry(-��y) * Rz(-��z) = transpose(Rx(��x)) * transpose(Ry(��y) * transpose(Rz(��z)) = transpose(Rz(��z) * Ry(��y) * Rx(��x))
	// b00 = cos(thz) * cos(thy);                                   b01 = cos(thy) * sin(thz);                                   b02 = -sin(thy);
	// b10 = cos(thz) * sin(thy) * sin(thx) - cos(thx) * sin(thz);  b11 = sin(thz) * sin(thy) * sin(thx) + cos(thz) * cos(thx);  b12 = cos(thy) * sin(thx);
	// b20 = cos(thz) * cos(thx) * sin(thy) + sin(thz) * sin(thx);  b21 = cos(thx) * sin(thz) * sin(thy) - cos(thz) * sin(thx);  b22 = cos(thy) * cos(thx);


	b00 = Vwc[0][0]; b01 = Vwc[1][0]; b02 = Vwc[2][0];  tcx = Vwc[3][0];
	b10 = Vwc[0][1]; b11 = Vwc[1][1]; b12 = Vwc[2][1];  tcy = Vwc[3][1];
	b20 = Vwc[0][2]; b21 = Vwc[1][2]; b22 = Vwc[2][2];  tcz = Vwc[3][2];

	// Xc  = Rwc * Xw + Tc,    Tc = -Rwc * Tw ==> Tw = -inv(Rwc) * Tc ==> Tw = -Rcw * Tc
	twx = -1.0 * (b00 * tcx + b10 * tcy + b20 * tcz);
	twy = -1.0 * (b01 * tcx + b11 * tcy + b21 * tcz);
	twz = -1.0 * (b02 * tcx + b12 * tcy + b22 * tcz);

	detA = b00 * (b11 * b22 - b12 * b21) + b01 * (b12 * b20 - b10 * b22) + b02 * (b10 * b21 - b10 * b21);
	detA = round(detA * 10000.0) / 10000.0;
	if (detA < 0.0)// reflection(det=-1) and permutation(det=-1, odd, trace != 0) matrices
	{
		b00 *= -1.0; b01 *= -1.0; b02 *= -1.0;
		b10 *= -1.0; b11 *= -1.0; b12 *= -1.0;
		b20 *= -1.0; b21 *= -1.0; b22 *= -1.0;
	}
	else
		noop;

	// tilt
	thx = atan2(b12, b22);  // tilt: -90 ~ +90

	if (M_PI_2 < thx)		     thx = thx - (double)M_PI; // when greater than 90 degree
	else if (thx < (-M_PI_2))	 thx = thx + (double)M_PI; // when less than -90 degree
	else noop;

	// roll
	thz = atan2(b01, b00);  // roll: -180 ~ +180
	if (fabs(thz + M_PI) <= 0.0000001) thz += 2.0 * M_PI;
	else noop;
    
	// pan
	thy = asin(-b02);

	camPos[0] = (float)twx;
	camPos[1] = (float)twy;
	camPos[2] = (float)twz;
	camDir[0] = glm::degrees((float)thx);
	camDir[1] = glm::degrees((float)thy);
	camDir[2] = glm::degrees((float)thz);
}

void invMat(float A[],/*[in] input square matrix */
			int A_rows,
			int A_cols,
			float invA[], /*[out] invA = inv(A)*/
			int invA_rows,
			int invA_cols)
{

	int i, j;
	float** ppLU = nullptr;
	float* pLU = nullptr;
	float** ppA = nullptr;
	float* pA = nullptr;
	float* col = nullptr;
	int* indx = nullptr;
	float d = 0.0F;

	if ((0 < A_rows) && (A != nullptr) && (invA != nullptr) && (A_rows == A_cols) &&
		(A_rows == invA_rows) && (invA_rows == invA_cols))
	{
		ppA = (float**)calloc((size_t)A_rows, sizeof(float*));
		pA = (float*)calloc((size_t)A_rows * (size_t)A_cols, sizeof(float));
		for (i = 0; i < A_rows; i++)
		{
			ppA[i] = &pA[i * A_cols];
			for (j = 0; j < A_cols; j++)
				ppA[i][j] = A[i * A_cols + j];
		}

		ppLU = (float**)calloc((size_t)A_rows, sizeof(float*));
		pLU = (float*)calloc((size_t)A_rows * (size_t)A_cols, sizeof(float));
		for (i = 0; i < A_rows; i++)
			ppLU[i] = &pLU[i * A_cols];


		indx = (int*)calloc((size_t)A_rows, sizeof(int));
		col = (float*)calloc((size_t)A_rows, sizeof(float));

		ludcmp_float(ppLU, ppA, A_rows, indx, &d);

		for (i = 0; i < A_rows; i++)
		{
			for (j = 0; j < A_rows; j++)
				col[j] = 0.0F;

			col[i] = 1.0F;

			back_sub_float(ppLU, A_rows, indx, col);

			for (j = 0; j < A_cols; j++)
			{
				invA[j * invA_cols + i] = (FLT_EPSILON < (float)fabs(col[j])) ? col[j] : 0.0f;
			}
		}

		if(indx != nullptr) free(indx); else noop;
		if(col != nullptr) free(col); else noop;
		if(pA != nullptr) free(pA); else noop;
		if(ppA != nullptr) free(ppA); else noop;
		if(pLU != nullptr) free(pLU); else noop;
		if(ppLU != nullptr) free(ppLU); else noop;
	}
}

/* this is the same as the function 'ludcmp' except the data type of parameters */
void ludcmp_float(float** LU/*[out]*/, float** A, int n, int* indx, float* d)
{
	// A = L*U;
	//Given a matrix a[1..n][1..n], this routine replaces it by the LU decomposition of a rowwise
	//permutation of itself. a and n are input. a is output, arranged as in equation (2.3.14) above;
	//indx[1..n] is an output vector that records the row permutation effected by the partial
	//pivoting; d is output as .1 depending on whether the number of row interchanges was even
	//or odd, respectively. This routine is used in combination with lubksb to solve linear equations
	//or invert a matrix.

	int i, j, k;
	int imax = 0;
	float big = 0.0f;
	float dum = 0.0f;
	float sum = 0.0f;
	float temp = 0.0f;
	float* vv = nullptr;


	vv = (float*)calloc((size_t)n, sizeof(float));

	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			LU[i][j] = A[i][j];

	*d = 1.0f;
	for (i = 0; i < n; i++)
	{
		big = 0.0f;
		for (j = 0; j < n; j++)
		{
			temp = (float)fabs(LU[i][j]);
			if (temp > big)
				big = temp;
		}

		vv[i] = (FLT_EPSILON < (float)fabs(big)) ? (1.0f / big) : 0.0f;
	}

	for (j = 0; j < n; j++)
	{
		for (i = 0; i < j; i++)
		{
			sum = LU[i][j];
			for (k = 0; k < i; k++)
				sum -= LU[i][k] * LU[k][j];

			LU[i][j] = sum;
		}

		big = 0.0F;
		for (i = j; i < n; i++)
		{
			sum = LU[i][j];
			for (k = 0; k < j; k++)
				sum -= LU[i][k] * LU[k][j];

			LU[i][j] = sum;
			dum = vv[i] * (float)fabs(sum);

			if (dum >= big)
			{
				big = dum;
				imax = i;
			}
		}

		if (j != imax)
		{
			for (k = 0; k < n; k++)
			{
				dum = LU[imax][k];
				LU[imax][k] = LU[j][k];
				LU[j][k] = dum;
			}
			*d = -(*d);
			vv[imax] = vv[j];
		}

		indx[j] = imax;
		if (LU[j][j] == 0.0f)
			LU[j][j] = (float)TINY;

		if (j != n)
		{
			dum = 1.0f / (LU[j][j]);
			for (i = j + 1; i < n; i++)
				LU[i][j] *= dum;
		}
	}
	free(vv);
}



void back_sub_float(float** LU, int n, int* indx, float* b)
{
	int i, ii = -1;
	int ip, j;
	float sum;

	for (i = 0; i < n; i++)
	{
		ip = indx[i];
		sum = b[ip];
		b[ip] = b[i];

		if (ii >= 0)
			for (j = ii; j < i; j++)
				sum -= LU[i][j] * b[j];
		else if (sum)
			ii = i;
		b[i] = sum;
	}

	for (i = n - 1; i >= 0; i--)
	{
		sum = b[i];
		for (j = i + 1; j < n; j++)
			sum -= LU[i][j] * b[j];

		b[i] = sum / LU[i][i];
	}
}
