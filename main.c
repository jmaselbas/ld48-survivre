#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifndef STATIC
#include <dlfcn.h>
#endif

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "engine/engine.h"
#include "game/game.h"
#include "plat/core.h"
#include "plat/audio.h"

#ifdef STATIC
extern game_init_t game_init;
extern game_step_t game_step;
extern game_fini_t game_fini;
#endif

static double
rate_limit(double rate)
{
	static double tlast;
	double tnext, tcurr;
	struct timespec ts;
	double period = 1 / (double) rate;

	tnext = tlast + period;
	tcurr = glfwGetTime() + 0.0001; /* plus some time for the overhead */
	if (tcurr < tnext) {
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 * (tnext - tcurr);
		nanosleep(&ts, NULL);
	}
	/* get current frame rate */
	tcurr = glfwGetTime();
	rate = 1.0 / (double) (tcurr - tlast);
	tlast = tcurr;
	return rate;
}

struct file_io file_io = {
	.size = file_size,
	.read = file_read,
	.time = file_time,
};

GLFWwindow *window;
unsigned int width = 1080;
unsigned int height = 800;
int focused;
int show_cursor;
static double xpre, ypre;

struct game_input game_input_next;
struct game_input game_input;
struct game_audio game_audio;
struct game_memory game_memory;

struct audio_config audio_config = {
	.samplerate = 48000,
	.channels = 2,
	.format = AUDIO_FORMAT_F32,
};

struct audio_state audio_state;

struct libgame {
	void *handle;
	time_t time;
	game_init_t *init;
	game_step_t *step;
	game_fini_t *fini;
};

static void
request_close(void)
{
	glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void
request_cursor(int show)
{
	show_cursor = show ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED;

	glfwSetInputMode(window, GLFW_CURSOR, show_cursor);
}

struct window_io glfw_io = {
	.close = request_close,
	.cursor = request_cursor,
};

static void
swap_input(struct game_input *new, struct game_input *buf)
{
	/* input buffering: this is to make sure that input callbacks
	 * doesn't modify the game input while the game step runs. */

	/* copy buffered input into the new input buffer */
	*new = *buf;
	/* grab time */
	new->time = glfwGetTime();
	/* update window's framebuffer size */
	new->width = width;
	new->height = height;

	/* flush xinc and yinc */
	buf->xinc = 0;
	buf->yinc = 0;
}

static void
framebuffer_callback(GLFWwindow *window, int w, int h)
{
	UNUSED(window);

	width = w;
	height = h;
}

static void
cursor_position_callback(GLFWwindow *window, double xpos, double ypos)
{
	struct game_input *input = &game_input_next;
	UNUSED(window);

	if (focused) {
		input->xpos = xpos;
		input->ypos = ypos;
		input->xinc += xpos - xpre;
		input->yinc += ypos - ypre;
	}
	xpre = xpos;
	ypre = ypos;
}

static void
mouse_callback(GLFWwindow* window, int button, int action, int mods)
{
	struct game_input *input = &game_input_next;

	if (button >= 0 && button < ARRAY_LEN(input->buttons))
		input->buttons[button] = action;
}

static void
focus_callback(GLFWwindow *window, int focus)
{
	focused = focus;
	if (focused)
		glfwSetInputMode(window, GLFW_CURSOR, show_cursor);
	else
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

static void
key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	struct game_input *input = &game_input_next;
	UNUSED(window);
	UNUSED(scancode);
	UNUSED(mods);

	if (key == GLFW_KEY_BACKSPACE)
		if ((mods & (GLFW_MOD_ALT | GLFW_MOD_CONTROL))
			==  (GLFW_MOD_ALT | GLFW_MOD_CONTROL))
			glfwSetWindowShouldClose(window, GLFW_TRUE);

	if (action == GLFW_REPEAT)
		return;

	if ((unsigned int)key < ARRAY_LEN(input->keys))
		input->keys[key] = action;
}

static void
glfw_init(char *app_name)
{
	GLFWmonitor *monitor;
	const GLFWvidmode *mode;
	int w, h;

	if (!glfwInit())
		die("GLFW init failed\n");

	glfwWindowHint(GLFW_RED_BITS, 8);
	glfwWindowHint(GLFW_GREEN_BITS, 8);
	glfwWindowHint(GLFW_BLUE_BITS, 8);
	glfwWindowHint(GLFW_ALPHA_BITS, 8);
	glfwWindowHint(GLFW_DEPTH_BITS, 24);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);

	glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_REFRESH_RATE, 60);
	glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);

	/* Try to grab better default width and height */
	monitor = glfwGetPrimaryMonitor();
	if (monitor) {
		mode = glfwGetVideoMode(monitor);
		if (mode) {
			width = mode->width;
			height = mode->height;
		}
	}

	window = glfwCreateWindow(width, height, app_name, NULL, NULL);
	if (!window)
		die("create window failed\n");

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	glfwGetFramebufferSize(window, &w, &h);
	width  = (w < 0) ? 0 : w;
	height = (h < 0) ? 0 : h;
	glfwSetFramebufferSizeCallback(window, framebuffer_callback);

	glfwSetWindowFocusCallback(window, focus_callback);
	glfwSetMouseButtonCallback(window, mouse_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwGetCursorPos(window, &xpre, &ypre);

	glfwSetKeyCallback(window, key_callback);
	show_cursor = GLFW_CURSOR_DISABLED;
	glfwSetInputMode(window, GLFW_CURSOR, show_cursor);
}

static void
glfw_fini(void)
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

static void
libgame_reload(struct libgame *libgame)
{
#ifndef STATIC
	time_t time;
	int ret;

	libgame->init = NULL;
	libgame->step = NULL;
	libgame->fini = NULL;

	if (libgame->handle) {
		ret = dlclose(libgame->handle);
		if (ret)
			fprintf(stderr, "dlcose failed\n");
	}

	time = file_time("libgame.so");

	/* clear previous error message  */
	dlerror();

	libgame->handle = dlopen("libgame.so", RTLD_NOW);

	if (libgame->handle) {
		libgame->init = dlsym(libgame->handle, "game_init");
		libgame->step = dlsym(libgame->handle, "game_step");
		libgame->fini = dlsym(libgame->handle, "game_fini");
		libgame->time = time;
	}
#endif
}

static int
libgame_changed(struct libgame *libgame)
{
#ifndef STATIC
	time_t new_time;

	new_time = file_time("libgame.so");

	return libgame->time < new_time;
#endif
	return 0;
}

static struct memory_zone
alloc_memory_zone(void *base, size_t align, size_t size)
{
	struct memory_zone zone;

	zone.base = xvmalloc(base, align, size);
	zone.size = size;
	zone.used = 0;

	return zone;
}

static void
alloc_game_memory(struct game_memory *memory)
{
	memory->state = alloc_memory_zone(NULL, SZ_4M, SZ_16M);
	memory->scrap = alloc_memory_zone(NULL, SZ_4M, SZ_16M);
	memory->asset = alloc_memory_zone(NULL, SZ_4M, SZ_16M);
	memory->audio = alloc_memory_zone(NULL, SZ_4M, SZ_16M);
}

int
main(int argc, char **argv)
{
	struct libgame libgame = {
		.init = game_init,
		.step = game_step,
		.fini = game_fini,
	};
	int ret;

	if (argc == 2 && strcmp(argv[1], "-v") == 0) {
		printf("version %s\n", VERSION);
		return 0;
	}

	alloc_game_memory(&game_memory);

	libgame_reload(&libgame);

	glfw_init(argv[0]);

	ret = glewInit();
	if (ret != GLEW_OK)
		die("GLEW init failed: %s\n", glewGetErrorString(ret));

	if (libgame.init)
		libgame.init(&game_memory, &file_io, &glfw_io);

	audio_state = audio_create(audio_config);
	audio_init(&audio_state);
	double rate = 60; /* this is the default target refresh rate */

	while (!glfwWindowShouldClose(window)) {
		if (libgame_changed(&libgame))
			libgame_reload(&libgame);

		glfwPollEvents();

		/* get an audio buffer */
		game_audio.size   = ring_buffer_write_size(&audio_state.buffer);
		game_audio.buffer = ring_buffer_write_addr(&audio_state.buffer);

		swap_input(&game_input, &game_input_next);
		if (libgame.step)
			libgame.step(&game_memory, &game_input, &game_audio);

		/* finalize audio write */
		ring_buffer_write_done(&audio_state.buffer, game_audio.size);
		audio_step(&audio_state);

		glfwSwapBuffers(window);
		rate = rate_limit(300);
	}

	if (libgame.fini)
		libgame.fini(&game_memory);

	glfw_fini();

	audio_fini(&audio_state);

	return 0;
}
