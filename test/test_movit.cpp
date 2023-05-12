//
// Created by huihui on 2023/5/6.
//
#include "SDL2/SDL.h"
#include <thread>
#include <iostream>
#include <fstream>
#include "movit/effect_chain.h"
#include "movit/input.h"
#include "movit/util.h"
#include "movit/mirror_effect.h"
#include "movit/init.h"
#include "resource_pool.h"

using namespace movit;
using namespace std;

std::string readFileToString(const std::string& filename) {
    std::ifstream input(filename);
    if (!input.is_open()) {
        std::cerr << "failed to open file " << filename << std::endl;
        return "";
    }

    std::string content;
    std::stringstream ss;
    while (input >> ss.rdbuf()){ }
    content = ss.str();

    input.close();
    return content;
}

class BlueInput : public Input {
public:
    BlueInput() { register_int("needs_mipmaps", &needs_mipmaps); }

    string effect_type_id() const override { return "IdentityEffect"; }

    string output_fragment_shader() override { return readFileToString("./blue.frag"); }

    AlphaHandling alpha_handling() const override { return OUTPUT_BLANK_ALPHA; }

    bool can_output_linear_gamma() const override { return true; }

    unsigned get_width() const override { return 1; }

    unsigned get_height() const override { return 1; }

    Colorspace get_color_space() const override { return COLORSPACE_sRGB; }

    GammaCurve get_gamma_curve() const override { return GAMMA_LINEAR; }

private:
    int needs_mipmaps;
};

class IdentityEffect : public Effect {
public:
    IdentityEffect() {}

    string effect_type_id() const override { return "IdentityEffect"; }

    string output_fragment_shader() override { return read_file("identity.frag"); }
};

GLuint initChain(int width, int height) {
    auto *chain = new EffectChain(width, height);
    chain->add_input(new BlueInput());
    chain->add_effect(new IdentityEffect());  // Not BlankAlphaPreservingEffect.
    ImageFormat image_format{COLORSPACE_sRGB,
                             GAMMA_LINEAR};//GL_RGBA, COLORSPACE_sRGB, GAMMA_LINEAR, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED
    chain->add_output(image_format, OUTPUT_ALPHA_FORMAT_POSTMULTIPLIED);
    chain->finalize();
    glActiveTexture(GL_TEXTURE0);
    check_error();
    GLenum framebuffer_format = GL_RGBA8;
    GLuint texnum = chain->get_resource_pool()->create_2d_texture(framebuffer_format, width, height);
    EffectChain::DestinationTexture destinationTexture{texnum, framebuffer_format};

    // The output texture needs to have valid state to be written to by a compute shader.
    glBindTexture(GL_TEXTURE_2D, texnum);
    check_error();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    check_error();

    chain->render_to_texture({destinationTexture}, width, height);
    glActiveTexture(GL_NONE);
    return texnum;
}

void testGL(SDL_Window *window) {

    const char *vertexShaderSource = "#version 330 core\n"
                                     "layout (location = 0) in vec3 aPos;\n"
                                     "void main()\n"
                                     "{\n"
                                     "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                     "}\0";
    const char *fragmentShaderSource = "#version 330 core\n"
                                       "out vec4 FragColor;\n"
                                       "void main()\n"
                                       "{\n"
                                       "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
                                       "}\n\0";


    //build and compile 着色器程序
    //顶点着色器
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    //检查顶点着色器是否编译错误
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    } else {
        std::cout << "vertexShader complie SUCCESS" << std::endl;
    }
    //片段着色器
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    //检查片段着色器是否编译错误
    glGetShaderiv(fragmentShader, GL_LINK_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    } else {
        std::cout << "fragmentShader complie SUCCESS" << std::endl;
    }
    //连接着色器
    unsigned int shaderProgram;
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    //检查片段着色器是否编译错误
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    } else {
        std::cout << "shaderProgram complie SUCCESS" << std::endl;
    }
    //连接后删除
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    //顶点数据
    float vertices[] = {
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            0.0f, 0.5f, 0.0f
    };

    /***Vertex Buffer Objects, VBO 它会在GPU内存(通常被称为显存)中储存大量顶点。
     * 使用这些缓冲对象的好处是我们可以一次性的发送一大批数据到显卡上，而不是每个顶点发送一次。***/
    GLuint VBO;
    glGenBuffers(1, &VBO);
    GLuint VAO;
    glGenVertexArrays(1, &VAO);

    // 初始化代码
    // 1. 绑定VAO
    glBindVertexArray(VAO);
    // 2. 把顶点数组复制到缓冲中供OpenGL使用
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // 3. 设置顶点属性指针 (告诉Opengl 该如何解析顶点数据)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);



    // 渲染循环
    while (true) {

        // 渲染指令
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 检查并调用事件，交换缓冲
        SDL_GL_SwapWindow(window);

        // 检查触发什么事件，更新窗口状态
    }

}

float vertices[] = {
        // positions          // texture coords
        0.5f, 0.5f, 0.0f, 1.0f, 1.0f,// top right
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, // bottom left
        -0.5f, 0.5f, 0.0f, 0.0f, 1.0f// top left
};

const char *shader_vertex = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main()
    {
        gl_Position = vec4(aPos, 1.0);
        TexCoord = vec2(aTexCoord.x, aTexCoord.y);
    }
)";

const char *shader_fragment = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoord;
    uniform sampler2D texture1;
    void main()
    {
        FragColor = texture(texture1, TexCoord);
    }
)";

unsigned int indices[] = {
        0, 1, 3, 2
};

int main() {
    int sdl_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    GLuint texture = 0;
    int window_width = 1280;
    int window_height = 720;
    int vertexShader = 0;
    int fragmentShader = 0;
    int shaderProgram = 0;
    int success;
    char infoLog[512];
    uint32_t VAO = 0;
    uint32_t VBO = 0;
    uint32_t EBO = 0;
    //shared context
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

    if (!SDL_WasInit(SDL_INIT_VIDEO)) {
        int ret = SDL_Init(SDL_INIT_VIDEO);
        if (ret < 0) {
            printf("Failed to initialize SDL: %s\n", SDL_GetError());
            return -1;
        }
    }
    SDL_Window *window = SDL_CreateWindow("MLT", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          window_width, window_height, sdl_flags);
    SDL_GLContext ctx = SDL_GL_CreateContext(window);
    if (!ctx) {
        printf("Failed to create SDL GLContext: %s\n", SDL_GetError());
        goto end;
    }
    if (!init_movit("/usr/local/share/movit", MovitDebugLevel::MOVIT_DEBUG_ON)) {
        printf("Failed to initialize movit\n");
        goto end;
    }

    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &shader_vertex, NULL); // 把这个着色器源码附加到着色器对象。着色器对象，源码字符串数量，VS真正的源码
    glCompileShader(vertexShader);
    // check for shader compile errors

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::VERTEX::COMPILATION_FAILED %s\n", infoLog);
        goto end;
    }
    // fragment shader
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &shader_fragment, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("ERROR::SHADER::FRAGMENT::COMPILATION_FAILED %s\n", infoLog);
        goto end;
    }
    // link shaders
    shaderProgram = glCreateProgram(); // shaderProgram 是多个着色器合并之后并最终链接完成的版本
    glAttachShader(shaderProgram, vertexShader); // 附加
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("ERROR::SHADER::PROGRAM::LINKING_FAILED %s\n", infoLog);
        goto end;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) 0);
    glEnableVertexAttribArray(0);
    // texture coord attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *) (3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    texture = initChain(window_width, window_height);
    while (true) {
        glClearColor(1.0f, 1.0f, 1.00f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindTexture(GL_TEXTURE_2D, texture);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);

        SDL_GL_SwapWindow(window);
    }

    end:
    return 0;
}