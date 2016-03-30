#define _DEFAULT_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <X11/cursorfont.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define GRAPHICS struct _txf_grctx *
#define unint unsigned int
typedef struct _txf_grctx {
    Window canvas;
    Display *disp;
    GC gc;
    Colormap map;
    unint offsetX;
    unint offsetY;
    unint width;
    unint height;
} _txf_grctx;

unsigned long alphaset(unsigned long color, uint8_t alpha) {
    uint32_t mod = alpha;
    mod <<= 24;
    return (0x00ffffff & color) | mod;
}


unsigned long _getcolor(GRAPHICS g, const char *colstr) {
    XColor color;
    if(!XAllocNamedColor(g->disp, g->map, colstr, &color, &color)) {
        return 0;
    }
    return color.pixel;
}

uint8_t hex(char c) {
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c-'0');
    }
    if (c >= 'a' && c <= 'f') {
        return (uint8_t)(10+c-'a');
    }
    if (c >= 'A' && c <= 'F') {
        return (uint8_t)(10+c-'A');
    }
    return 0;
}


unsigned long getcolor(GRAPHICS g, const char *colstr) {
    char *rgbcs = strdup(colstr);
    char *freeme = rgbcs;
    uint8_t value;
    switch(strlen(colstr)) {
        case 4: // #rgb
        case 7: // #rrggbb
            free(freeme);
            return _getcolor(g, colstr);
        case 9: // #aarrggbb
            value = 16*hex(colstr[1]) + hex(colstr[2]);
            rgbcs += 2;
            break;
        case 5: // #argb
            value = hex(colstr[1]) * 17;
            rgbcs += 1;
            break;
    }
    rgbcs[0] = '#';
    unsigned long result = _getcolor(g, rgbcs);
    result = alphaset(result, value);
    free(freeme);
    return result;
}


void rectdraw();
int main() {
    rectdraw();
}

char** split_args(char* buffer) {
    char* clone = (char*)(malloc(128));
    int   count = 0;
    strncpy(clone, buffer, 128);
    char* toks = strtok (clone," ");
    while (toks != NULL) {
        count++;
        toks = strtok (NULL, " ");
    }
    free(toks);

    char** result = (char**)(malloc(sizeof(char*) * count));
    char** writer = result;

    toks = strtok (buffer," ");
    while (toks != NULL) {
        char* arg = (char*)(malloc(sizeof(char) * (strlen(toks) + 1)));
        strcpy(arg, toks);
        strcat(arg, "\0");
        *(writer++) = arg;
        toks = strtok (NULL, " ");
    }
    free(toks);

    return result;
}

int boxing = 0;
pid_t t;
void *sysexec(void *sys) {
    char *ss = (char *)sys;
    char *syse;
    char count = 0;
rerun:
    syse = strdup(ss);
    t = fork();
    char **ffmpeg = split_args(syse);
    if (t == -1) {
        exit(1);
    }
    if (t > 0) {
        int status;
        waitpid(t, &status, 0);
        printf("%i\n", status);
        if (status == 256 && (count++) < 2) {
            goto rerun;
        }

    } else {
        if (execvp("ffmpeg", ffmpeg) == -1) {
            exit(1);
        }
        exit(0);
    }

    return NULL;
}

int mstime() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec;
}

void rectdraw() {
    Display *d = XOpenDisplay(NULL);
    Window root = XDefaultRootWindow(d);

    XGCValues gcval;
    gcval.foreground = XWhitePixel(d, 0);
    gcval.function = GXxor;
    gcval.background = XBlackPixel(d, 0);
    gcval.plane_mask = gcval.background + gcval.foreground;
    gcval.subwindow_mode = IncludeInferiors;
    GC gc = XCreateGC(d, root,
            GCFunction|GCForeground|GCBackground|GCSubwindowMode, &gcval);

    int grab = XGrabPointer(d, root, False,
            PointerMotionMask | ButtonPressMask,
            GrabModeAsync, GrabModeAsync, root, None, CurrentTime);

    XGrabKey(d,
            XKeysymToKeycode(d, XK_Q),
            AnyModifier, root, False, GrabModeAsync, GrabModeAsync);

    if(grab != GrabSuccess) {
        puts("couldn't grab pointer");
        if (grab == AlreadyGrabbed) {
            puts("already grabbed");
        }
        if (grab == GrabFrozen) {
            puts("grab frozen");
        }
    }

    char *c = "ffmpeg -f alsa -ac 2 -i pulse -video_size %ix%i -f x11grab -i :0.0+%i,%i recording_%i.mp4";
    XVisualInfo vinfo;
    XMatchVisualInfo(d, DefaultScreen(d), 32, TrueColor, &vinfo);
    Colormap cm = XCreateColormap(d, root, vinfo.visual, AllocNone);
    GRAPHICS g = calloc(sizeof(struct _txf_grctx), 1);
    g->disp = d;
    g->map = cm;
    XEvent ev;
    unint sx, sy, ex, ey;
    unint tx, ty, w, h;
    char size[1000] = {0};
    pthread_t systhread;
    while(1) {
        XNextEvent(d, &ev);
        switch(ev.type) {
            case KeyPress:
                kill(t, SIGTERM);
                pthread_join(systhread, NULL);
                exit(0);
                break;
            case KeyRelease:
                break;
            case ButtonPress:
                if (boxing == 0) {
                    sx = ev.xbutton.x;
                    sy = ev.xbutton.y;
                    boxing = 1;
                } else {
                    boxing = 2;
                    XUngrabPointer(d, CurrentTime);
                    sprintf(size, c, w, h, tx, ty, mstime());
                    puts(size);
                    pthread_create(&systhread, NULL, sysexec, size);
                }
                break;
            case MotionNotify:
                ex = ev.xbutton.x;
                ey = ev.xbutton.y;
                if (boxing == 1) {
                    XClearWindow(d, root);
                    XSetForeground(d, gc, getcolor(g, "#ff00ff"));
                    XDrawRectangle(d, root, gc, tx, ty, w, h);
                }
                if (ex > sx) {
                    tx = sx;
                    w = ex-sx;
                } else {
                    tx = ex;
                    w = sx-ex;
                }
                if (ey > sy) {
                    ty = sy;
                    h = ey-sy;
                } else {
                    ty = ey;
                    h = sy-ey;
                }
                if (boxing == 1) {
                    XClearWindow(d, root);
                    XSetForeground(d, gc, getcolor(g, "#ff00ff"));
                    XDrawRectangle(d, root, gc, tx, ty, w, h);
                }
                break;
        }
    }
}


