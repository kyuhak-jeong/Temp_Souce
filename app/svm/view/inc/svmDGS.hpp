#ifndef _SVMDGS_HPP_   
#define _SVMDGS_HPP_  

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmLogger.hpp"
#include "svmWorld.hpp"

class sanDGS: public sanVABT    // Distance Guide System
{
public:
	PM* m_ppm;
	sanXML* m_pxml;
	TRIGGER* m_ptrigger;

	glm::vec3 m_layout0_color = glm::vec3(1.0f, 1.0f, 1.0f);
	float m_layout0_alpha = 0.120f; 

	glm::vec3 m_layout1_color = glm::vec3(1.0f, 1.0f, 1.0f);
	float m_layout1_alpha = 0.120f;

	sanShader m_dgsShader = sanShader(vs_view_primitive2d, NULL, fs_view_primitive2d);
public:
	sanDGS(sanXML* pxml, PM* ppm, TRIGGER *ptrigger);
	~sanDGS();
	void initialize();

	vector<Point3f> get_horizontal_distance_guide_square(float line_tickness);
	vector<Point3f> get_vertical_distance_guide_square(float line_tickeness);
	vector<Point3f> get_distance_guide_square(float line_thickness);

	GLfloat* generate_LUT(vector<Point3f>& pt3d);
	void renderDGS(int vabt_idx, glm::vec3& color, float alpha, glm::mat4& mvp);
	void drawLayout0(glm::mat4& vcvm);
	void drawLayout1(glm::mat4& vcvm);
};

#endif 
