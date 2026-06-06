#ifndef SVMVBC_HPP_
#define SVMVBC_HPP_

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmWorld.hpp"

class sanVBC: public sanVABT  // Vehicle Bottom Coloring
{
public:
	sanXML* m_pxml;
	PM* m_ppm;

	float      m_bottom_color_update_time_sec = 0.50f;
	glm::vec3  m_bottom_default_color = glm::vec3(0.0f, 0.0f, 0.0f);
	float      m_bottom_default_color_alpha = 1.0f;

	vector<Point2f> m_expanded_vehicle_box_on_viewport;
	vector<glm::vec3> m_vehicle_bottom_color_list;
	vector<float>m_vehicle_bottom_color_alpha_list;

	sanShader m_vbcShader = sanShader(vs_view_primitive2d, NULL, fs_view_primitive2d);

public:
	sanVBC(sanXML* pxml, PM* ppm);
	~sanVBC();

	void initialize();
	vector<Point2f> get_expanded_vehicle_box_on_viewport();

	GLfloat* generate_expanded_vehicle_box_mesh(int& total_vertex_num);
	void renderVBC(glm::vec3 color, float alpha, glm::mat4 vcvm);

	void drawLayout0(glm::mat4& vcvm);
	void drawLayout1(glm::mat4& vcvm);
};

#endif




