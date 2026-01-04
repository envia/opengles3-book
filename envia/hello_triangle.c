#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>

typedef struct ESContext ESContext;

struct ESContext
{
	GLint width;
	GLint height;
	EGLNativeDisplayType egl_native_display;
	EGLNativeWindowType  egl_native_window;
};

Display *x_display = NULL;
Atom x_wm_delete_window = None;

EGLBoolean x_win_create(ESContext *es_context, const char *title)
{
	x_display = XOpenDisplay(/* display_name = */ NULL);
	if (x_display == NULL) {
		return EGL_FALSE;
	}

	Window root = DefaultRootWindow(x_display);

	XSetWindowAttributes attr_create;
	attr_create.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;
	Window win = XCreateWindow(
			/* display = */ x_display,
			/* parent = */ root,
			/* x = */ 0,
			/* y = */ 0,
			/* width = */ es_context->width,
			/* height = */ es_context->height,
			/* border_width = */ 0,
			/* depth = */ CopyFromParent,
			/* class = */ InputOutput,
			/* visual = */ CopyFromParent,
			/* valuemask = */ CWEventMask,
			/* attributes = */ &attr_create);

	x_wm_delete_window = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(x_display, win, &x_wm_delete_window, 1);

	XSetWindowAttributes attr_change;
	attr_change.override_redirect = False;
	XChangeWindowAttributes(x_display, win, CWOverrideRedirect, &attr_change);

	XWMHints hints;
	hints.flags = InputHint;
	hints.flags = True;
	XSetWMHints(x_display, win, &hints);

	XMapWindow(x_display, win);

	XStoreName(x_display, win, title);

	Atom wm_state = XInternAtom(x_display, "_NET_WM_STATE", False);
	XEvent xev;
	memset(&xev, 0, sizeof(xev));
	xev.type = ClientMessage;
	xev.xclient.window = win;
	xev.xclient.message_type = wm_state;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = 1;
	xev.xclient.data.l[1] = False;
	XSendEvent(
			/* display = */ x_display,
			/* w = */ root,
			/* propagate = */ False,
			/* event_mask */ SubstructureNotifyMask,
			/* event_send */ &xev);

	es_context->egl_native_window = (EGLNativeWindowType)win;
	es_context->egl_native_display = (EGLNativeDisplayType)x_display;

	return EGL_TRUE;
}

GLboolean es_create_window(ESContext *es_context, const char *title, GLint width, GLint height)
{
	if (es_context == NULL) {
		return GL_FALSE;
	}

	es_context->width = width;
	es_context->height = height;

	if (x_win_create(es_context, title) == EGL_FALSE) {
		return GL_FALSE;
	}

	return GL_TRUE;
}

int es_main(ESContext *es_context)
{
	if (es_create_window(es_context, "Hello Triangle", 320, 240) == GL_FALSE) {
		return GL_FALSE;
	}

	return GL_TRUE;
}

int main()
{
	ESContext es_context;

	memset(&es_context, 0, sizeof(es_context));

	if (es_main(&es_context) != GL_TRUE) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
