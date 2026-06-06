#ifndef SVMOD_HPP_
#define SVMOD_HPP_


#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmCamera.hpp"
#include "svmCamvec.hpp"
#include "svmMOBS.hpp"
#include "svmShader.hpp"
#include "svmTracker.h"
#include "svmTextRenderer.hpp"

#define NUM_POINTS_PER_RECTANGLE			(4)
#define NUM_LINE_POINTS_PER_RECTANGLE		(8)
#define NUM_TRIANGLE_POINTS_PER_RECTANGLE	(6)

#define CIRCLE_TARGERT_GRADIENT_STEPS		(20)
#define CIRCLE_TARGET_DIVISION_NUM			(15)

#define PRIMARY_ZONE						(0)
#define SECONDARY_ZONE						(1)


enum ZONE_TYPE {OUT_OF_ZONE = 0,
				MOIS_WARNING_ZONE = 1,		
				MOIS_MONITORING_ZONE = 2,
				RIGHT_BSIS_TOP_WARNING_ZONE = 3,
				RIGHT_BSIS_BOT_WARNING_ZONE = 4,
				RIGHT_BSIS_TOP_MONITORING_ZONE = 5,	
				RIGHT_BSIS_MID_MONITORING_ZONE = 6,
				RIGHT_BSIS_BOT_MONITORING_ZONE = 7,
				LEFT_BSIS_TOP_WARNING_ZONE = 8,
				LEFT_BSIS_BOT_WARNING_ZONE = 9,
				LEFT_BSIS_TOP_MONITORING_ZONE = 10,	  
				LEFT_BSIS_MID_MONITORING_ZONE = 11,
				LEFT_BSIS_BOT_MONITORING_ZONE = 12,
				RIGHT_REAR_WARNING_ZONE = 13,
				LEFT_REAR_WARNING_ZONE = 14,
				CENTER_REAR_WARNING_ZONE = 15,
				OD_WARNING_ZONE = 16};

enum ALERT_TYPE { NO_ALERT = 0,	
	              ALERT_LEVEL1 = 1,	
	              ALERT_LEVEL2 = 2,	
	              ALERT_OD = 3,
				  ALERT_LCA = 4};


struct SVM_OBJECT
{
	// inferencing information about an object
	int			camID;                          // camera ID(front: 0, right:1, rear: 2, left: 3)
	int			classID;                        // class ID(pedestrian: 0, vehicle: 1, cycle(cyclist): 2)
	int			objectID;                       // 1-based integer
	int			x;                              // the horizontal coordinate of the starting point of a detected rectangle, based on the fisheye image, in pixel
	int			y;                              // the vertical coordinate of the starting point of a detected rectangle, based on the fisheye image, in pixel 
	int			width;                          // width of the rectangle, in pixel
	int			height;                         // height of the rectangle, in pixel
	clock_t     timestamp;					    // system clock time, in millisecond
	int         coast_cycles;                   // counter of an undetected object after detected once
	int         hit_streak;                     // counter of a continual dectected object

	// basic information about an object
	float		gx;							    // normalized x coordinate based on GL coordinate system, with min_distance, no unit, for drawing
	float		gy;							    // normalized y coordinate based on GL coordinate system, with min_distance, no unit, for drawing 
	float		gz;							    // normalized z coordinate based on GL coordinate system, with min distance, no unit, for drawing
	float		xmm;						    // real x coordinate based on GL coordinate system, with min_distance, in millimeter
	float		ymm;						    // real y coordinate based on GL coordinate system, with min_distance, in millimeter
	float		zmm;						    // real z coordinate based on GL coordinate system, with min_distance, in millimeter
	float		min_distance_mm_from_car;	    // minimal distance from the edge of the real car, in millimeter
	ZONE_TYPE   primary_zone_type;              // type of zones for MOIS, BSIS and OD
	ZONE_TYPE   secondary_zone_type;            // type of zones for MOIS, BSIS and OD

	// extended information about the amount of change of an object between two frames
	float		delta_xmm;                      // displacement of xmm between two frames, in millimeter
	float		delta_ymm;                      // displacement of ymm between two frames, in millimeter
	float		delta_zmm;                      // displacement of zmm between two frames, in millimeter 	
	clock_t		delta_timestamp;				// difference of timestamp between two frames, in millisecond
	
	Point3f     object_moving_direction;	    // unit vector, based on GL coordinate system
	float       object_moving_speed;            // Km/h
	float       TTC;					        // Time To Collision, in second
	float		estimated_object_speed;			// syan absolute speed
	ALERT_TYPE  alert_level; 	                // warning information

	// member variables for internal processing	
	float       previous_TTC;
	float       previous_object_speed;

	vector<Point3f>		bbox_pt3d_mm;           // to remove duplicate objects in an overlap region of two cameras

	SVM_OBJECT(int _camID = 0, int _classID = 0, int _objectID = 0,
				int _x = 0, int _y = 0, int _width = 0, int _height = 0, clock_t _timestamp = 0, int _coast_cycles = 0, int _hit_streak = 0,
				float _gx = 0.0f, float _gy = 0.0f, float _gz = 0.0f,
				float _xmm = 0.0f, float _ymm = 0.0f, float _zmm = 0.0f,
				float _min_distance_mm_from_car = 0.0f, ZONE_TYPE _primary_zone_type = OUT_OF_ZONE, ZONE_TYPE _secondary_zone_type = OUT_OF_ZONE,
				float _delta_xmm = 0.0f, float _delta_ymm = 0.0f, float _delta_zmm = 0.0f,
				clock_t _delta_timestamp = 0, Point3f _object_moving_direction = Point3f(0.0f, 0.0f, 0.0f), float _object_moving_speed = 0.0f, float _TTC = INFINITY, float _estimated_object_speed = 0.0f,
				ALERT_TYPE _alert_level = NO_ALERT, float _previous_TTC = INFINITY, float _previous_object_speed = 0.0f) :
															camID(_camID), classID(_classID), objectID(_objectID),
															x(_x), y(_y), width(_width), height(_height), timestamp(_timestamp), coast_cycles(_coast_cycles), hit_streak(_hit_streak),
															gx(_gx), gy(_gy), gz(_gz), xmm(_xmm), ymm(_ymm), zmm(_zmm), 
															min_distance_mm_from_car(_min_distance_mm_from_car), primary_zone_type(_primary_zone_type), secondary_zone_type(_secondary_zone_type),
															delta_xmm(_delta_xmm), delta_ymm(_delta_ymm), delta_zmm(_delta_zmm), delta_timestamp(_delta_timestamp),
															object_moving_direction(_object_moving_direction), object_moving_speed(_object_moving_speed), TTC(_TTC), estimated_object_speed(_estimated_object_speed), alert_level(_alert_level),
															previous_TTC(_previous_TTC), previous_object_speed(_previous_object_speed) { };
};


class sanOD: public sanVABT
{
public:
	PM* m_ppm;
	sanXML* m_pxml;
	sanCamera* m_pcameras;
	sanMOBS* m_pmobs;
	sanTextRenderer* m_ptextRenderer;
	VEHICLESIGNAL *m_pvehicle_signal;

	GLuint m_c2d_lut_lines_num;
	glm::vec3 m_color_pallet[5] = { glm::vec3(1.00f, 0.00f, 0.00f),		// RED		PERSON
									glm::vec3(0.00f, 1.00f, 0.00f),		// GREEN	CAR
									glm::vec3(1.00f, 1.00f, 0.00f) };   // YELLOW	BICYCLE}

	vector<Point3f> m_circle_target_shape;
	float m_circle_target_gradient_unit_time_msec;
	
	sanShader m_odShader = sanShader(vs_view_primitive2d, NULL, fs_view_primitive2d);
	sanTracker* m_ptracker = nullptr;

	map<int, SVM_OBJECT> m_previous_frame_pool;

	int m_totalObjectCount;
	int m_ttcViolatedCount;

public:
	sanOD(sanXML* pxml, PM* ppm, sanCamera* pcameras, sanMOBS* pmobs, sanTextRenderer* ptextRenderer, VEHICLESIGNAL* pvehicleSignal);
	~sanOD();

	void initialize();
	void generateCircleTarget(Point3f center);
	vector<Point2f> convert_object_to_point(SVM_OBJECT& one_object);
	vector<Point2f> pickout_candidates_with_min_distance(SVM_OBJECT& one_od);

	vector<Point2f> makeTargetMesh_2D(vector<Point2f> rect_points);
	GLfloat* getTargetMesh_2D(int camID, SVM_OBJECT& one_object, int& total_vertex_num);
	void renderOD_2D(int vabt_idx, glm::vec3& color, float alpha, glm::mat4 mvp);

	float calcTTC_in_Front(SVM_OBJECT& object);
	float calcTTC_in_Rear(SVM_OBJECT& object);
	float calcTTC_on_Left(SVM_OBJECT& object);
	float calcTTC_on_Right(SVM_OBJECT& object);
	float calcTTC(SVM_OBJECT& object);


	GLfloat* getTargetMesh_3D(Point3f& center);
	void renderOD_3D(int vabt_idx, glm::vec3& color, float alpha, glm::mat4 mvp);

	vector<ZONE_TYPE> getObjectZoneType(MOBS_MARKERS* markers, Point3f object_pt3d_mm, float distance_mm_from_car);
	void extractBasicInformationFromObject(vector<SVM_OBJECT>& object_list/*[in/out]*/);
	void extractExtendedInformationFromSameObjectBetweenFrames(SVM_OBJECT& prev_object /*[in]*/, SVM_OBJECT& curr_object/*[in/out]*/);
	vector<SVM_OBJECT> extractObjectInformation(vector<SVM_OBJECT>& frame_objects);

	// vector<int>  Get_Camid_OrgCoords(EXT_OBJECT quad_object);
	vector<SVM_OBJECT> convertQuadrantCoords(vector<EXT_OBJECT>& frame_extened_objects);
	vector<SVM_OBJECT> applyAlertPolicy(vector<SVM_OBJECT>& target_objects);
	vector<SVM_OBJECT> removeDuplicateObjects(vector<SVM_OBJECT>& target_objects /*[in], [out]*/, float iou_threshold /*[in]*/);

	void drawLayout0(glm::mat4& vcvm, vector<SVM_OBJECT>& alert_objects);
	void drawLayout1_3D(glm::mat4& vcvm, vector<SVM_OBJECT>& alert_objects);
	void drawLayout1_2D(vector<SVM_OBJECT>& alert_objects);
};

#endif

