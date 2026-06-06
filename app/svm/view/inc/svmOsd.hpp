#ifndef SVMOSD_HPP_
#define SVMOSD_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmVABT.hpp"
#include "svmShader.hpp"
#include "svmShaderString.hpp"
#include "svmFromFile.hpp"

class sanOsd: public sanVABT
{
public:
	sanXML* m_pxml;

    GLuint m_osd_lut_lines_num = 6;  // for 2D camview
    GLfloat m_osd_lut[30] = { -1.0f,  1.0f,  0.0f,	0.0f, 0.0f,   // p1  top-left
                              -1.0f, -1.0f,  0.0f,	0.0f, 1.0f,   // p2  bottom-left
                               1.0f,  1.0f,  0.0f,	1.0f, 0.0f,   // p3  top-right

                               1.0f,  1.0f,  0.0f,	1.0f, 0.0f,   // p3  top-right
                              -1.0f, -1.0f,  0.0f,	0.0f, 1.0f,   // p2  bottom-left
                               1.0f, -1.0f,  0.0f,	1.0f, 1.0f }; // p4  bottom-right

    sanShader m_osdShader = sanShader(vs_view_osd, NULL, fs_view_osd);

    std::map<std::string, SPRITE_INFO> m_sprites_map;
    std::vector<std::string> m_static_render_list;
    std::map<std::string, float> m_dynamic_render_map;

public:
	sanOsd(sanXML* pxml);
	~sanOsd();

	void initialize();
	void update_dynamic_osd();
	void renderOsd(string sprite_name, float scale = 1.0f);
	void drawLayouts();
    
	void load_osd_data(std::string osd_path);
    TEXTURE_INFO get_osd_texture(std::string name);
};


#endif


