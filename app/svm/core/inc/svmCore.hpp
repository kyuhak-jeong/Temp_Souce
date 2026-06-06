#ifndef SVMCORE_HPP_
#define SVMCORE_HPP_

// #ifndef GL_GLEXT_PROTOTYPES
// #define GL_GLEXT_PROTOTYPES (1)

#include "constants.h"

#ifndef WND_WIDTH
	#define WND_WIDTH 							(int) (1920)
#endif
#ifndef WND_HEIGHT
	#define WND_HEIGHT 							(int) (1080)
#endif

#ifndef SVM_CAMERAS_NUM
	#define SVM_CAMERAS_NUM 					(int) (4)
#endif

#ifndef ADD_CAMERAS_NUM
	#define ADD_CAMERAS_NUM						(int) (1)
#endif

#ifndef EXT_CAMERAS_NUM
	#define EXT_CAMERAS_NUM						(int) (4)
#endif

#ifndef MAX_BUFFER_NUM
	#define MAX_BUFFER_NUM 						(int) (SVM_CAMERAS_NUM + ADD_CAMERAS_NUM + EXT_CAMERAS_NUM)
#endif

#define MANUAL_CALIBRATION						(int)(0)
#define AUTO_CALIBRATION						(int)(1)

#define AFFINE_COEF_NUM							(int)(3)
#define POLY_COEF_MAX_NUM						(int)(5)
#define INV_POLY_COEF_MAX_NUM					(int)(16)

#define CONTROL_POINTS_NUM						(int)(6)
#define PATTERN_POINTS_NUM						(int)(3)
#define PATTERN_MAX_NUM							(int)(6 * 2)


#define CAMID_FRONT								(int)(0)
#define CAMID_RIGHT								(int)(1)
#define CAMID_REAR								(int)(2)
#define CAMID_LEFT								(int)(3)
#define CAMID_ADD0								(int)(4)


#ifndef PROJECT_SRC_DIR    // Windows not defined, but Linux defined.
	#define PROJECT_SRC_DIR ".."
#endif

// extern const string _RESOURCES_PATH_ = string(PROJECT_SRC_DIR) + string("/resources");
// extern const string _FONTS_PATH_ = string(_RESOURCES_PATH_) + string("/fonts");
// extern const string _TEXTURES_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/textures");

const string _SVM_RESOURCES_PATH_ = string(_RESOURCES_PATH_) + string("/svm");
const string _ARRAYS_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/arrays");
const string _SOURCE_IMAGES_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/images");
const string _MAPPINGS_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/mappings");
const string _MASK_IMAGES_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/masks");
const string _MODELS_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/models");
const string _PIPELINES_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/pipelines");
const string _SETTINGS_PATH_ = string(_SVM_RESOURCES_PATH_) + string("/settings");

const string _LOGS_PATH_ = string(PROJECT_SRC_DIR) + string("/logs");
const string _OUTPUTS_PATH_ = string(PROJECT_SRC_DIR) + string("/outputs");
const string _PERSPECTIVE_PATH_ = string(_OUTPUTS_PATH_) + string("/perspective");


#define CLASS_PTR(klassName) \
class klassName; \
using klassName ## UPtr = std::unique_ptr<klassName>; \
using klassName ## Ptr = std::shared_ptr<klassName>; \
using klassName ## WPtr = std::weak_ptr<klassName>;


typedef enum {
	TOPVIEW3D = 0,
	CAMVIEW3D_FRONT = 1,
	CAMVIEW3D_RIGHT = 2,
	CAMVIEW3D_REAR = 3,
	CAMVIEW3D_LEFT = 4,
	CAMVIEW2D_FRONT = 5,
	CAMVIEW2D_RIGHT = 6,
	CAMVIEW2D_REAR = 7,
	CAMVIEW2D_LEFT = 8,
	CAMVIEW2D_ADD0 = 9,
} VIEWMODE;

typedef enum {
	TOPVIEW_FRONT_3D = 1,
	TOPVIEW_RIGHT_3D = 2,
	TOPVIEW_REAR_3D = 3,
	TOPVIEW_LEFT_3D = 4,
	TOPVIEW_FRONT_2D = 5,
	TOPVIEW_RIGHT_2D = 6,
	TOPVIEW_REAR_2D = 7,
	TOPVIEW_LEFT_2D = 8,
	TOPVIEW_ADD0_2D = 9,
} LAYOUTMODE;


typedef enum {
	FORWARD = 0,
	RIGHT_TURN = 1,
	REVERSE = 2,
	LEFT_TURN = 3,
	EMERGENCY = 4,
} VEHICLESTATE;


struct CAMPARAM         // Intrinsic and extrinsic camera parameters 
{
	Mat K; 				// 3X3 Camera matrix 
	Mat distCoeffs; 	// 10 istortion coefficients (defisheye -> fisheye) for openCV
	Mat rvec;			// 3X1 rotation vector(Rodrigues vector)
	Mat tvec;			// 3x1 translation vector
};

struct PM // projection matrix
{
	glm::mat4 opm; // orthographic
	glm::mat4 ppm; // perspective
};


// enum TURN_SIGNAL { TURN_SIGNAL_OFF = 0, TURN_SIGNAL_LEFT = 1, TURN_SIGNAL_RIGHT = 2, TURN_SIGNAL_EMERGENCY = 3 };
// enum GEAR { GEAR_REVERSE = -1, GEAR_NEUTRAL = 0, GEAR_DRIVING = 1, GEAR_PARKING = 2 };


struct TRIGGER
{
	GEAR gear;
	TURN_SIGNAL turn_signal;
};

struct VEHICLESIGNAL
{
	TRIGGER  m_trigger;
	float m_steering_angle;		//in degree
	float m_wheel_angle;		//in degree
	float m_vehicle_velocity;	// in Km/h
};


// struct TEXTURE_INFO
// {
// 	int width;
// 	int height;
// 	int nchannel;
// 	GLenum color_format;
// 	cv::Mat texture;
// };

// struct SPRITE_INFO
// {
// 	int type;
// 	cv::Point size;
// 	cv::Point position;
// 	TEXTURE_INFO texture_info;
// 	unsigned int texture_id;
// };

enum SPRITE_RENDER_MODE 
{
	SPRITE_DYNAMIC = 0,
	SPRITE_STATIC = 1,
};

// #endif


// template <typename T>
// std::string to_mystring(const T value, const int n = 2, const int w = 4, const char c = '0')
// {
// 	std::ostringstream out;
// 	out << std::fixed << std::setprecision(n) << std::setw(w) << std::setfill(c) << value;
// 	return out.str();
// }

#define noop {}
#define FM(fmu, fmv)  sqrt( (fmu) * (fmu) / 2.0 + (fmv) * (fmv) / 2.0)

#define MIN4(a,b,c,d)  (((a <= b) & (a <= c) & (a <= d)) ? (a) : \
						(((b <= c) & (b <= d)) ? (b) : \
						(((c <= d)) ? (c) : (d))))

#define MAX4(a,b,c,d)  (((a >= b) & (a >= c) & (a >= d)) ? (a) : \
						(((b >= c) & (b >= d)) ? (b) : \
						(((c >= d)) ? (c) : (d))))

#define MAX6(a,b,c,d,e,f)	(((a >= b) & (a >= c) & (a >= d) & (a >= e) & (a >= f)) ? (a) : \
							(((b >= c) & (b >= d) & (b >= e) & (b >= f)) ? (b) : \
							(((c >= d) & (c >= e) & (c >= f)) ? (c) : \
							(((d >= e) & (d >= f)) ? (d) : \
							((e >= f) ? (e) : (f))))))

#define HAVE_TO_REMOVE_IN_RELEASE_VERSION	(1)

#define user_error { throw runtime_error(string("$An user forced an runtime error to occur.")); } 

inline string delimiter(string msg)
{
	string delim;
	if (1 <= msg.size() && msg[0] != '(') delim = (msg[0] == '$') ? " " : " -> ";
	else delim = "";

	return delim + msg;
}


#endif 
