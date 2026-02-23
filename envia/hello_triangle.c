// libegl-dev
#include <EGL/egl.h>
#include <EGL/eglext.h>
// libgles-dev
#include <GLES3/gl3.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Common/Include/esUtil.h

#define ES_WINDOW_RGB         0
#define ES_WINDOW_ALPHA       1
#define ES_WINDOW_DEPTH       2
#define ES_WINDOW_STENCIL     4
#define ES_WINDOW_MULTISAMPLE 8

typedef struct ESContext ESContext;

struct ESContext
{
	void *platform_data;
	void *user_data;

	GLint width;
	GLint height;

	EGLNativeDisplayType egl_native_display;
	EGLNativeWindowType egl_native_window;
	EGLDisplay egl_display;
	EGLContext egl_context;
	EGLSurface egl_surface;

	void (*draw_func)(ESContext *);
	void (*shutdown_func)(ESContext *);
	void (*key_func)(ESContext *, unsigned char, int, int);
	void (*update_func)(ESContext *, float delta_time);
};

// Common/Source/LinuxX11/esUtil_X11.c

Display *x_display = NULL;
Atom x_wm_delete_window = None;

EGLBoolean x_win_create(ESContext *es_context, const char *title)
{
	x_display = XOpenDisplay(/* display_name = */ NULL);
	if (x_display == NULL) {
		return EGL_FALSE;
	}

	Window root = DefaultRootWindow(x_display);

	XSetWindowAttributes create_attr;
	create_attr.event_mask =
			ExposureMask | PointerMotionMask | KeyPressMask;
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
			/* attributes = */ &create_attr);

	x_wm_delete_window = XInternAtom(x_display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(x_display, win, &x_wm_delete_window, 1);

	XSetWindowAttributes change_attr;
	change_attr.override_redirect = False;
	XChangeWindowAttributes(
			/* display = */ x_display,
			/* w = */ win,
			/* valuemask = */ CWOverrideRedirect,
			/* attributes = */ &change_attr);

	XWMHints hints;
	hints.flags = InputHint;
	hints.input = True;
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

GLboolean x_user_interrupt(ESContext *es_context)
{
	GLboolean user_interrupt = GL_FALSE;

	while (XPending(x_display)) {
		XEvent xev;
		XNextEvent(x_display, &xev);
		if (xev.type == KeyPress) {
			KeySym key;
			char text;
			if (XLookupString(&xev.xkey, &text, 1, &key, 0) == 1) {
				if (es_context->key_func != NULL) {
					es_context->key_func(
							es_context, text, 0, 0);
				}
			}
		}
		if (xev.type == ClientMessage) {
			if (xev.xclient.data.l[0] == x_wm_delete_window) {
				user_interrupt = GL_TRUE;
			}
		}
		if (xev.type == DestroyNotify) {
			user_interrupt = GL_TRUE;
		}
	}

	return user_interrupt;
}

void x_win_loop(ESContext *es_context)
{
	struct timeval t1, t2;
	struct timezone tz;
	float deltatime;

	gettimeofday(&t1, &tz);

	while (x_user_interrupt(es_context) == GL_FALSE) {
		gettimeofday(&t2, &tz);
		deltatime =
				t2.tv_sec - t1.tv_sec +
				(t2.tv_usec - t1.tv_usec) * 1e-6;
		t1 = t2;

		if (es_context->update_func != NULL) {
			es_context->update_func(es_context, deltatime);
		}
		if (es_context->draw_func != NULL) {
			es_context->draw_func(es_context);
		}

		eglSwapBuffers(
				es_context->egl_display,
				es_context->egl_surface);
	}
}

// Common/Source/esUtil.c

EGLint es_get_context_renderable_type ( EGLDisplay egl_display )
{
#ifdef EGL_KHR_create_context
	const char *extensions = eglQueryString(egl_display, EGL_EXTENSIONS);

	if (extensions != NULL && strstr(extensions, "EGL_KHR_create_context"))
	{
		return EGL_OPENGL_ES3_BIT_KHR;
	}
#endif
	return EGL_OPENGL_ES2_BIT;
}

GLboolean es_config(ESContext *es_context, GLuint flags, EGLConfig *config)
{
	EGLint num_configs = 0;
	EGLint attrib_list[] = {
			EGL_RED_SIZE,
			5,
			EGL_GREEN_SIZE,
			6,
			EGL_BLUE_SIZE,
			5,
			EGL_ALPHA_SIZE,
			(flags & ES_WINDOW_ALPHA) ? 8 : EGL_DONT_CARE,
			EGL_DEPTH_SIZE,
			(flags & ES_WINDOW_DEPTH) ? 8 : EGL_DONT_CARE,
			EGL_STENCIL_SIZE,
			(flags & ES_WINDOW_STENCIL) ? 8 : EGL_DONT_CARE,
			EGL_SAMPLE_BUFFERS,
			(flags & ES_WINDOW_MULTISAMPLE) ? 1 : 0,
			EGL_RENDERABLE_TYPE,
			es_get_context_renderable_type(es_context->egl_display),
			EGL_NONE};

	if (!eglChooseConfig(
			es_context->egl_display,
			attrib_list,
			config,
			1,
			&num_configs)) {
		return GL_FALSE;
	}

	if (num_configs < 1) {
		return GL_FALSE;
	}

	return GL_TRUE;
}

GLboolean es_create_window(
		ESContext *es_context,
		const char *title,
		GLint width,
		GLint height,
		GLuint flags)
{
	if (es_context == NULL) {
		return GL_FALSE;
	}

	es_context->width = width;
	es_context->height = height;

	if (!x_win_create(es_context, title)) {
		return GL_FALSE;
	}

	es_context->egl_display = eglGetDisplay(es_context->egl_native_display);
	if (es_context->egl_display == EGL_NO_DISPLAY) {
		return GL_FALSE;
	}

	EGLint major_version;
	EGLint minor_version;
	if (!eglInitialize(
			es_context->egl_display,
			&major_version,
			&minor_version)) {
		return GL_FALSE;
	}

	EGLConfig config;
	if (es_config(es_context, flags, &config) == GL_FALSE) {
		return GL_FALSE;
	}

	es_context->egl_surface = eglCreateWindowSurface(
			es_context->egl_display,
			config,
			es_context->egl_native_window,
			NULL);

	if (es_context->egl_surface == EGL_NO_SURFACE) {
		return GL_FALSE;
	}

	EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
	es_context->egl_context = eglCreateContext(
			es_context->egl_display,
			config,
			EGL_NO_CONTEXT,
			context_attribs);

	if (es_context->egl_context == EGL_NO_CONTEXT) {
		return GL_FALSE;
	}

	if (!eglMakeCurrent(
			es_context->egl_display,
			es_context->egl_surface,
			es_context->egl_surface,
			es_context->egl_context)) {
		return GL_FALSE;
	}

	return GL_TRUE;
}

void es_register_draw_func(
		ESContext *es_context, void (*draw_func)(ESContext *))
{
	es_context->draw_func = draw_func;
}

void es_register_shutdown_func(
		ESContext *es_context, void (*shutdown_func)(ESContext *))
{
	es_context->shutdown_func = shutdown_func;
}

void es_register_update_func(
		ESContext *es_context, void (*update_func)(ESContext *, float))
{
	es_context->update_func = update_func;
}

void es_register_key_func(
		ESContext *es_context,
		void (*key_func)(ESContext *, unsigned char, int, int))
{
	es_context->key_func = key_func;
}

void es_log_message(const char *format_str, ...)
{
	va_list params;
	char buf[BUFSIZ];

	va_start(params, format_str);
	vsprintf(buf, format_str, params);

	printf("%s", buf);

	va_end(params);
}

// Chapter_2/Hello_Triangle/Hello_Triangle.c

typedef struct
{
	GLuint program_object;
} UserData;

GLuint ht_load_shader(GLenum type, const char *shader_src)
{
	GLuint shader = glCreateShader(type);

	if (shader == 0) {
		return 0;
	}

	glShaderSource(shader, 1, &shader_src, NULL);

	glCompileShader(shader);

	GLint compiled;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if (info_len > 1) {
			char *info_log = malloc(sizeof(char) * info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log);
			es_log_message(
					"Error compiling shader:\n%s\n",
					info_log);
			free(info_log);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

GLboolean ht_init(ESContext *es_context)
{
	UserData *user_data = es_context->user_data;
	char v_shader_str[] =
			"#version 300 es                          \n"
			"layout(location = 0) in vec4 vPosition;  \n"
			"void main()                              \n"
			"{                                        \n"
			"   gl_Position = vPosition;              \n"
			"}                                        \n";

	char f_shader_str[] =
			"#version 300 es                              \n"
			"precision mediump float;                     \n"
			"out vec4 fragColor;                          \n"
			"void main()                                  \n"
			"{                                            \n"
			"   fragColor = vec4 ( 1.0, 0.0, 0.0, 1.0 );  \n"
			"}                                            \n";

	GLuint vertex_shader = ht_load_shader(GL_VERTEX_SHADER, v_shader_str);
	GLuint fragment_shader = ht_load_shader(
			GL_FRAGMENT_SHADER, f_shader_str);

	GLuint program_object = glCreateProgram();

	if (program_object == 0) {
		return GL_FALSE;
	}

	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);

	glLinkProgram(program_object);

	GLint linked;
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);

	if (!linked) {
		GLint info_len = 0;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
		if (info_len > 1) {
			char *info_log = malloc(sizeof(char) * info_len);
			glGetProgramInfoLog(
					program_object,
					info_len,
					NULL,
					info_log);
			es_log_message(
					"Error linking program:\n%s\n",
					info_log);
			free(info_log);
		}

		glDeleteProgram(program_object);
		return GL_FALSE;
	}

	user_data->program_object = program_object;

	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	return GL_TRUE;
}

void ht_draw(ESContext *es_context)
{
	UserData *user_data = es_context->user_data;
	GLfloat v_vertices[] = {
			0.0f, 0.5f, 0.0f,
			-0.5f, -0.5f, 0.0f,
			0.5f, -0.5f, 0.0f};

	glViewport(0, 0, es_context->width, es_context->height);

	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(user_data->program_object);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, v_vertices);
	glEnableVertexAttribArray(0);

	glDrawArrays(GL_TRIANGLES, 0, 3);
}

void ht_shutdown(ESContext *es_context)
{
	UserData *user_data = es_context->user_data;

	glDeleteProgram(user_data->program_object);

	free(user_data);
}

GLboolean ht_main(ESContext *es_context)
{
	es_context->user_data = malloc(sizeof(UserData));

	if (!es_create_window(
			es_context,
			"Hello Triangle",
			320,
			240,
			ES_WINDOW_RGB)) {
		return GL_FALSE;
	}

	if (!ht_init(es_context)) {
		return GL_FALSE;
	}

	es_register_shutdown_func(es_context, ht_shutdown);
	es_register_draw_func(es_context, ht_draw);

	return GL_TRUE;
}

// Common/Source/LinuxX11/esUtil_X11.c

int main()
{
	ESContext es_context;
	memset(&es_context, 0, sizeof(es_context));

	if (ht_main(&es_context) != GL_TRUE) {
		return EXIT_FAILURE;
	}

	x_win_loop(&es_context);

	if (es_context.shutdown_func != NULL) {
		es_context.shutdown_func(&es_context);
	}

	return EXIT_SUCCESS;
}
