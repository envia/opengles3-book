#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>

typedef struct ESContext ESContext;

struct ESContext
{
	GLint width;
	GLint height;
};

Display *x_display = NULL;

EGLBoolean x_win_create(ESContext *es_context, const char *title)
{
	Window root;
	XSetWindowAttributes swa;
	Window win;

	x_display = XOpenDisplay(NULL);
	if (x_display == NULL) {
		return EGL_FALSE;
	}

	root = DefaultRootWindow(x_display);

	swa.event_mask = ExposureMask | PointerMotionMask | KeyPressMask;
	win = XCreateWindow(x_display, root, 0, 0, es_context->width, es_context->height, 0, CopyFromParent, InputOutput, CopyFromParent, CWEventMask, &swa);

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
