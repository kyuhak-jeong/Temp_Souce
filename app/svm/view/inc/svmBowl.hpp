#ifndef SVMBOWL_HPP_
#define SVMBOWL_HPP_

#include "svmCore.hpp"
#include "svmVABT.hpp"
#include "svmXML.hpp"
#include "svmShaderString.hpp"
#include "svmShader.hpp"

class sanBowl: public sanVABT
{
public:
	sanXML* m_pxml;
	PM* m_ppm;
	sanShader m_bowlbShader = sanShader(vs_view_bowl_b, NULL, fs_view_bowl_b);
	sanShader m_bowlShader = sanShader(vs_view_bowl, NULL, fs_view_bowl);

public:
	sanBowl(sanXML* pxml, PM *pm);
	~sanBowl();

	void initialize(GLuint* ppitxID[], GLuint* pmtxID[]);
	void renderBowl(GLuint camID, glm::mat4 mvp);
	void drawLayout0(GLuint camID, glm::mat4& vcvm);
	void drawLayout1(GLuint camID, glm::mat4 &vcvm);
};


#endif
