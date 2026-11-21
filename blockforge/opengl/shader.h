#pragma once

#include <string>
#include <GL/glew.h>

class Shader {
public:
    GLuint ID;
    
    // Constructor reads and builds the shader
    Shader(const char* vertexPath, const char* fragmentPath);
    
    // Activate the shader
    void use();
    
    // Utility functions for setting material/light uniforms
    void setMat4(const std::string& name, const glm::mat4& mat);
    void setVec3(const std::string& name, float v0, float v1, float v2);
    void setVec3(const std::string& name, const glm::vec3& vec);
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    
    // Delete copy constructor and assignment
    Shader& operator=(const Shader&) = delete;
    Shader(const Shader&) = delete;
    
    // Destructor
    ~Shader();
    
private:
    // Shader source paths
    std::string vertexPath;
    std::string fragmentPath;
    
    // Utility function for compiling shaders
    unsigned int compileShader(unsigned int type, const std::string& source);
    
    // Utility function for linking program
    unsigned int linkProgram(unsigned int vertex, unsigned int fragment);
};