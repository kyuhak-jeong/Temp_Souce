#include "svmError.hpp"

#if defined(_WIN32)||defined(_WIN64)
std::unordered_map<GLuint, string> glerror_map = {
	{GL_NO_ERROR, "GL_NO_ERROR"},
	{GL_INVALID_ENUM, "GL_INVALID_ENUM"},
	{GL_INVALID_VALUE, "GL_INVALID_VALUE"},
	{GL_INVALID_OPERATION, "GL_INVALID_OPERATION"},
	{GL_INVALID_FRAMEBUFFER_OPERATION, "GL_INVALID_FRAMEBUFFER_OPERATION"},
	{GL_OUT_OF_MEMORY, "GL_OUT_OF_MEMORY"},
	{GL_STACK_OVERFLOW, "GL_STACK_OVERFLOW"},
	{GL_STACK_UNDERFLOW, "GL_STACK_UNDERFLOW"}
};
#else
std::unordered_map<GLuint, string> glerror_map = {
	{GL_NO_ERROR, "GL_NO_ERROR"},
	{GL_INVALID_ENUM, "GL_INVALID_ENUM"},
	{GL_INVALID_VALUE, "GL_INVALID_VALUE"},
	{GL_INVALID_OPERATION, "GL_INVALID_OPERATION"},
	{GL_INVALID_FRAMEBUFFER_OPERATION, "GL_INVALID_FRAMEBUFFER_OPERATION"},
	{GL_OUT_OF_MEMORY, "GL_OUT_OF_MEMORY"}
};
#endif


string sanError::glGetErrorMessage(int glerror)
{
	string errStr = (glerror_map.find(glerror) != glerror_map.end())? glerror_map[glerror] : "$Unknown";
	return "glError(" + to_string(glerror) + ") " + errStr;
}

void sanError::glCheckError()
{
	GLenum glerror = GL_NO_ERROR;

	glerror = glGetError();
	
	if (GL_NO_ERROR != glerror) throw runtime_error(sanError::glGetErrorMessage(glerror)); \
	else noop;
}

void sanError::glCheckError(string msg)
{	
	string delim;
	if (0 < msg.size()) delim = (msg[0] == '$')? " ":" $";
	else delim = " $";

	GLenum glerror = GL_NO_ERROR;

	glerror = glGetError();
	
	if (GL_NO_ERROR != glerror) throw runtime_error(msg + delim + sanError::glGetErrorMessage(glerror)); \
	else noop; 
} 
