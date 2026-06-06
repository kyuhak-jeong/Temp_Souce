#include "svmOsd.hpp"

sanOsd::sanOsd(sanXML* pxml)
{
    m_pxml = pxml;
}

sanOsd::~sanOsd()
{
	for (const auto& sprite : m_sprites_map)
	{
		if(sprite.second.texture_id)
		{
			glDeleteTextures(1, &sprite.second.texture_id);
		}
	}
	if(!m_sprites_map.empty()) m_sprites_map.clear(); else noop;

	if(!m_static_render_list.empty()) m_static_render_list.clear();
	if(!m_dynamic_render_map.empty()) m_dynamic_render_map.clear();

	m_osdShader.~sanShader();
}


void sanOsd::initialize()
{
	load_osd_data(string(_TEXTURES_PATH_) + string("/osd"));
	generateVAB(m_osd_lut, m_osd_lut_lines_num, 3, 2, GL_STATIC_DRAW);
}

void sanOsd::load_osd_data(std::string osd_path)
{
	string osd_list = osd_path + string("/osd_list.txt");
	ifstream data_file(osd_list.c_str());
	int type = 0;
	string name = "";
	cv::Point size, position;

	if(data_file)
	{
		while (data_file >> name >> type >> size.x >> size.y >> position.x >> position.y)
		{
			if (m_sprites_map.find(name) == m_sprites_map.end() && std::strncmp(&name[0],"#",1))
			{
				m_sprites_map[name].type = type;
				m_sprites_map[name].size = size;
				m_sprites_map[name].position = position;
				string file_name = osd_path + string("/") + name + string(".png");
				m_sprites_map[name].texture_info = sanFromFile::read_texture_from_file(file_name);
				if(m_sprites_map[name].texture_info.width == size.x && m_sprites_map[name].texture_info.height == size.y)
				{
					sanVABT::generateTexture(GL_TEXTURE0, &m_sprites_map[name].texture_id);
				}
				else
				{
					string msg = string("$sprite size and image size are mismatch!");
					throw runtime_error(__FUNCTION__ + delimiter(msg));
				}

				if(type == (int)SPRITE_RENDER_MODE::SPRITE_STATIC) m_static_render_list.push_back(name); else noop;

			}
			else noop;
		}
	}
	else
	{
		string msg = string("$Failed to load osd_list.txt!");
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
}

TEXTURE_INFO sanOsd::get_osd_texture(std::string name)
{
	return m_sprites_map[name].texture_info;
}

void sanOsd::update_dynamic_osd()
{
	m_dynamic_render_map.clear();

	string sprite_name;

	// camera status
	switch (m_pxml->m_layout[1].view_mode)
    {
		case VIEWMODE::CAMVIEW2D_FRONT:
		case VIEWMODE::CAMVIEW3D_FRONT:
		{
			sprite_name = string("cam_01");
			break;
		}
		case VIEWMODE::CAMVIEW2D_RIGHT:
		case VIEWMODE::CAMVIEW3D_RIGHT:
		{
			sprite_name = string("cam_02");
			break;
		}
		case VIEWMODE::CAMVIEW2D_REAR:
		case VIEWMODE::CAMVIEW3D_REAR:
		{
			sprite_name = string("cam_03");
			break;
		}
		case VIEWMODE::CAMVIEW2D_LEFT:
		case VIEWMODE::CAMVIEW3D_LEFT:
		{
			sprite_name = string("cam_04");
			break;
		}
		default:
		{
			sprite_name = string("cam_00");
			break;
		}
    }

	m_dynamic_render_map.insert({sprite_name, 1.0f});
}


void sanOsd::renderOsd(string sprite_name, float scale)
{
	SPRITE_INFO sprite = m_sprites_map[sprite_name];

	if(sprite.size.x > 0 && sprite.size.y > 0)
	{
		sanVABT::updateTexture(GL_TEXTURE0, sprite.texture_id, sprite.texture_info);
		m_osdShader.use();
		
		glViewport(sprite.position.x, sprite.position.y, (GLsizei)((float)sprite.size.x*scale), (GLsizei)((float)sprite.size.y*scale));
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
		glBindVertexArray(m_vabt_list[0].vaoID);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_sprites_map[sprite_name].texture_id);
		glUniform1i(glGetUniformLocation(m_osdShader.getProgram(), "img"), 0);
		glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[0].vnum);

		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_BLEND);
	}
	else
	{
		string msg = string("$failed to render sprite: ") + sprite_name;
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
}


void sanOsd::drawLayouts()
{
	for(int i = 0; i < (int)m_static_render_list.size(); i++)
	{
		renderOsd(m_static_render_list[i]);
	}

	for (auto const& dr : m_dynamic_render_map)
	{
		renderOsd(dr.first, dr.second);
	}
}

