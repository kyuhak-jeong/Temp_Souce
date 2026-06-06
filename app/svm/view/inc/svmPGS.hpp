#ifndef SVMPGS_HPP_
#define SVMPGS_HPP_

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmCamera.hpp"
#include "svmShader.hpp"
#include "svmShaderString.hpp"

class sanPGS : public sanVABT
{
public:
	sanXML* m_pxml;
	PM* m_ppm;
	sanCamera* m_pcameras;
	VEHICLESIGNAL* m_pvehicleSignal;
	sanShader m_pgsShader = sanShader(vs_view_pgs, NULL, fs_view_pgs);

public:
	// curve generation
	vector<Point3f> m_pgs_wheel_box_mm;
	float m_rear_track_width_mm;
	float m_front_track_width_mm;
	float m_wheel_base_mm;

	vector<Point3f> m_front_left_pt3d_mm;
	vector<Point3f> m_front_right_pt3d_mm;
	vector<Point3f> m_rear_left_pt3d_mm;
	vector<Point3f> m_rear_right_pt3d_mm;

	vector<Point3f> m_center_pt3d_mm;
	vector<float> m_vehicle_heading_rad;

	float m_pgs_curve_alpha = 0.80f;
	glm::vec3 m_pgs_curve_color = { glm::vec3(0.90f, 0.90f, 0.00f) };

	unsigned int m_texDriveLeft, m_texDriveRight, m_texReverseLeft, m_texReverseRight;

public:
	sanPGS(sanXML* pxml, PM* ppm, sanCamera* pcameras, VEHICLESIGNAL* pvehicleSignal);
	~sanPGS();

	void initialize();
	void generate_and_update_PGSTexture();

	void generate_wheel_box_mm(vector<Point3f>& wheel_position /*[out]*/, float& rear_track_width_mm /*[out]*/, float& front_track_width_mm/*[out]*/, float& wheel_base_mm/*[out]*/);
	void generate_wheel_trajectory(float vehicle_initial_heading = 90.0f/* degree */);
	void generateLeftRightPoints_3D(vector<Point3f> center_pt3d /*[in]*/, float horizontal_distance /*[in]*/, vector<Point3f>& right_pt3d /*[out]*/, vector<Point3f>& left_pt3d/*[out]*/);

	GLfloat* generatePGSMeshQuad_3D(vector<Point3f> pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/);
	GLfloat* generatePGS_2D(int camID, float pgs_length_mm, vector<Point3f> left_pt3d_mm /*[in]*/, vector<Point3f> right_pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/);
	GLfloat* generatePGSMeshQuad_2D(int camID /*[in]*/, vector<Point3f> pt3d_mm /*[in]*/, int& total_vertex_num /*[out]*/);
	void generate_and_update_PGSMeshQuad_3D();
	void generate_and_update_PGSMeshQuad_2D(int camID);
	
	void renderPGS(int vabt_idx, glm::vec3 color, float alpha, glm::mat4 mvp = glm::mat4(1.0f));
	void renderPGS_2D(int vabt_idx, glm::vec3 color, float alpha);

	void drawLayout0(glm::mat4& vcvm);
	void drawLayout1(glm::mat4& vcvm);

private:
	bool is_2D_VAB_generated = false;
	bool is_3D_VAB_generated = false;

#define PGS_LINE_POINTS_NUM (100)
};

#endif
