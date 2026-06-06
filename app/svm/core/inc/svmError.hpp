#ifndef SVMERROR_HPP_
#define SVMERROR_HPP_

#include "svmCore.hpp"

class sanError
{
public:
	inline static void glClearError() { while (glGetError() != GL_NO_ERROR); }
	static string glGetErrorMessage(int glerror);
	static void glCheckError();
	static void glCheckError(string msg);
};

#endif
