#ifndef SVMVABT_HPP_
#define SVMVABT_HPP_

#include "svmCore.hpp"
#include "svmLogger.hpp"
#include "svmError.hpp"

#define NO_BINDING         (0)
#define BINDING            (1)

struct VABT   // Vertex Buffter & Texture
{
	GLuint	vaoID;   // vertex array object ID
	GLuint	vboID;   // vertex buffer object ID
	GLuint  texID;   // texture ID                   for Calibration
	int		vnum;    // number of vetices
	GLuint* pitxID;  // pointer of image texture ID  for Viewer
	GLuint* pmtxID;  // pointer of mask texture ID   for Viewer
};


class sanVABT // Vertex Array, Buffer and Texture
{
public: 
	vector<VABT> m_vabt_list;

public:
	sanVABT();
	~sanVABT();

	inline int getVABTLastIndex() { return (int)(m_vabt_list.size() - 1); }
	inline int getVABTSize() { return (int)m_vabt_list.size(); }

	inline int getVaoID(int list_index) { if (list_index < (int)m_vabt_list.size()) return m_vabt_list[list_index].vaoID; else return 0; };
	inline int getVboID(int list_index) { if (list_index < (int)m_vabt_list.size()) return m_vabt_list[list_index].vboID; else return 0; };
	inline int getTexID(int list_index) { if (list_index < (int)m_vabt_list.size()) return m_vabt_list[list_index].texID; else return 0; };
	inline int getVnum(int list_index)  { if (list_index < (int)m_vabt_list.size()) return m_vabt_list[list_index].vnum;  else return 0; };
	inline void setVnum(int list_index, int vnum) { m_vabt_list[list_index].vnum = vnum; }

	int generateVAB(GLfloat* updating_vertices, GLuint updating_total_lines_num, GLuint fenum_in_a_line, GLuint senum_in_a_line, GLuint gl_drawing_type);
	void updateVAB(GLuint list_index, GLfloat* updating_vertices, GLuint updating_total_lines_num, GLuint fenum_in_a_line, GLuint senum_in_a_line, GLuint gl_drawing_type);

	static void generateTexture(GLuint txMode, GLuint* txID);
	static void updateTexture(GLuint txMode, GLuint txID, unsigned char* texture, int width, int height, GLuint nchannel, int binding_option=BINDING);
	static void updateTexture(GLuint txMode, GLuint txID, TEXTURE_INFO& texture_info);	
};

#endif

