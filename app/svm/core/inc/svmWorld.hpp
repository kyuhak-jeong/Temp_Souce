#ifndef SVMWORLD_HPP_
#define SVMWORLD_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmCamera.hpp"

#define VEHICLE_BOX_UNIT_MM     (0) // millimeter
#define VEHICLE_BOX_UNIT_LOGIC  (1) // unitless due to normalization


class sanWorld
{
public:
	sanWorld();
	~sanWorld();
	void initialize();

public:
	// space conversion(Fisheye <--> Defisheye <--> NDC <--> LOCAL <--> GLOBAL <--> GL)
	// NDC(Normalized Image Plane) 
	// LOCAL: as the local space, it has different coordinate system depended on cameras
	// GLOBAL: as the global space, equal to openCV coordinate system
	// GL: as the openGL space

	// coordinate coversion
	static Point3f LOCAL_to_GLOBAL(int camID, Point3f local_point);
	static Point3f GLOBAL_to_LOCAL(int camID, Point3f global_point);
	static Point3f GLOBAL_to_GL(Point3f point);
	static Point3f GL_to_GLOBAL(Point3f point);

	static vector<Point3f> LOCAL_to_GL(int camID, vector<Point3f>& local_pt3d /* logic or mm */);
	static vector<Point3f> GL_to_LOCAL(int camID, vector<Point3f>& global_pt3d /* logic or mm*/);

	static vector<Point3f> CV2GL(vector<Point3f>& cv_points); // openCV to open GL
	static vector<Point2f> CV2GL(vector<Point2f>& cv_points);
	static vector<Point2f> GL_to_NDC(glm::mat4 vp, vector<Point3f>& gl_p3d);


	static vector<Point3f> get_Logic_InLOCAL(sanXML* pxml, vector<Point3f>& pt3d_mm);
	static vector<Point3f> get_mm_InLOCAL(sanXML* pxml, vector<Point3f>& pt3d);

	static vector<Point3f> get_Logic_InGLOBAL(sanXML* pxml, vector<Point3f>& pt3d_mm);
	static vector<Point3f> get_mm_InGLOBAL(sanXML* pxml, vector<Point3f>& pt3d);	

	static vector<Point3f> get_full_Logic_InLOCAL(int camID, sanXML* pxml, vector<Point3f>& pt3d_mm); // MilliMeter --> (-1 ~ 1)


	static vector<Point3f> defisheye_to_LOCAL(int camID, sanXML* pxml, vector<Point2f>& defisheye_p2d);
	static Point2f fisheye_to_defisheye(int camID, sanXML* pxml, Point2f& fisheye_p2d);
	static vector<Point2f> fisheye_to_defisheye(int camID, sanXML* pxml, vector<Point2f>& fisheye_p2d);
	static vector<Point3f> fisheye_to_LOCAL(int camID, sanXML* pxml, vector<Point2f>& fisheye_pt2d);
	//static vector<Point3f> fisheye_to_GL(int camID, sanXML* pxml, vector<Point2f>& fisheye_pt2d); // don't use it
	static vector<Point3f> fisheye_to_GL_mm(int camID, sanXML* pxml, vector<Point2f>& fisheye_pt2d);


	static vector<Point2f> LOCAL_to_defisheye(int camID, sanXML* pxml, vector<Point3f>& local_pt3d);
	static Point2f defisheye_to_fisheye(int camID, sanXML* pxml, Point2f& defisheye_p2d);
	static vector<Point2f> defisheye_to_fisheye(int camID, sanXML* pxml, vector<Point2f>& defisheye_p2d);
	static vector<Point2f> LOCAL_to_fisheye(int camID, sanXML* pxml, vector<Point3f>& local_pt3d);
	static vector<Point2f> GL_mm_to_fisheye(int camID, sanXML* pxml, vector<Point3f>& global_p3d_mm);
	static vector<Point2f> GL_mm_to_defisheye(int camID, sanXML* pxml, vector<Point3f>& global_pt3d_mm);
	
	static Point2f normalize_points_in_defisheye(int camID, sanXML* pxml, Point2f defisheye_pt2d);
	static vector<Point2f> normalize_points_in_defisheye(int camID, sanXML* pxml, vector<Point2f>& defisheye_pt2d); // based on CV coordinate syste,

	// for circle mesh
	static float calBaseRadius(sanXML* pxml);
	static Size get_pattern_arrangement_space(int camID, sanXML* pxml);

	static void generate_real_pattern_point_mm_in_GLOBAL(sanXML* pxml, XYZ global_pt[PATTERN_MAX_NUM][PATTERN_POINTS_NUM] /*[out]*/);
	static void get_real_pattern_point_mm_in_PATTERN(int camID, sanXML* pxml, XYZ* pattern_pt3d_mm /*[out]*/);

	// relative to the real vehicle
	static vector<Point3f> get_vehicle_box_InGLOBAL(sanXML* PXML, int vehicle_box_unit);
	static void apply_expanded_distance_to_vehicle_box(sanXML* pxml, vector<Point3f>& vehicle_box, int vehicle_box_unit);
	static vector<float> get_distance_from_vehicle_mm(sanXML* pxml, vector<Point3f>& global_pt3d_mm); // millimeter

	//static bool isInsideZone(vector<Point3f> zone_pt3d_mm, Point3f target_pt3d_mm);

	static bool is_P_InSegment_P0P1(Point3f P, Point3f P0, Point3f P1);
	static bool isInsideZone(vector<Point3f> zone_pt3d_mm, Point3f target_pt3d_mm, bool validBorder = true);

	static bool is_P_InSegment2d_P0P1(Point2f P, Point2f P0, Point2f P1);
	static bool isInsideZone2d(vector<Point2f> zone_pt3d_mm, Point2f target_pt3d_mm, bool validBorder = true);
};

#endif 
