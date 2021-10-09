#include <iostream>
#include <fstream>
#include <vector>

#include <SDL.h>
#include <glad/glad.h>

#include "shader.h"
#include "matrix.h"
#include "affine2d.h"
#include "fftseq.h"

void test_fft_seq()
{
    std::ifstream f("/tmp/mpd.fifo");
    if (f.good()) {
        std::cout << "Opened successfully." << std::endl;
    }
    PcmStream<int16_t> stream(f);
    stream.channels(2);
    stream.solo(0);

    std::vector<std::complex<float>> v;
    while ((v = stream.read_chunk(256)).empty()) {}
    for (auto x : v) {
        std::cout << x << std::endl;
    }
}

void test_no_block_adapter()
{
    std::ifstream f("example.txt");
    NoBlockAdapter nba(f);
    for (int i = 0; i < 12; i++) {
        while (!nba.read(256)) {}
        while (!nba.done()) {}

        std::string msg(nba.result(), 256);
        std::cout << msg << std::endl;
    }
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
            1280,
            720,
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

    spectrum_shader.use();

    //shader["color"] = {0.5, 1.0};
    glUniform2f(spectrum_shader["color"], 0.5, 0.0);

    auto spectrum_transform = translate(0.0f, 0.9f) * scale(1.0f, 0.1f);

    glUniformMatrix3fv(spectrum_shader["transform"], 1, GL_TRUE, spectrum_transform.data());

    waterfall_shader.use();
    auto waterfall_transform = translate(0.0f, -0.1) * scale(1.0f, 0.9f);

    glUniformMatrix3fv(waterfall_shader["transform"], 1, GL_TRUE, waterfall_transform.data());

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

    //test_no_block_adapter();
    test_fft_seq();

    return 0;
}
