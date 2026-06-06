#ifndef SVMMOBS_HPP_
#define SVMMOBS_HPP_

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmCamera.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmLogger.hpp"
#include "svmWorld.hpp"
#include "svmPGS.hpp"

struct MOBS_MARKERS
{
	~MOBS_MARKERS()
	{
		if (!mois_warning_pt3d_mm.empty())
		{
			mois_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(mois_warning_pt3d_mm);
		}
		else noop;
		if (!mois_monitoring_pt3d_mm.empty()) 
		{
			mois_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(mois_monitoring_pt3d_mm);
		}
		else noop;

		if (!right_bsis_top_warning_pt3d_mm.empty()) 
		{
			right_bsis_top_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(right_bsis_top_warning_pt3d_mm);
		}
		else noop;
		if (!right_bsis_bot_warning_pt3d_mm.empty()) 
		{
			right_bsis_bot_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(right_bsis_bot_warning_pt3d_mm);
		}
		else noop;
		if (!right_bsis_top_monitoring_pt3d_mm.empty()) 
		{
			right_bsis_top_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(right_bsis_top_monitoring_pt3d_mm);
		}
		else noop;
		if (!right_bsis_mid_monitoring_pt3d_mm.empty()) 
		{
			right_bsis_mid_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(right_bsis_mid_monitoring_pt3d_mm);
		}
		else noop;
		if (!right_bsis_bot_monitoring_pt3d_mm.empty()) 
		{
			right_bsis_bot_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(right_bsis_bot_monitoring_pt3d_mm);
		}
		else noop;

		if (!left_bsis_top_warning_pt3d_mm.empty()) 
		{
			left_bsis_top_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(left_bsis_top_warning_pt3d_mm);
		}
		else noop;
		if (!left_bsis_bot_warning_pt3d_mm.empty()) 
		{
			left_bsis_bot_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(left_bsis_bot_warning_pt3d_mm);
		}
		else noop;
		if (!left_bsis_top_monitoring_pt3d_mm.empty()) 
		{
			left_bsis_top_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(left_bsis_top_monitoring_pt3d_mm);
		}
		else noop;
		if (!left_bsis_mid_monitoring_pt3d_mm.empty()) 
		{
			left_bsis_mid_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(left_bsis_mid_monitoring_pt3d_mm);
		}
		else noop;
		if (!left_bsis_bot_monitoring_pt3d_mm.empty()) 
		{
			left_bsis_bot_monitoring_pt3d_mm.clear(); 
			vector<Point3f>().swap(left_bsis_bot_monitoring_pt3d_mm);
		}
		else noop;

		if (!right_rear_warning_pt3d_mm.empty()) 
		{
			right_rear_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(right_rear_warning_pt3d_mm);
		}
		else noop;
		if (!left_rear_warning_pt3d_mm.empty()) 
		{
			left_rear_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(left_rear_warning_pt3d_mm);
		}
		else noop;
		if (!center_rear_warning_pt3d_mm.empty()) 
		{
			center_rear_warning_pt3d_mm.clear(); 
			vector<Point3f>().swap(center_rear_warning_pt3d_mm);
		}
		else noop;
	}

	vector<Point3f> mois_warning_pt3d_mm;
	vector<Point3f> mois_monitoring_pt3d_mm;

	vector<Point3f> right_bsis_top_warning_pt3d_mm;
	vector<Point3f> right_bsis_bot_warning_pt3d_mm;
	vector<Point3f> right_bsis_top_monitoring_pt3d_mm;
	vector<Point3f> right_bsis_mid_monitoring_pt3d_mm;
	vector<Point3f> right_bsis_bot_monitoring_pt3d_mm;

	vector<Point3f> left_bsis_top_warning_pt3d_mm;
	vector<Point3f> left_bsis_bot_warning_pt3d_mm;
	vector<Point3f> left_bsis_top_monitoring_pt3d_mm;
	vector<Point3f> left_bsis_mid_monitoring_pt3d_mm;
	vector<Point3f> left_bsis_bot_monitoring_pt3d_mm;

	vector<Point3f> right_rear_warning_pt3d_mm;
	vector<Point3f> left_rear_warning_pt3d_mm;
	vector<Point3f> center_rear_warning_pt3d_mm;
};


class sanMOBS : public sanVABT
{
public:
	sanXML* m_pxml;
	PM* m_ppm;
	sanCamera* m_pcameras;
	sanPGS* m_ppgs;
	VEHICLESIGNAL* m_pvehicleSignal;
	MOBS_MARKERS* m_pmarkers;

	vector<Point3f> m_center_pt3d_mm;
	vector<float> m_vehicle_heading_rad;

	glm::vec3 m_mobs_color = glm::vec3(1.00f, 1.00f, 0.00f);

	// partial colors
	glm::vec3 m_mois_warning_color = glm::vec3(0.83f, 0.34f, 0.16f);
	float m_mois_warning_alpha = 0.25f;
	glm::vec3 m_mois_monitoring_color = glm::vec3(0.92f, 0.83f, 0.31f);
	float m_mois_monitoring_alpha = 0.15f;

	glm::vec3 m_bsis_warning_color = glm::vec3(0.83f, 0.34f, 0.16f);
	float m_bsis_warning_alpha = 0.25f;
	glm::vec3 m_bsis_top_monitoring_color = glm::vec3(0.85f, 0.72f, 0.30f);
	float m_bsis_top_monitoring_alpha = 0.15f;
	glm::vec3 m_bsis_bot_monitoring_color = glm::vec3(0.30f, 0.77f, 0.34f);
	float m_bsis_bot_monitoring_alpha = 0.15f;

	glm::vec3 m_rear_warning_color = glm::vec3(0.83f, 0.34f, 0.16f);
	float m_rear_warning_alpha = 0.25f;

	sanShader m_mobsShader = sanShader(vs_view_primitive2d, NULL, fs_view_primitive2d);

public:
	sanMOBS(sanXML* pxml, PM* ppm, sanCamera* pcameras, sanPGS* ppgs, VEHICLESIGNAL* pvehicleSignal);
	~sanMOBS();

	void initialize();

	void renderMOBS(int vabt_idx, glm::vec3 color, float alpha, glm::mat4 mvp);
	void drawLayout0(glm::mat4& view_matrix);
	void drawLayout1(glm::mat4& view_matrix);

	void generate_and_update_MOBSMesh_3D();
	void generate_and_update_MOBSMesh_2D(int camID);

	GLfloat* generate_3D_LUT(vector<Point3f> borders_pt3d_mm, int& total_vertex_num /*out*/);
	vector<Point3f> reorganizeBorders(int camID, vector<Point3f> borders_pt3d_mm);
	GLfloat* generate_2D_LUT(int camID, vector<Point3f> borders_pt3d_mm, int& total_vertex_num /*out*/);

	void get_mois_guide(vector<Point3f>& mois_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& mois_monitoring_pt3d_mm /*[out]*/);
	void get_bsis_guide(vector<Point3f>& right_bsis_top_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& right_bsis_bot_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& right_bsis_top_monitoring_pt3d_mm /*[out]*/,\
						vector<Point3f>& right_bsis_mid_monitoring_pt3d_mm /*[out]*/,\
						vector<Point3f>& right_bsis_bot_monitoring_pt3d_mm /*[out]*/,\
						vector<Point3f>& left_bsis_top_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& left_bsis_bot_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& left_bsis_top_monitoring_pt3d_mm /*[out]*/,\
						vector<Point3f>& left_bsis_mid_monitoring_pt3d_mm /*[out]*/,\
						vector<Point3f>& left_bsis_bot_monitoring_pt3d_mm /*[out]*/);
	void get_rear_guide(vector<Point3f>& right_rear_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& left_rear_warning_pt3d_mm /*[out]*/,\
						vector<Point3f>& center_rear_warning_pt3d_mm /*[out]*/);
	void generate_trajectory(float vehicle_initial_heading /* [in] degree */, float distance_travel_mm /* [in] mm */);

private:
	bool is_2D_VAB_generated = false;
	bool is_3D_VAB_generated = false;
	float m_max_distance = 0.0f;

	enum MOBS_VAB_IDX {
		MOIS_WARNING_3D = 0,
		MOIS_MONITORING_3D = 1,
		RIGHT_BSIS_TOP_WARNING_3D = 2,
		RIGHT_BSIS_BOT_WARNING_3D = 3,
		RIGHT_BSIS_TOP_MONITORING_3D = 4,
		RIGHT_BSIS_MID_MONITORING_3D = 5,
		RIGHT_BSIS_BOT_MONITORING_3D = 6,
		LEFT_BSIS_TOP_WARNING_3D = 7,
		LEFT_BSIS_BOT_WARNING_3D = 8,
		LEFT_BSIS_TOP_MONITORING_3D = 9,
		LEFT_BSIS_MID_MONITORING_3D = 10,
		LEFT_BSIS_BOT_MONITORING_3D = 11,
		RIGHT_REAR_WARNING_3D = 12,
		LEFT_REAR_WARNING_3D = 13,
		CENTER_REAR_WARNING_3D = 14,

		MOIS_WARNING_2D = 15,
		MOIS_MONITORING_2D = 16,
		RIGHT_BSIS_TOP_WARNING_2D = 17,
		RIGHT_BSIS_BOT_WARNING_2D = 18,
		RIGHT_BSIS_TOP_MONITORING_2D = 19,
		RIGHT_BSIS_MID_MONITORING_2D = 20,
		RIGHT_BSIS_BOT_MONITORING_2D = 21,
		LEFT_BSIS_TOP_WARNING_2D = 22,
		LEFT_BSIS_BOT_WARNING_2D = 23,
		LEFT_BSIS_TOP_MONITORING_2D = 24,
		LEFT_BSIS_MID_MONITORING_2D = 25,
		LEFT_BSIS_BOT_MONITORING_2D = 26,
		RIGHT_REAR_WARNING_2D = 27,
		LEFT_REAR_WARNING_2D = 28,
		CENTER_REAR_WARNING_2D = 29,
	};
};

#endif
