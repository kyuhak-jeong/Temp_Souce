#ifndef SVMTEXTRENDERER_HPP_
#define SVMTEXTRENDERER_HPP_

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"
#include "svmError.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

/// Holds all state information relevant to a character as loaded using FreeType
struct TextCharacter {
    unsigned int TextureID; // ID handle of the glyph texture
    glm::ivec2   Size;      // size of glyph
    glm::ivec2   Bearing;   // offset from baseline to left/top of glyph
    unsigned int Advance;   // horizontal offset to advance to next glyph
};

enum TEXT_RENDER_MODE {
    TEXT_CV = 0,
    TEXT_GL = 1,
};

class sanTextRenderer
{
public:
    sanXML* m_pxml;
    std::map<char, TextCharacter> m_characters;                             // holds a list of pre-compiled Characters
    sanShader m_textShader = sanShader(vs_common_text, NULL, fs_common_text);     // shader used for text rendering
    int m_fontSize;

    sanTextRenderer(sanXML* pxml);
    ~sanTextRenderer();

    void initialize(std::string fontPath, int fontSize);
    void cleanCharactersMap();
    glm::vec3 calculateTextSize(const std::string& text, float scale = 1.0f);
    void renderText(const std::string& text, glm::vec3 pos, TEXT_RENDER_MODE render_mode, glm::mat4 mvp = glm::mat4(1.0f), float scale = 1.0f, glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.0f));

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
};

#endif 
