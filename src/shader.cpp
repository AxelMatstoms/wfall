#include <fstream>
#include <sstream>
#include <optional>
#include <vector>
#include <iostream>
#include <stdexcept>

#include "shader.h"

std::string read_file(const std::string& path)
{ std::ifstream input(path);

    if (!input) {
        std::cerr << "Could not open " << path << " for reading." << std::endl;
        return std::string();
    }

    std::ostringstream ss;
    ss << input.rdbuf();

    return ss.str();
}

GLuint compile_shader(const std::string& source, GLenum type)
{
    GLuint shader = glCreateShader(type);

    const GLchar* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);

    glCompileShader(shader);

    int compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
        int log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        std::string log(log_length, ' ');
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());

        glDeleteShader(shader);

        std::cerr << log << std::endl;

        return 0;
    }

    return shader;
}

GLuint link_program(const std::vector<GLuint>& shaders)
{
    GLuint program = glCreateProgram();

    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked) {
        int log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

        std::string log(log_length, ' ');
        glGetProgramInfoLog(program, log_length, nullptr, log.data());

        glDeleteProgram(program);
        for (auto shader : shaders) {
            glDeleteShader(shader);
        }

        std::cerr << "An error occured when linking shader program:" << std::endl;
        std::cerr << log << std::endl;

        return 0;
    }

    return program;
}

GLint Shader::operator[](const std::string& name)
{
    GLint location;
    if (auto search = uniforms.find(name); search != uniforms.end()) {
        location = search->second;
    } else {
        location = glGetUniformLocation(program, name.c_str());
        if (location == -1)
            throw std::invalid_argument(name + " is not an active uniform in program,"
                    " or is a structure, array of structures, or a subcomponent of"
                    " a vector or matrix.");
        uniforms[name] = location;
    }

    return location;
}

bool Shader::bad() const
{
    return program == 0;
}

void Shader::use() const
{
    glUseProgram(program);
}

Shader Shader::compile(const std::string& vtxPath, const std::string& fragPath)
{
    std::cout << "Compiling vertex shader at path: " << vtxPath << " ..." << std::endl;
    auto vtxShader = compile_shader(read_file(vtxPath), GL_VERTEX_SHADER);
    if (!vtxShader) {
        return Shader{0};
    }

    std::cout << "Compiling fragment shader at path: " << fragPath << " ..." << std::endl;
    auto fragShader = compile_shader(read_file(fragPath), GL_FRAGMENT_SHADER);
    if (!fragShader) {
        return Shader{0};
    }

    std::cout << "Linking shader program..." << std::endl;
    auto program = link_program({vtxShader, fragShader});
    if (!program) {
        return Shader{0};
    }

    return Shader{program};
}
