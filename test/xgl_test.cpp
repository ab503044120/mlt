#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <GL/glx.h>

// 三角形点坐标
GLfloat vertices[] = {
        0.0, 0.5,
        -0.5, -0.5,
        0.5, -0.5
};

Display *dpy;
Window win;

// 创建 OpenGL 环境
GLint att[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
XVisualInfo *vi;
GLXContext glc;

// 绘制函数
void draw() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);

    glColor3f(1.0f, 1.0f, 1.0f); // 设置颜色为白色
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisableClientState(GL_VERTEX_ARRAY);

    glXSwapBuffers(dpy, win);
}

static void
no_border( Display *dpy, Window w)
{
    static const unsigned MWM_HINTS_DECORATIONS = (1 << 1);
    static const int PROP_MOTIF_WM_HINTS_ELEMENTS = 5;

    typedef struct
    {
        unsigned long       flags;
        unsigned long       functions;
        unsigned long       decorations;
        long                inputMode;
        unsigned long       status;
    } PropMotifWmHints;

    PropMotifWmHints motif_hints;
    Atom prop, proptype;
    unsigned long flags = 0;

    /* setup the property */
    motif_hints.flags = MWM_HINTS_DECORATIONS;
    motif_hints.decorations = flags;

    /* get the atom for the property */
    prop = XInternAtom( dpy, "_MOTIF_WM_HINTS", True );
    if (!prop) {
        /* something went wrong! */
        return;
    }

    /* not sure this is correct, seems to work, XA_WM_HINTS didn't work */
    proptype = prop;

    XChangeProperty( dpy, w,                         /* display, window */
                     prop, proptype,                 /* property, type */
                     32,                             /* format: 32-bit datums */
                     PropModeReplace,                /* mode */
                     (unsigned char *) &motif_hints, /* data */
                     PROP_MOTIF_WM_HINTS_ELEMENTS    /* nelements */
    );
}


static GLboolean stereo = GL_FALSE;	/* Enable stereo.  */
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
    attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask| ButtonPressMask;
    /* XXX this is a bad way to get a borderless window! */
    mask =  CWBorderPixel | CWColormap | CWEventMask;

    win = XCreateWindow(dpy, root, x, y, width, height,
                        0, visinfo->depth, InputOutput,
                        visinfo->visual, mask, &attr);

//    if (fullscreen)
//        no_border(dpy, win);
    Atom wmDelete = XInternAtom( dpy, "WM_DELETE_WINDOW", True );
    XSetWMProtocols( dpy, win, &wmDelete, 1 );
    XSetStandardProperties(dpy, win, "haha", "haha", None, NULL, 0, NULL );
    XMapRaised( dpy, win );
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
    unsigned int winWidth = 300, winHeight = 300;
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
    // 设置 OpenGL 状态
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // 主循环
    while (1) {
        XEvent event;

        while (XCheckWindowEvent(dpy, win, ~0, &event)) {
            switch (event.type) {
                // 监听窗口关闭事件
                case ClientMessage:
                    if (event.xclient.data.l[0] == (long) wmDelete)
                        goto exit_loop;
                    break;
            }
        }

        draw();
    }

    exit_loop:
    // 释放资源
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, glc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return 0;
}