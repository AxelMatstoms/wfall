#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

#include <SDL.h>
#include <glad/glad.h>

#include "shader.h"
#include "matrix.h"
#include "affine2d.h"
#include "fftseq.h"
#include "cmap.h"

static const std::size_t WIN_HEIGHT = 800;
static const std::size_t WIN_WIDTH = 1280;
static const std::size_t FFT_SIZE = 2048;
static const float SPECTRUM_HEIGHT = 0.2f;
static const std::string cmap_path = "res/cmap/turbo.csv";

void GLAPIENTRY
message_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
        GLsizei length, const GLchar* msg, const void* user)
{
    std::cerr << "type = " << type << ", severity = " << severity << ", msg = " << msg << std::endl;
}

std::vector<float> fft_db(const std::vector<float>& fft)
{
    std::vector<float> out(fft.size());

    for (std::size_t i = 0; i < fft.size(); i++) {
        out[i] = 20.0f * std::log10(fft[i]);
    }

    return out;
}

std::vector<float> fft_pos_abs(const std::vector<std::complex<float>>& fft)
{
    std::vector<float> out(fft.size() / 2);
    float norm = 2.0f / fft.size();
    for (std::size_t i = 0; i < out.size(); i++) {
        out[i] = std::abs(fft[i]) * norm;
    }

    return out;
}

std::vector<float> fft_shift_abs(const std::vector<std::complex<float>>& fft)
{
    std::vector<float> out(fft.size());
    std::size_t half = out.size() / 2;
    float norm = 1.0f / fft.size();
    for (std::size_t i = 0; i < out.size(); i++) {
        int shift = (i < half) ? half : -half;
        out[i] = std::abs(fft[i + shift]) * norm;
    }

    return out;
}

void gen_fft_mipmap(const std::vector<std::complex<float>>& fft,
        std::size_t idx, bool negative = false)
{
    std::vector<float> mipmap;
    if (negative) {
        mipmap = fft_shift_abs(fft);
    } else {
        mipmap = fft_pos_abs(fft);
    }

    int level = 0;
    do {
        std::vector<float> tex_line = fft_db(mipmap);
        glTexSubImage2D(GL_TEXTURE_1D_ARRAY, level, 0, idx, tex_line.size(), 1,
                GL_RED, GL_FLOAT, tex_line.data());

        for (std::size_t i = 0; i < mipmap.size() / 2; ++i) {
            mipmap[i] = 0.5f * (mipmap[2 * i] + mipmap[2 * i + 1]);
        }
        mipmap.resize(mipmap.size() / 2);
        level++;

    } while (mipmap.size() > 1);
}

int main()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cerr << "SDL could not initialize. Error:"
                  << std::endl
                  << SDL_GetError()
                  << std::endl;
        exit(1);
    }

    SDL_Window* window = SDL_CreateWindow(
            "wfall",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            WIN_WIDTH,
            WIN_HEIGHT,
            SDL_WINDOW_OPENGL
    );

    SDL_Renderer* renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_ACCELERATED);

    if (renderer == nullptr) {
        std::cerr << "Could not create renderer. Error:"
                  << std::endl
                  << SDL_GetError()
                  << std::endl;
        exit(1);
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);

    int version = gladLoadGLLoader((GLADloadproc) SDL_GL_GetProcAddress);
    if (!version) {
        std::cerr << "Could not init GL context." << std::endl;
        exit(1);
    }

    std::cout << "OpenGL loaded successfully version " << glGetString(GL_VERSION) << std::endl;
    std::cout << "OpenGL loaded on renderer " << glGetString(GL_RENDERER) << std::endl;

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, nullptr);

    Shader spectrum_shader = Shader::compile("res/shader/shader.vert", "res/shader/spectrum.frag");
    if (spectrum_shader.bad()) {
        exit(1);
    }

    Shader waterfall_shader = Shader::compile("res/shader/shader.vert", "res/shader/waterfall.frag");
    if (waterfall_shader.bad()) {
        exit(1);
    }

    std::vector<float> vertices{
    //        x      y      u     v
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,

            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
    };

    GLuint vbo;
    glGenBuffers(1, &vbo);

    GLuint vao;
    glGenVertexArrays(1, &vao);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
            vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*) (2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glActiveTexture(GL_TEXTURE0);

    GLuint texture;
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_1D_ARRAY, texture);

    glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    float border_color[4] = {-200.f, 0.0f, 0.0f, 0.0f};
    glTexParameterfv(GL_TEXTURE_1D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_color);

    std::vector<float> wfall_init(FFT_SIZE * 1024, -250.0f);
    glTexImage2D(GL_TEXTURE_1D_ARRAY, 0, GL_R32F, FFT_SIZE, 1024, 0, GL_RED, GL_FLOAT, wfall_init.data());
    glGenerateMipmap(GL_TEXTURE_1D_ARRAY);

    glActiveTexture(GL_TEXTURE0 + 1);

    GLuint cmap_texture;
    glGenTextures(1, &cmap_texture);

    glBindTexture(GL_TEXTURE_1D, cmap_texture);

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

    auto cmap_data = csv_read_cmap(cmap_path);
    std::cout << "Loaded " << cmap_path << " read " << cmap_data.size() << " colors" << std::endl;
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB32F, cmap_data.size(), 0,
            GL_RGB, GL_FLOAT, cmap_data.data());

    glActiveTexture(GL_TEXTURE0);

    std::size_t line = 0;

    PcmStream<int16_t> stream(std::cin);
    stream.channels(2);
    stream.mix();

    FftSeq fft_seq(stream, FFT_SIZE * 2, blackman);
    fft_seq.optimal_spacing(44100, 12);

    fft_seq.start();

    std::cout << "FFT spacing: " << fft_seq.spacing() << std::endl;

    spectrum_shader.use();

    glUniform1i(spectrum_shader["wfall"], 0);

    auto spectrum_transform = translate(0.0f, 1.0f - SPECTRUM_HEIGHT) * scale(1.0f, SPECTRUM_HEIGHT);

    glUniformMatrix3fv(spectrum_shader["transform"], 1, GL_TRUE, spectrum_transform.data());
    glUniform1f(spectrum_shader["width"], WIN_WIDTH);

    waterfall_shader.use();
    auto waterfall_transform = translate(0.0f, -SPECTRUM_HEIGHT) * scale(1.0f, 1.0f - SPECTRUM_HEIGHT);

    glUniformMatrix3fv(waterfall_shader["transform"], 1, GL_TRUE, waterfall_transform.data());
    glUniform1i(waterfall_shader["wfall"], 0);
    glUniform1i(waterfall_shader["cmap"], 1);
    glUniform1f(waterfall_shader["wrapPos"], 0.0f);
    glUniform1f(waterfall_shader["histLen"], 1024.0f);
    glUniform1f(waterfall_shader["wfallHeight"], (1.0f - SPECTRUM_HEIGHT) * WIN_HEIGHT);

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }
        }

        if (fft_seq.has_next()) {
            auto fft_line = fft_seq.next();
            fft_seq.notify();

            gen_fft_mipmap(fft_line, line, false);

            spectrum_shader.use();
            glUniform1f(spectrum_shader["wrapPos"], line);

            waterfall_shader.use();
            glUniform1f(waterfall_shader["wrapPos"], line);

            line++;
            line &= 0x3ff;
        }

        glClearColor(1.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindVertexArray(vao);

        spectrum_shader.use();
        glDrawArrays(GL_TRIANGLES, 0, 6);

        waterfall_shader.use();
        glDrawArrays(GL_TRIANGLES, 0, 6);

        SDL_GL_SwapWindow(window);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
