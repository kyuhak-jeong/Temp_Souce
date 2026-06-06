#include "svmTextRenderer.hpp"
#include "svmIO.hpp"

sanTextRenderer::sanTextRenderer(sanXML* pxml)
{
	m_pxml = pxml;
    m_fontSize = 100;
}

sanTextRenderer::~sanTextRenderer()
{
    cleanCharactersMap();
    
    if(m_VAO != 0)
    {
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &m_VAO);
        m_VAO = 0;
    }

    if (m_VBO != 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glDeleteBuffers(1, &m_VBO);
        m_VBO = 0;
    }

    m_textShader.~sanShader();
}

void sanTextRenderer::cleanCharactersMap()
{
    if(!m_characters.empty())
    {
        for (auto const& [key, val] : m_characters)
        {
            if(val.TextureID) glDeleteTextures(1, &val.TextureID);
        }
        m_characters.clear();
    }

    m_characters.clear();
}

void sanTextRenderer::initialize(std::string fontPath, int fontSize)
{
    // configure VAO/VBO for texture quads
    glGenVertexArrays(1, &this->m_VAO);
    glGenBuffers(1, &this->m_VBO);
    glBindVertexArray(this->m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, this->m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 5, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*)(3 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // first clear the previously loaded Characters
    cleanCharactersMap();

    // then initialize and load the FreeType library
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    }

    // load font as face
    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
    }
    else
    {
        // set size to load glyphs as
        m_fontSize = fontSize;
        FT_Set_Pixel_Sizes(face, 0, m_fontSize);
        
        // disable byte-alignment restriction
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // then for the first 128 ASCII characters, pre-load/compile their characters and store them
        for (GLubyte c = 0; c < 128; c++) // lol see what I did there 
        {
            // load character glyph 
            if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            {
                std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
                continue;
            }
            
            unsigned int texture;

            // generate texture
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
            // set texture options
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // now store character for later use
            TextCharacter character = {
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                (unsigned int)face->glyph->advance.x
            };
            m_characters.insert(std::pair<char, TextCharacter>(c, character));
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // destroy FreeType once we're finished
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

glm::vec3 sanTextRenderer::calculateTextSize(const std::string& text, float scale)
{
    std::string::const_iterator c;
    glm::vec2 size = glm::vec2(0.0f, 0.0f);

    for (c = text.begin(); c != text.end(); c++)
    {
        TextCharacter ch = m_characters[*c];
        size.x += (ch.Advance >> 6) * scale;
        size.y = std::max(size.y, ch.Size.y * scale);
    }

    return glm::vec3(std::ceil(size.x), std::ceil(size.y), 0.0f);
}

void sanTextRenderer::renderText(const std::string& text, glm::vec3 pos, TEXT_RENDER_MODE render_mode, glm::mat4 mvp, float scale, glm::vec3 color)
{
    sanError::glClearError();
    
    this->m_textShader.use();
    this->m_textShader.setInt("text", 0);
    this->m_textShader.setVec3("textColor", color);
    this->m_textShader.setMat4("mvp", mvp);

    //glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(this->m_VAO);

    // iterate through all characters
    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); c++)
    {
        TextCharacter ch = m_characters[*c];

        float xpos, ypos, w, h;
        GLfloat* vtx = (GLfloat*)calloc((size_t)(6 * 5), sizeof(GLfloat));

        if (vtx != nullptr)
        {
            if (render_mode == TEXT_RENDER_MODE::TEXT_CV)
            {
                xpos = pos.x + ch.Bearing.x * scale;
                ypos = pos.y + (this->m_characters['H'].Bearing.y - ch.Bearing.y) * scale;

                w = ch.Size.x * scale;
                h = ch.Size.y * scale;

                // update VBO for each character
                vtx[0] = xpos;          vtx[1] = ypos + h;      vtx[2] = 0.0f;      vtx[3] = 0.0f;      vtx[4] = 1.0f;
                vtx[5] = xpos + w;      vtx[6] = ypos;          vtx[7] = 0.0f;      vtx[8] = 1.0f;      vtx[9] = 0.0f;
                vtx[10] = xpos;         vtx[11] = ypos;         vtx[12] = 0.0f;     vtx[13] = 0.0f;      vtx[14] = 0.0f;
                vtx[15] = xpos;         vtx[16] = ypos + h;     vtx[17] = 0.0f;     vtx[18] = 0.0f;      vtx[19] = 1.0f;
                vtx[20] = xpos + w;     vtx[21] = ypos + h;     vtx[22] = 0.0f;     vtx[23] = 1.0f;      vtx[24] = 1.0f;
                vtx[25] = xpos + w;     vtx[26] = ypos;         vtx[27] = 0.0f;     vtx[28] = 1.0f;      vtx[29] = 0.0f;
            }
            else
            {
                xpos = pos.x + ch.Bearing.x * scale;
                ypos = pos.y - (ch.Size.y - ch.Bearing.y) * scale;

                w = ch.Size.x * scale;
                h = ch.Size.y * scale;

                // update VBO for each character
                vtx[0] = xpos;          vtx[1] = ypos + h;      vtx[2] = 0.0f;      vtx[3] = 0.0f;      vtx[4] = 0.0f;
                vtx[5] = xpos;          vtx[6] = ypos;          vtx[7] = 0.0f;      vtx[8] = 0.0f;      vtx[9] = 1.0f;
                vtx[10] = xpos + w;     vtx[11] = ypos;         vtx[12] = 0.0f;     vtx[13] = 1.0f;      vtx[14] = 1.0f;
                vtx[15] = xpos;         vtx[16] = ypos + h;     vtx[17] = 0.0f;     vtx[18] = 0.0f;      vtx[19] = 0.0f;
                vtx[20] = xpos + w;     vtx[21] = ypos;         vtx[22] = 0.0f;     vtx[23] = 1.0f;      vtx[24] = 1.0f;
                vtx[25] = xpos + w;     vtx[26] = ypos + h;     vtx[27] = 0.0f;     vtx[28] = 1.0f;      vtx[29] = 0.0f;
            }
        }
        else
            throw runtime_error(__FUNCTION__ + string(" $failed to allocate memory"));

        // render glyph texture over quad
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        // update content of VBO memory
        glBindBuffer(GL_ARRAY_BUFFER, this->m_VBO);
        //glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 5 * 6, vtx, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        // render quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // now advance cursors for next glyph
        pos.x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (1/64th times 2^6 = 64)

        if(vtx != nullptr) free(vtx);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    //glDisable(GL_CULL_FACE);

    sanError::glCheckError(__FUNCTION__);
}
