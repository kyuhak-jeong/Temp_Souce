#include "svmBowl.hpp"
#include "svmFromFile.hpp"
#include "svmError.hpp"

sanBowl::sanBowl(sanXML* pxml, PM* ppm)
{
    m_pxml = pxml;
    m_ppm = ppm;
}

sanBowl::~sanBowl()
{
	m_bowlbShader.~sanShader();
	m_bowlShader.~sanShader();
}


void sanBowl::initialize(GLuint* ppitxID[] /* pointer of image texture ID */, GLuint* ppmtxID[] /* pointer of mask image texture ID */)
{  
    for (int i = 0; i < 2 * SVM_CAMERAS_NUM; i++) // VAO and VBO(8)
    {
		string str_index = "(index[" + to_string(i) + "]) -> ";

		try
		{
			int quotient = (i / 2 + 1); // array1x, array2x ...
			int remainder = (i % 2 + 1); // arrayx1, arrayx2 ...
			string full_file_path = string(_ARRAYS_PATH_) + string("/array") + to_string(quotient) + to_string(remainder);

			GLfloat* vertices = nullptr;
			int vertices_num = sanFromFile::read_mesh(full_file_path, &vertices);
			generateVAB(vertices, vertices_num, 3, 2, GL_STATIC_DRAW);
			if (vertices != nullptr) free(vertices);
			else noop;

			if (i % 2 == 0)
			{
				m_vabt_list[i].pitxID = ppitxID[i / 2];
				m_vabt_list[i].pmtxID = ppmtxID[i / 2];
			}
			else
			{
				m_vabt_list[i].pitxID = ppitxID[i / 2];
				m_vabt_list[i].pmtxID = ppmtxID[i / 2];
			}
		}
		catch (exception& e)
		{
			throw logger.svm_fatal("C2102001", __FUNCTION__ + str_index + delimiter(string(e.what())));
		}
    }
}




void sanBowl::renderBowl(GLuint camID, glm::mat4 vp)
{
	sanError::glClearError();

	int vabt_list_index = 0;

	// overlapped
	m_bowlbShader.use();
	vabt_list_index = 2 * camID + 0;

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);  // Enable blending
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glBindVertexArray(m_vabt_list[vabt_list_index].vaoID);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *m_vabt_list[vabt_list_index].pitxID);
	glUniform1i(glGetUniformLocation(m_bowlbShader.getProgram(), "img"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, *m_vabt_list[vabt_list_index].pmtxID);
	glUniform1i(glGetUniformLocation(m_bowlbShader.getProgram(), "msk"), 1);

	glUniformMatrix4fv(glGetUniformLocation(m_bowlbShader.getProgram(), "mvp"), 1, GL_FALSE, &vp[0][0]);
	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_list_index].vnum);

	sanError::glCheckError(__FUNCTION__ + string(": step 1"));

	// non-overlapped
	m_bowlShader.use();
	vabt_list_index = 2 * camID + 1;

	glm::vec4 compColor = glm::vec4(0.0f);
	// if(camID == 2) compColor = glm::vec4(0.15f, 0.15f, 0.15f, 0.0f);
	
	glBindVertexArray(m_vabt_list[vabt_list_index].vaoID);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *m_vabt_list[vabt_list_index].pitxID);
	glUniform1i(glGetUniformLocation(m_bowlShader.getProgram(), "img"), 0);
	glUniformMatrix4fv(glGetUniformLocation(m_bowlShader.getProgram(), "mvp"), 1, GL_FALSE, &vp[0][0]);
	glUniform4fv(glGetUniformLocation(m_bowlShader.getProgram(), "compensate"), 1, &compColor[0]);
	glDrawArrays(GL_TRIANGLES, 0, m_vabt_list[vabt_list_index].vnum);

	glBindVertexArray(0);
	glUseProgram(0);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	sanError::glCheckError(__FUNCTION__ + string(": step 2"));
}



void sanBowl::drawLayout0(GLuint camID, glm::mat4 &vcvm)
{
	try
	{
		if (m_pxml->m_layout[0].view_mode == TOPVIEW3D)
		{
			glViewport(m_pxml->m_layout[0].x, m_pxml->m_layout[0].y, m_pxml->m_layout[0].width, m_pxml->m_layout[0].height);

			renderBowl(camID, m_ppm->opm * vcvm);
		}
		else
			throw runtime_error("view_mode wrong");
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2202101", __FUNCTION__ + delimiter(string(e.what())));
	}		
}


void sanBowl::drawLayout1(GLuint camID, glm::mat4 &vcvm)
{
	try
	{
		switch (m_pxml->m_layout[1].view_mode)
		{
			case CAMVIEW3D_FRONT:
			case CAMVIEW3D_RIGHT:
			case CAMVIEW3D_REAR:
			case CAMVIEW3D_LEFT:
			{
				glViewport(m_pxml->m_layout[1].x, m_pxml->m_layout[1].y, m_pxml->m_layout[1].width, m_pxml->m_layout[1].height);

				renderBowl(camID, m_ppm->ppm * vcvm);
				break;
			}
			case TOPVIEW3D:
			case CAMVIEW2D_FRONT:
			case CAMVIEW2D_RIGHT:
			case CAMVIEW2D_REAR:
			case CAMVIEW2D_LEFT:
			case CAMVIEW2D_ADD0:
				break;
			default:
				throw runtime_error("view_mode wrong");
				break;
		}
	}
	catch (exception& e)
	{
		throw logger.svm_fatal("C2202201", __FUNCTION__ + delimiter(string(e.what())));
	}
}

