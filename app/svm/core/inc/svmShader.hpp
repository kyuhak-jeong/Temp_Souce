#ifndef SVMSHADER_HPP_
#define SVMSHADER_HPP_

#include "svmCore.hpp"

class sanShader
{
public:
    
    GLuint m_programID = 0;

    sanShader(const char* vertexString, const char* geometryString, const char* fragmentString)
    {
        GLuint vertex = 0, fragment = 0;
        // vertex shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexString, nullptr);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        // fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentString, nullptr);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        // shader Program
        m_programID = glCreateProgram();
        glAttachShader(m_programID, vertex);
        glAttachShader(m_programID, fragment);

        #if defined(_WIN32) || defined(_WIN64)
            // if geometry shader is given, compile geometry shader
            GLuint geometry = 0;
            if(geometryString != nullptr)
            {
                geometry = glCreateShader(GL_GEOMETRY_SHADER);
                glShaderSource(geometry, 1, &geometryString, nullptr);
                glCompileShader(geometry);
                checkCompileErrors(geometry, "GEOMETRY");
                glAttachShader(m_programID, geometry);
            }
        #endif

        glLinkProgram(m_programID);
        checkCompileErrors(m_programID, "PROGRAM");

        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        #if defined(_WIN32) || defined(_WIN64)
            if(geometryString != nullptr)
                glDeleteShader(geometry);
        #endif
    }

    ~sanShader()
    {
        if (0 < m_programID) glDeleteProgram(m_programID);
    }

    GLuint getProgram()       { return m_programID; }

    void use()                      { glUseProgram(m_programID); }


    void setBool(const std::string &name, bool value) const
    {
        glUniform1i(glGetUniformLocation(m_programID, name.c_str()), (int)value);
    }
    
    void setInt(const std::string &name, int value) const
    { 
        glUniform1i(glGetUniformLocation(m_programID, name.c_str()), value);
    }
    
    void setFloat(const std::string &name, float value) const
    { 
        glUniform1f(glGetUniformLocation(m_programID, name.c_str()), value);
    }
    
    void setVec2(const std::string &name, const glm::vec2 &value) const
    { 
        glUniform2fv(glGetUniformLocation(m_programID, name.c_str()), 1, &value[0]);
    }
    void setVec2(const std::string &name, float x, float y) const
    { 
        glUniform2f(glGetUniformLocation(m_programID, name.c_str()), x, y);
    }
    
    void setVec3(const std::string &name, const glm::vec3 &value) const
    { 
        glUniform3fv(glGetUniformLocation(m_programID, name.c_str()), 1, &value[0]);
    }
    
    void setVec3(const std::string &name, float x, float y, float z) const
    { 
        glUniform3f(glGetUniformLocation(m_programID, name.c_str()), x, y, z);
    }
    
    void setVec4(const std::string &name, const glm::vec4 &value) const
    { 
        glUniform4fv(glGetUniformLocation(m_programID, name.c_str()), 1, &value[0]);
    }
    void setVec4(const std::string &name, float x, float y, float z, float w) 
    { 
        glUniform4f(glGetUniformLocation(m_programID, name.c_str()), x, y, z, w);
    }
    
    void setMat2(const std::string &name, const glm::mat2 &mat) const
    {
        glUniformMatrix2fv(glGetUniformLocation(m_programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    
    void setMat3(const std::string &name, const glm::mat3 &mat) const
    {
        glUniformMatrix3fv(glGetUniformLocation(m_programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }
    
    void setMat4(const std::string &name, const glm::mat4 &mat) const
    {
        glUniformMatrix4fv(glGetUniformLocation(m_programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
    }

    std::string getShaderInfoLog(GLuint shader) 
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    
        std::string infoLog;
        infoLog.resize(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
    
        return infoLog;
    }

    void checkCompileErrors(GLuint shader, std::string type)
    {
        GLint success = 0;
        if(type != "PROGRAM")
        {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if(!success)
            {
                std::string infoLog = getShaderInfoLog(shader);
                throw runtime_error(__FUNCTION__ + string(" $") + std::string(infoLog));
            }
        }
        else
        {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if(!success)
            {
                std::string infoLog = getShaderInfoLog(shader);
                throw runtime_error(__FUNCTION__ + string(" $") + std::string(infoLog));
            }
        }
    }

};


#endif
