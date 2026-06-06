#ifndef SVMMRT_HPP_
#define SVMMRT_HPP_

#include "svmCore.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"

#define MRT_ENABLED (1)

class sanMRT  // Multiple Render Targets
{
protected:
	sanXML* m_pxml;
	int m_display_width;
	int m_display_height;

	GLuint m_fboID;      // frame buffer object ID
	GLuint m_rboID;      // render buffer object ID
	GLuint m_vaoID;      // vertex array object ID
	GLuint m_vboIDs[2];  // vertex buffer object ID

	GLuint m_screen_texID;
	GLuint m_video_texID;

	GLfloat m_vtx_coords[8] = { 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f }; // the pairs of (X,Y) based on GL

	#if(0)
		GLfloat m_tex_coords[8] = { 1.0f,  1.0f, 1.0f, 0.0f,  0.0f,  1.0f,  0.0f, 0.0f }; // horizontal flip
	#else
		GLfloat m_tex_coords[8] = { 1.0f,  0.0f, 1.0f, 1.0f,  0.0f,  0.0f,  0.0f, 1.0f }; // the pairs of (x,y) based on NDC
	#endif

	sanShader m_mrtShader = sanShader(vs_common_mrt, NULL, fs_common_mrt);

public:

	sanMRT(sanXML* pxml);
	~sanMRT();
	void initialize();

	bool isEnabled() { return (0 < MRT_ENABLED); }
	GLuint getFBO() { return m_fboID; }
	GLuint getTexID() { return m_screen_texID; }

	void createFrameBuffer();
	void UpdateBufferData(GLfloat* vtx = nullptr, GLfloat* tex = nullptr);
	void setMRT();
	void RenderMRT();
};

#endif 
