#ifndef _SVM_CALIB_GRID_HPP_
#define _SVM_CALIB_GRID_HPP_

#include "svmCore.hpp"
#include "svmCamera.hpp"
#include "svmXML.hpp"
#include "svmCalibCal.hpp"


struct CurvilinearGrid
{
    int actual_circle_steps_num;              // grids information when they ara created.
    int actual_parabola_steps_num;
    int actual_arcs_num;    
 
    vector<Point3f> v3d;	                   // 3D grid vertexes in the local space(XY) based on openCV
    vector<Point2f> p2d;	                   // 2D grid points in defisheye image 2D points)
    vector<Point2f> seam_candidators_defisheye;// seam candidators(8 points) in defisheye image, for debugging
    vector<Point3f> seam_candidators_local;	   // seam candidator(8 points) in local space
};


class sanCalibGrid: public sanVABT
{
public:
    sanXML* m_pxml;
    sanCamera* m_pcameras;
    sanCalibCal* m_pcalibration;

public:
    BOWL3D* m_pgrid_params;
    CurvilinearGrid m_grids[SVM_CAMERAS_NUM];
    float m_base_radius;
    sanShader m_gridShader = sanShader(vs_calib_grid, NULL, fs_calib_grid);
    float* m_gridLUT[SVM_CAMERAS_NUM] = {nullptr};
    int m_vertices_num[SVM_CAMERAS_NUM] = {0};
    bool m_is_vabt_generated[SVM_CAMERAS_NUM] = {false};
    bool m_is_grids_generated[SVM_CAMERAS_NUM] = {false};

public:
    sanCalibGrid(sanXML*pxml, sanCamera* pcameras, sanCalibCal* pcalibration);
    ~sanCalibGrid();
    void initialize();
    void createGrids(int camID);
    void updateGrids(int camID);
    void createGridLUT(int camID);
    void renderGrids(int camID, int glDrawingType = GL_POINTS);

    
    int get_minimal_NOP_Z();
    int get_nop_index_from_bowl(int camID, double circle_radius, double step_length);
    Point3f get_local_threshold_point(int camID);
    void create_curvilinear_grid(double circle_radius, CurvilinearGrid& cg, vector<Point3f>& local_threshold_pt3d);
    void project_pt3d(int camID, CurvilinearGrid& cg);
    void reorganize_grid(int camID, CurvilinearGrid& cg);

    vector<Point3f> get_seam_candidator_p1p8(int camID);
    void find_seam_candidators(int camID, CurvilinearGrid& cg, CurvilinearGrid& cg_p1p8); // 8 points
};


#endif 
