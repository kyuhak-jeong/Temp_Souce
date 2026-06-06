#include <iostream>
#include <cassert>
#include "svmMRT.hpp"
#include "svmShaderString.hpp"
#include "svmLogger.hpp"


sanMRT::sanMRT(sanXML* pxml)
{
	m_pxml = pxml;

	this->m_display_width = 0;
	this->m_display_height = 0;

	this->m_fboID = 0;
	this->m_vaoID = 0;
	this->m_vboIDs[0] = 0;
	this->m_vboIDs[1] = 0;
	
	this->m_rboID = 0;
	this->m_screen_texID = 0;
	this->m_video_texID = 0;
}

sanMRT::~sanMRT()
{
	if (isEnabled())
	{
		if(m_screen_texID)
		{
			glDeleteTextures(1, &m_screen_texID);
			m_screen_texID = 0;
		}
		if(m_video_texID)
		{
			glDeleteTextures(1, &m_video_texID);
			m_video_texID = 0;
		}

		if(m_rboID)
		{
			glDeleteRenderbuffers(1, &m_rboID);
			m_rboID = 0;
		}

		if(m_fboID)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &m_fboID);
			m_fboID = 0;
		}

		if(m_vboIDs[0] || m_vboIDs[1])
		{
			glDeleteBuffers(2, m_vboIDs);
			m_vboIDs[0] = 0;
			m_vboIDs[1] = 0;
		}
		
		if(m_vaoID)
		{
			glDeleteVertexArrays(1, &m_vaoID);
			m_vaoID = 0;
		}
		

		m_mrtShader.~sanShader();
	}
	else noop;
}


void sanMRT::initialize()
{
	m_display_width = (int)m_pxml->m_resolution.display.width;
	m_display_height = (int)m_pxml->m_resolution.display.height;
	createFrameBuffer();
}


void sanMRT::createFrameBuffer()
{
	if (isEnabled())
	{
		//generate FBO
		glGenFramebuffers(1, &m_fboID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_fboID);

		//generate RBO
		glGenRenderbuffers(1, &m_rboID); //depth buffer
		glBindRenderbuffer(GL_RENDERBUFFER, m_rboID);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, m_display_width, m_display_height);

		//color texture
		glGenTextures(1, &m_screen_texID);
		glBindTexture(GL_TEXTURE_2D, m_screen_texID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_display_width, m_display_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		//normal texture
		glGenTextures(1, &m_video_texID);
		glBindTexture(GL_TEXTURE_2D, m_video_texID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_display_width, m_display_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		//bind texture to FBO
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_screen_texID, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_video_texID, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rboID);

		//Test FrameBuffer completeness
		assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		// unbind
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		//vao
		glGenVertexArrays(1, &m_vaoID);
		glBindVertexArray(m_vaoID);

		//vbo
		glGenBuffers(1, &m_vboIDs[0]);
		glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[0]);
		glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), m_vtx_coords, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(GLuint(0), 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(0);

		//vbo
		glGenBuffers(1, &m_vboIDs[1]);
		glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[1]);
		glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), m_tex_coords, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(GLuint(1), 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		//assert(GL_NO_ERROR == glGetError());
	}
	else noop;
}


void sanMRT::UpdateBufferData(GLfloat* vtx, GLfloat* tex)
{
	glBindVertexArray(m_vaoID);
	glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[0]);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), (vtx != nullptr) ? vtx : m_vtx_coords, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[1]);
	glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(GLfloat), (tex != nullptr) ? tex : m_tex_coords, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}


void sanMRT::RenderMRT()
{
	if (this->isEnabled())
	{
		m_mrtShader.use();

		glBindVertexArray(m_vaoID);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_screen_texID);
		glUniform1i(glGetUniformLocation(m_mrtShader.getProgram(), "tex"), 0);

		glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[0]);
		glVertexAttribPointer(GLuint(0), 2, GL_FLOAT, GL_FALSE, 0, 0); 

		glBindBuffer(GL_ARRAY_BUFFER, m_vboIDs[1]);
		glVertexAttribPointer(GLuint(1), 2, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
		// assert(glGetError() == GL_NO_ERROR);

	}
	else noop;
}


void sanMRT::setMRT()
{
	if (this->isEnabled())
	{
		glBindFramebuffer(GL_FRAMEBUFFER, getFBO());
		GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, drawBuffers); // "1" is the size of DrawBuffers
		// assert(glGetError() == GL_NO_ERROR);
	}
	else noop;
}
