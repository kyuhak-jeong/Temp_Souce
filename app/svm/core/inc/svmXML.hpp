#ifndef SVMXML_HPP_
#define SVMXML_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "libxml/parser.h"
#include "libxml/xmlmemory.h"
#include "libxml/xmlwriter.h"
#include <ctime>

// struct XY
// {
//     union {
//         float x;
//         float width;
//     };
//     union {
//         float y;
//         float height;
//     };

//     XY(float _x = 0.0f, float _y = 0.0f) : x(_x), y(_y) {}

//     XY operator*(float scalar) const
//     {
//         return XY(x * scalar, y * scalar);
//     }
//     XY operator*(const XY& other) const
//     {
//         return XY(x * other.x, y * other.y);
//     }
//     XY operator+(const XY& other) const
//     {
//         return XY(x + other.x, y + other.y);
//     }
// };

// struct XYZ
// {
//     float x;
//     float y;
//     float z;

//     XYZ(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) : x(_x), y(_y), z(_z) {}

//     XYZ operator*(float scalar) const
//     {
//         return XYZ(x * scalar, y * scalar, z * scalar);
//     }
//     XYZ operator*(const XYZ& other) const
//     {
//         return XYZ(x * other.x, y * other.y, z * other.z);
//     }
//     XYZ operator+(const XYZ& other) const
//     {
//         return XYZ(x + other.x, y + other.y, z + other.z);
//     }
// };

struct OFFSET
{
    float hleft;
    float hright;
    float vtop;
    float vbot;
};

struct RCAM_PARAMETERS
{
    double brightness;
    bool flipx;
    OFFSET camview_offset;

    double sf;
    double cx;
    double cy;
    double aff[AFFINE_COEF_NUM];
    double invpol[INV_POLY_COEF_MAX_NUM];
    double pol[POLY_COEF_MAX_NUM];
};

struct POSE
{
    glm::vec3 pos;
    glm::vec3 ori;
};

struct LAYOUT
{
    int x;
    int y;
    int width;
    int height;
    int view_mode;
};

struct VIEWSETTING
{
    int default_view;
    int rear_gear;
    int left_turn;
    int right_turn;
    int emergency;
};

struct ORTHOGRAPHIC
{
	float left;
	float right;
	float bottom;
	float top;
	float znear;
	float zfar;
};

struct PERSPECTIVE
{
	float fov;
	float znear;
	float zfar;
};

struct RESOLUTION
{
    XY image;
    XY display;
};

struct CONTOUR
{
    int roi_start_x;
    int roi_start_y;
    int roi_width;
    int roi_height;
    int contour_max_area;
};

struct BOWL3D
{
	int arcs_num;
    int start_arc_index;
    int start_step_index;
    float step_length;
	int nop_z;
	float base_radius_times;
	float smooth_angle;
};

struct MODEL
{
    string model_file;
    XYZ translation;
    XYZ rotation;
    XYZ scale;
};

struct VEHICLE_INFORMATION
{
    string vehicle_id;          // xx_xxx_xx_xx_xxx : manufacturer code / model code / year / vehicle_type code / customer code
    string customer;
    string manufacturer;
    string model_name;
    string model_year;
    string vehicle_type;
};

struct VEHICLE_SPECIFICATION
{
    float length;
    float width;
    float height;
    float front_overhang;
    float wheel_base;
    float rear_overhang;
    float front_track;
    float rear_track;
    float min_steering_angle;
    float max_steering_angle;
};

struct SVMPATTERN
{
    float unit_space;
};

struct DISTANCE_BETWEEN_PATTERNS
{
    float vertical_1st_distance;
    float vertical_2nd_distance;
    float vertical_3rd_distance;
};

struct PATTERN_OFFSET_FROM_CAR
{
    float front_pattern_offset;
    float right_pattern_offset;
    float rear_pattern_offset;
    float left_pattern_offset;
};

struct POSTER
{
    float width;
    float height;
    float radius_scale;
};

struct ARRANGEMENT
{
    PATTERN_OFFSET_FROM_CAR  pattern_offset_from_car;
    DISTANCE_BETWEEN_PATTERNS distance_between_patterns;
    POSTER poster;
};

struct ACTIVATION
{
    bool scene_animation;
    bool model_animation;
    bool model_transparency;
    bool vbc;
	bool pgs;
	bool dgs;
	bool od;
    bool mobs;
};

struct SCENE_ANIMATION_PARAMETER
{
    int scene_animation_type;
};

struct MODEL_ANIMATION_PARAMETER
{
   
};

struct MODEL_TRANSPARENCY_PARAMETER
{
    float rate;
};

struct MODEL_LAMP_PARAMETER
{
    vector<string> left_lamp_names;
    vector<string> right_lamp_names;
};

struct VBC_PARAMETER
{
    float front_expanded_distance_mm;
    float right_expanded_distance_mm;
    float rear_expanded_distance_mm;
    float left_expanded_distance_mm;
};


struct PGS_PARAMETER
{
    float guide_max_distance;
    float rear_1st_distance;
    float rear_2nd_distance;
    float rear_3rd_distance;
    float unit_sample_width_mm;
    float unit_sample_length_mm;
};

struct DGS_PARAMETER
{
	float monitoring_distance;
	float unit_square_length;
    float layout0_line_thickness;
    float layout1_line_thickness;
};

struct OD_PARAMETER
{
    float warning_min_distance;
    float warning_max_distance;
    float circle_target_radius;
    float target_gradient_time_msec;
    bool info_enable;
    string info_font_name;
    int info_font_size;
    float info_scale_2d;
    float info_scale_3d;
    int coast_cycles_threshold;
    int min_hit_streak;
    float duplicate_object_iou_threshold;
};

struct MOBS_PARAMETER
{
    float mois_speed_min;
    float mois_speed_max;
    float mois_ttc_warning;
    float mois_ttc_monitoring;
    float mois_dist_warning_head;
    float mois_dist_warning_side;
    float mois_dist_monitoring_head;
    float mois_dist_monitoring_side;

    float bsis_speed_min;
    float bsis_speed_max;
    float bsis_lca_speed_min;
    float bsis_ttc_warning;
    float bsis_ttc_monitoring;
    float right_bsis_dist_warning_head;
    float right_bsis_dist_warning_side;
    float right_bsis_dist_warning_tail;
    float right_bsis_dist_monitoring_head;
    float right_bsis_dist_monitoring_side;
    float right_bsis_dist_monitoring_tail;
    float left_bsis_dist_warning_head;
    float left_bsis_dist_warning_side;
    float left_bsis_dist_warning_tail;
    float left_bsis_dist_monitoring_head;
    float left_bsis_dist_monitoring_side;
    float left_bsis_dist_monitoring_tail;

    float reverse_speed_min;
    float reverse_speed_max;
    float reverse_ttc_warning;
    float right_rear_dist_warning_side;
    float right_rear_dist_warning_tail;
    float left_rear_dist_warning_side;
    float left_rear_dist_warning_tail;
    float center_rear_dist_warning_side;
    float center_rear_dist_warning_near_tail;
    float center_rear_dist_warning_far_tail;
};

struct CALIBRATED_PARAMETER
{
    double K[9];  // 3x3 camera matrix 
    double ext[6];// extrinsic parameter
};

struct CALIBRATED_STATUS
{
    int calibration_done;
    string completion_time;
};

struct WRITING_PARAMETERS
{
    int    feature_points[2 * CONTROL_POINTS_NUM];
    int    local_points[3 * CONTROL_POINTS_NUM];
    double calibrated_parameter[15];
    vector<float> poster_size;
    vector<string> calibrated_status;
};


class sanXML
{
public:
    string m_settings_common_filePath;
    string m_settings_calibration_filePath;
    string m_settings_view_filePath;
    string m_output_calibration_filePath;
    string m_output_calibration_result_image_filePath;

    string m_xml_file_version;            // setting file version
    string m_device_id;                   // device's ID: xx_xx_xxxx_xxx : country code / year / device code / hash?
    string m_software_version;            // device's software version
    string n_server_ip;                   // device's server IP

    int m_svm_cameras_num;                // number of cameras for SVM
    int m_svm_patterns_num;               // number of patterns for calibration
	int m_calibration_type;               // calibration method

    RESOLUTION m_resolution;              // resolution
    RCAM_PARAMETERS m_rcam[SVM_CAMERAS_NUM]; // real camera
    RCAM_PARAMETERS m_acam[ADD_CAMERAS_NUM]; // real additional camera

    SVMPATTERN m_pattern;                 // pattern
    ARRANGEMENT m_arrangement;            // information about arrangement of patterns
    LAYOUT m_layout[2];                   // layout_view
    VIEWSETTING m_view_setting;           // view setting
	ORTHOGRAPHIC m_orthogrphic;           // orthographic projection param
	PERSPECTIVE m_perspective;            // perspective projection param
	CONTOUR m_contour;                    // search pattern contour
	BOWL3D m_grid;                        // grid bowl
    POSE m_vcam[11]; 	                  // virtual_camera(front:0, right: 1, rear: 2, left: 3, top: 10)
    MODEL  m_model;                       // 3D car model
    VEHICLE_INFORMATION m_vehicle_info;   // vehicle information
    VEHICLE_SPECIFICATION m_vehicle_spec; // vehicle specification
	ACTIVATION m_activation;              // function activation
    SCENE_ANIMATION_PARAMETER       m_scene_animation_parameter;
    MODEL_ANIMATION_PARAMETER       m_model_animation_parameter;
    MODEL_TRANSPARENCY_PARAMETER    m_model_transparency_parameter;
    MODEL_LAMP_PARAMETER            m_model_lamp_parameter;
    PGS_PARAMETER m_pgs_parameter;        //
	DGS_PARAMETER m_dgs_parameter;        // front observation scope
    OD_PARAMETER m_od_parameter;          // warning distance for OD 
    MOBS_PARAMETER m_mobs_parameter;
    VBC_PARAMETER m_vbc_parameter;

    // calibrated parameter
    XYZ m_global_pts[PATTERN_MAX_NUM][PATTERN_POINTS_NUM];
    XYZ m_local_pts[SVM_CAMERAS_NUM][CONTROL_POINTS_NUM];           // real_local_points
    XY m_feature_pts[SVM_CAMERAS_NUM][CONTROL_POINTS_NUM];          // feature_points
    CALIBRATED_PARAMETER m_calibrated_parameters[SVM_CAMERAS_NUM];  // calibrated result
    CALIBRATED_STATUS m_calibrated_status;

    // mask image seam contour points
    vector<vector<Point2f>> m_mask_seam;

public:
    sanXML();
    ~sanXML();
    void initialize();
    
    void calPoster();
    
    void calcLocalPoints();
    void loadMaskSeam();

public:
    void xmlLoad(string filePath);

    void convertStringToDatatype(char* name, int code, char* str_val);
    int getCodeFromName(const char* name);

    void read_bool(char* tag, char* src, bool* dst);
    void read_int(char* tag, char* src, int* dst);
    void read_uint(char* tag, char* src, unsigned int* dst);
    void read_float(char* tag, char* src, float* dst);
    void read_xy(char* tag, char * src, XY * dst);
	void read_xyz(char*tag, char * src, XYZ * dst);
    void read_rcam_parameters(char* tag, char* src, RCAM_PARAMETERS* dst);
    void read_vcam_pose(char* tag,  char* src, POSE* dst);
    void read_distance_between_patterns(char* tag, char* src, DISTANCE_BETWEEN_PATTERNS* dst);
    void read_poster(char* tag, char* src, POSTER* dst);

    void read_pattern_offset_from_car(char* tag, char* src, PATTERN_OFFSET_FROM_CAR* dst);

    void read_layout(char* tag, char* src, LAYOUT* dst);
    void read_view_setting(char* tag, char* str_val, VIEWSETTING* dst);
	void read_projection_orthographic(char* tag, char* src, ORTHOGRAPHIC* dst);
	void read_projection_perspective(char* tag,  char* src, PERSPECTIVE* dst);
    void read_calibrated_parameters(char* tag, char* src, CALIBRATED_PARAMETER* dst);

    void read_calibrated_status(char* tag, char* src, CALIBRATED_STATUS* dst);

	void read_contour(char*tag, char* src, CONTOUR* dst);
	void read_bowl3d(char* tag, char * src, BOWL3D* dst);

    void read_vehicle_information(char* tag, char* src, VEHICLE_INFORMATION* dst);
	void read_vehicle_specification(char* tag, char * src, VEHICLE_SPECIFICATION* dst);
	void read_activation(char* tag, char* src, ACTIVATION* dst);

    void read_scene_animation_parameter(char* tag, char* src, SCENE_ANIMATION_PARAMETER* dst);
    void read_model_animation_parameter(char* tag, char* src, MODEL_ANIMATION_PARAMETER* dst);
    void read_pgs_parameter(char* tag, char* src, PGS_PARAMETER* dst);
    void read_dgs_parameter(char* tag, char* src, DGS_PARAMETER* dst);
    void read_vbc_parameter(char* tag, char* src, VBC_PARAMETER* dst);

    void read_model_lamp_parameter(char* tag, char* src, MODEL_LAMP_PARAMETER* dst);
    void read_od_parameter(char* tag, char* src, OD_PARAMETER* dst);
    void read_mobs_parameter(char* tag, char* src, MOBS_PARAMETER* dst);
    void read_local_points(char* tag, char* src, XYZ* dst, int iter);
    void read_feature_points(char* tag, char* src, XY* dst, int iter);

    void format_feature_points_for_writing(XY* points/*in*/, int* data/*[out]*/);
    void format_local_points_for_writing(XYZ* points /*[in]*/, int* data /*[out]*/); 
    void format_calibrated_parameters_for_writing(CALIBRATED_PARAMETER& calibrated_param, double* data /*[out]*/);

    void xmlUpdateCache(xmlDocPtr xml_doc_ptr, WRITING_PARAMETERS& wp, string level3_node_basename);
    void create_output_calibration_file(string file_path);

    void check_range_all_parameters();

    bool check_range_system_parameters();
    string check_range_resolution_parameters();
    bool check_range_real_vehicle_parameters();

    bool check_range_calibration_parameters();
    bool check_range_image_processing_parameters();
    bool check_range_space_grid_parameters();
    bool check_range_unit_pattern_parameters();
    bool check_range_arrangement_parameters();
    bool check_range_feature_points_parameters();

    bool check_range_virtual_camera_parameters();
    bool check_range_projection_parameters();
    bool check_range_layout_view_parameters();
    bool check_range_camview_parameters();
    bool check_range_vehicle_model_parameters();
    bool check_range_function_parameters();
    string check_range_activation_parameters();
    string check_range_scene_animation_parameters();
    string check_range_model_animation_parameters();
    string check_range_model_transparency_parameters();
    string check_range_model_lamp_parameters();
    string check_range_vbc_parameters();
    string check_range_pgs_parameters();
    string check_range_dgs_parameters();
    string check_range_od_parameters();
    string check_range_mobs_parameters();

};
#endif
