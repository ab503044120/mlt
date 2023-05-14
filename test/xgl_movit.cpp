#include "movit/effect_chain.h"
#include "movit/input.h"
#include "movit/util.h"
#include "movit/mirror_effect.h"
#include "movit/init.h"
#include "resource_pool.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <fstream>
#include <iostream>


using namespace movit;
using namespace std;

std::string readFileToString(const std::string &filename) {
    std::ifstream input(filename);
    if (!input.is_open()) {
        std::cerr << "failed to open file " << filename << std::endl;
        return "";
    }

    std::string content;
    std::stringstream ss;
    while (input >> ss.rdbuf()) {}
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
//    glActiveTexture(GL_NONE);
    check_error();

    return texnum;
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

Display *dpy;
Window win;

// 创建 OpenGL 环境
GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
XVisualInfo *vi;
GLXContext glc;


static void
no_border(Display *dpy, Window w) {
    static const unsigned MWM_HINTS_DECORATIONS = (1 << 1);
    static const int PROP_MOTIF_WM_HINTS_ELEMENTS = 5;

    typedef struct {
        unsigned long flags;
        unsigned long functions;
        unsigned long decorations;
        long inputMode;
        unsigned long status;
    } PropMotifWmHints;

    PropMotifWmHints motif_hints;
    Atom prop, proptype;
    unsigned long flags = 0;

    /* setup the property */
    motif_hints.flags = MWM_HINTS_DECORATIONS;
    motif_hints.decorations = flags;

    /* get the atom for the property */
    prop = XInternAtom(dpy, "_MOTIF_WM_HINTS", True);
    if (!prop) {
        /* something went wrong! */
        return;
    }

    /* not sure this is correct, seems to work, XA_WM_HINTS didn't work */
    proptype = prop;

    XChangeProperty(dpy, w,                         /* display, window */
                    prop, proptype,                 /* property, type */
                    32,                             /* format: 32-bit datums */
                    PropModeReplace,                /* mode */
                    (unsigned char *) &motif_hints, /* data */
                    PROP_MOTIF_WM_HINTS_ELEMENTS    /* nelements */
    );
}


static GLboolean stereo = GL_FALSE;    /* Enable stereo.  */
static GLint samples = 0;
bool fullscreen = false;
Atom wmDelete;

static void
make_window(Display *dpy, const char *name,
            int x, int y, int width, int height,
            Window *winRet, GLXContext *ctxRet, VisualID *visRet) {
    int attribs[64];
    int i = 0;

    int scrnum;
    XSetWindowAttributes attr;
    unsigned long mask;
    Window root;
    Window win;
    GLXContext ctx;
    XVisualInfo *visinfo;

    /* Singleton attributes. */
    attribs[i++] = GLX_RGBA;
    attribs[i++] = GLX_DOUBLEBUFFER;
    if (stereo)
        attribs[i++] = GLX_STEREO;

    /* Key/value attributes. */
    attribs[i++] = GLX_RED_SIZE;
    attribs[i++] = 8;
    attribs[i++] = GLX_GREEN_SIZE;
    attribs[i++] = 8;
    attribs[i++] = GLX_BLUE_SIZE;
    attribs[i++] = 8;
    attribs[i++] = GLX_DEPTH_SIZE;
    attribs[i++] = 16;
    if (samples > 0) {
        attribs[i++] = GLX_SAMPLE_BUFFERS;
        attribs[i++] = 1;
        attribs[i++] = GLX_SAMPLES;
        attribs[i++] = samples;
    }

    attribs[i++] = None;
    scrnum = DefaultScreen(dpy);
    visinfo = glXChooseVisual(dpy, scrnum, attribs);
    if (!visinfo) {
        printf("Error: couldn't get an RGB, Double-buffered");
        if (stereo)
            printf(", Stereo");
        if (samples > 0)
            printf(", Multisample");
        printf(" visual\n");
        exit(1);
    }
    root = RootWindow(dpy, visinfo->screen);

    /* window attributes */
    attr.border_pixel = 0;
    attr.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask;
    /* XXX this is a bad way to get a borderless window! */
    mask = CWBorderPixel | CWColormap | CWEventMask;

    win = XCreateWindow(dpy, root, x, y, width, height,
                        0, visinfo->depth, InputOutput,
                        visinfo->visual, mask, &attr);

//    if (fullscreen)
//        no_border(dpy, win);
    Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    XSetStandardProperties(dpy, win, "haha", "haha", None, NULL, 0, NULL);
    XMapRaised(dpy, win);
    /* set hints and properties */
    {
        XSizeHints sizehints;
        sizehints.x = x;
        sizehints.y = y;
        sizehints.width = width;
        sizehints.height = height;
        sizehints.flags = USSize | USPosition;
        XSetNormalHints(dpy, win, &sizehints);
        XSetStandardProperties(dpy, win, name, name,
                               None, (char **) NULL, 0, &sizehints);
    }

    ctx = glXCreateContext(dpy, visinfo, NULL, True);
    if (!ctx) {
        printf("Error: glXCreateContext failed\n");
        exit(1);
    }

    *winRet = win;
    *ctxRet = ctx;
    *visRet = visinfo->visualid;

    XFree(visinfo);
}

int main(int argc, char *argv[]) {
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

    unsigned int winWidth = window_width, winHeight = window_height;
    int x = 0, y = 0;
    Display *dpy;
    Window win;
    GLXContext ctx;
    char *dpyName = NULL;
    dpy = XOpenDisplay(dpyName);
    GLboolean printInfo = true;
    VisualID visId;
    if (!dpy) {
        printf("Error: couldn't open display %s\n",
               dpyName ? dpyName : getenv("DISPLAY"));
        return -1;
    }

    if (fullscreen) {
        int scrnum = DefaultScreen(dpy);

        x = 0;
        y = 0;
        winWidth = DisplayWidth(dpy, scrnum);
        winHeight = DisplayHeight(dpy, scrnum);
    }
    make_window(dpy, "glxgears", x, y, winWidth, winHeight, &win, &ctx, &visId);
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, ctx);
//    query_vsync(dpy, win);

    if (printInfo) {
        printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
        printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
        printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
        printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
        printf("VisualID %d, 0x%x\n", (int) visId, (int) visId);
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
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_INT, 0);
        check_error();
        glXSwapBuffers(dpy, win);
    }

    end:
    return 0;

    exit_loop:
    // 释放资源
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return 0;
}