#ifndef WFALL_SHADER_H
#define WFALL_SHADER_H

#include <string>
#include <optional>
#include <unordered_map>
#include <array>

#include <glad/glad.h>

GLuint compile_shader(const std::string& source, GLenum type);
GLuint link_program(const std::vector<GLuint>& shaders);

struct Shader {
    GLuint program;
    std::unordered_map<std::string, GLint> uniforms;

    GLint operator[](const std::string& name);
    bool bad() const;
    void use() const;

    static Shader compile(const std::string& vtxPath, const std::string& fragPath);
};

#endif /* WFALL_SHADER_H */
