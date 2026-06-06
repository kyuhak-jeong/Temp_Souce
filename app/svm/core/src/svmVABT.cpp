#include "svmVABT.hpp"

sanVABT::sanVABT()
{
}

sanVABT::~sanVABT()
{
	if (m_vabt_list.size())
	{
		for (int list_index = 0; list_index < (int)m_vabt_list.size(); list_index++)
		{
			if (m_vabt_list[list_index].vaoID)
			{
				glDeleteVertexArrays(1, &m_vabt_list[list_index].vaoID);
				m_vabt_list[list_index].vaoID = 0;
			}
			else noop;

			if (m_vabt_list[list_index].vboID)
			{
				glDeleteBuffers(1, &m_vabt_list[list_index].vboID);
				m_vabt_list[list_index].vboID = 0;
			}
			else noop;

			if (m_vabt_list[list_index].texID)
			{			
				glDeleteTextures(1, &m_vabt_list[list_index].texID);
				m_vabt_list[list_index].texID = 0;
			}
			else noop;

			if (m_vabt_list[list_index].pitxID != nullptr)
			{
				if (*m_vabt_list[list_index].pitxID)
				{
					glDeleteTextures(1, m_vabt_list[list_index].pitxID);
					m_vabt_list[list_index].pitxID = nullptr;
				}
				else noop;
			}
			else noop;

			if (m_vabt_list[list_index].pmtxID != nullptr)
			{
				if (*m_vabt_list[list_index].pmtxID)
				{
					glDeleteTextures(1, m_vabt_list[list_index].pmtxID);
					m_vabt_list[list_index].pmtxID = nullptr;
				}
				else noop;
			}
			else noop;
		}

		if (!m_vabt_list.empty()) m_vabt_list.clear();
		else noop;
	}
	else noop;

}


int sanVABT::generateVAB(GLfloat* updating_vertices, GLuint updating_total_lines_num, GLuint fenum_in_a_line, GLuint senum_in_a_line, GLuint gl_drawing_type)  //Vertex Array Buffer
{
	GLuint tenum_in_a_line = fenum_in_a_line + senum_in_a_line; // total element number
	VABT vabt = { 0, 0, 0, 0, nullptr, nullptr };

	sanError::glClearError();


	glGenVertexArrays(1, &vabt.vaoID); // VAO
	glBindVertexArray(vabt.vaoID);

	glGenBuffers(1, &vabt.vboID); // VBO
	glBindBuffer(GL_ARRAY_BUFFER, vabt.vboID);

	if (updating_vertices != nullptr)
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * tenum_in_a_line * updating_total_lines_num, &updating_vertices[0], gl_drawing_type);
	else noop;

	// VBO - first 
	if (0 < fenum_in_a_line) // number of first element in a line
	{
		glVertexAttribPointer(0, fenum_in_a_line, GL_FLOAT, GL_FALSE, tenum_in_a_line * sizeof(GLfloat), (const void*)0);
		glEnableVertexAttribArray(0);
	}
	else
	{
		glDeleteBuffers(1, &vabt.vboID);
		glDeleteVertexArrays(1, &vabt.vaoID);
		string msg = string("$no first element");
		throw runtime_error(__FUNCTION__+ delimiter(msg));
	}

	// VBO - second
	if (0 < senum_in_a_line) // number of the second element in a line
	{
		glVertexAttribPointer(1, senum_in_a_line, GL_FLOAT, GL_FALSE, tenum_in_a_line * sizeof(GLfloat), (const void*)(fenum_in_a_line * sizeof(GLfloat)));
		glEnableVertexAttribArray(1);
	}
	else noop;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);


	sanError::glCheckError(__FUNCTION__);

	vabt.vnum = updating_total_lines_num;
	m_vabt_list.push_back(vabt);

	return (int)(m_vabt_list.size() - 1); // index
}

void sanVABT::updateVAB(GLuint list_index, GLfloat* updating_vertices, GLuint updating_total_lines_num, GLuint fenum_in_a_line, GLuint senum_in_a_line, GLuint gl_drawing_type)
{
	GLuint tenum_in_a_line = fenum_in_a_line + senum_in_a_line; // total element number = first element number + second element number
	if (list_index < m_vabt_list.size())
	{
		m_vabt_list[list_index].vnum = updating_total_lines_num;

		sanError::glClearError();

		glBindVertexArray(m_vabt_list[list_index].vaoID);
		glBindBuffer(GL_ARRAY_BUFFER, m_vabt_list[list_index].vboID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * tenum_in_a_line * m_vabt_list[list_index].vnum, &updating_vertices[0], gl_drawing_type);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		sanError::glCheckError(__FUNCTION__);
	}
	else
	{
		string msg = string("$Invalid argument");
		throw runtime_error(__FUNCTION__ + delimiter(msg));
	}
}


void sanVABT::generateTexture(GLuint txMode, GLuint* txID /*[out]*/)
{
	sanError::glClearError();

	glActiveTexture(txMode);
	glGenTextures(1, txID);
	glBindTexture(GL_TEXTURE_2D, *txID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	sanError::glCheckError(__FUNCTION__);
}


void sanVABT::updateTexture(GLuint txMode, GLuint txID, unsigned char* texture_data, int width, int height, GLuint nchannel, int binding_option)
{
	GLenum color_format = (nchannel == 1) ? GL_RED : (nchannel == 4) ? GL_RGBA : GL_RGB;

	sanError::glClearError();

	glActiveTexture(txMode);
	glBindTexture(GL_TEXTURE_2D, txID);
	glTexImage2D(GL_TEXTURE_2D, 0, color_format, width, height, 0, color_format, GL_UNSIGNED_BYTE, texture_data);

	if (binding_option)	glBindTexture(GL_TEXTURE_2D, 0);
	else noop;

	sanError::glCheckError(__FUNCTION__);
}

void sanVABT::updateTexture(GLuint txMode, GLuint txID, TEXTURE_INFO& texture_info)
{
	sanError::glClearError();

	glActiveTexture(txMode);
	glBindTexture(GL_TEXTURE_2D, txID);
	glTexImage2D(GL_TEXTURE_2D, 0, texture_info.color_format, texture_info.width, texture_info.height, 0, texture_info.color_format, GL_UNSIGNED_BYTE, texture_info.texture.data);
	glBindTexture(GL_TEXTURE_2D, 0);

	sanError::glCheckError(__FUNCTION__);
}

