#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifndef CONFIG_LIBDIR
#define CONFIG_LIBDIR ""
#endif
#ifndef STATIC
#include <dlfcn.h>
#endif

#include <glad.h>
#include <SDL.h>

#include "engine/engine.h"
#include "game/game.h"
#include "plat/core.h"
#include "plat/audio.h"

#ifdef STATIC
extern game_init_t game_init;
extern game_step_t game_step;
extern game_fini_t game_fini;
#endif

#define MSEC_PER_SEC 1000
static double
window_get_time(void)
{
	return SDL_GetTicks() / (double) MSEC_PER_SEC;
}

static double
rate_limit(double rate)
{
	static double tlast;
	double tnext, tcurr;
	struct timespec ts;
	double period = 1 / (double) rate;

	tnext = tlast + period;
	tcurr = window_get_time() + 0.0001; /* plus some time for the overhead */
	if (tcurr < tnext) {
		ts.tv_sec = 0;
		ts.tv_nsec = 1000000000 * (tnext - tcurr);
		nanosleep(&ts, NULL);
	}
	/* get current frame rate */
	tcurr = window_get_time();
	rate = 1.0 / (double) (tcurr - tlast);
	tlast = tcurr;
	return rate;
}

struct file_io file_io = {
	.size = file_size,
	.read = file_read,
	.time = file_time,
};

SDL_Window *window;
SDL_GLContext context;
unsigned int width = 1080;
unsigned int height = 800;
int should_close;
int focused;
int show_cursor;
static int xpre, ypre;

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
	should_close = 1;
}

static void
request_cursor(int show)
{
	show_cursor = show;
	SDL_SetRelativeMouseMode(show_cursor ? SDL_FALSE : SDL_TRUE);
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
	new->time = window_get_time();
	/* update window's framebuffer size */
	new->width = width;
	new->height = height;

	/* flush xinc and yinc */
	buf->xinc = 0;
	buf->yinc = 0;
}

static void
window_init(char *name)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO))
		die("SDL init failed: %s\n", SDL_GetError());

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, SDL_TRUE);
	SDL_GL_SetSwapInterval(1);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	window = SDL_CreateWindow(name, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
				  width, height,
				  SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
				  | SDL_WINDOW_INPUT_FOCUS
				  | SDL_WINDOW_MOUSE_FOCUS
				  | SDL_WINDOW_MAXIMIZED
				  );

	if (!window)
		die("Failed to create window: %s\n", SDL_GetError());

	context = SDL_GL_CreateContext(window);
	if (!context)
		die("Failed to create openGL context: %s\n", SDL_GetError());

	show_cursor = 1;
	SDL_SetRelativeMouseMode(show_cursor ? SDL_FALSE : SDL_TRUE);

	if (!gladLoadGLES2Loader((GLADloadproc) SDL_GL_GetProcAddress))
		die("GL init failed\n");

	glViewport(0, 0, width, height);
}

static void
window_fini(void)
{
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static int
window_should_close(void)
{
	return should_close;
}

static void
window_swap_buffers(void)
{
	SDL_GL_SwapWindow(window);
}

static void
focus_event(int focus)
{
	focused = focus;
	if (focused) {
		SDL_SetRelativeMouseMode(show_cursor ? SDL_FALSE : SDL_TRUE);
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
}

static void
mouse_motion_event(int xpos, int ypos, int xinc, int yinc)
{
	struct game_input *input = &game_input_next;

	if (focused) {
		input->xpos = xpos;
		input->ypos = ypos;
		input->xinc += xinc;
		input->yinc += yinc;
	}
	xpre = xpos;
	ypre = ypos;
}

static void
mouse_button_event(int button, int act)
{
	struct game_input *input = &game_input_next;

	if (button >= 0 && button < (int)ARRAY_LEN(input->buttons))
		input->buttons[button] = act;
}

static void
key_event(int key, int mod, int act)
{
	struct game_input *input = &game_input_next;

	if (key == KEY_BACKSPACE) {
		/* TODO: fix key mod */ 
		if ((mod & (KMOD_ALT | KMOD_CTRL))
			== (KMOD_ALT | KMOD_CTRL))
			should_close = 1;
	}

	if ((unsigned int)key < ARRAY_LEN(input->keys))
		input->keys[key] = act;
}

static int
map_key(SDL_Keycode sym)
{
	switch (sym) {
	case SDLK_UNKNOWN:
	default: return 0;
	case SDLK_RETURN: return KEY_ENTER;
	case SDLK_ESCAPE: return KEY_ESCAPE;
	case SDLK_BACKSPACE: return KEY_BACKSPACE;
	case SDLK_TAB: return KEY_TAB;
	case SDLK_SPACE: return KEY_SPACE;
	case SDLK_a: return KEY_A;
	case SDLK_b: return KEY_B;
	case SDLK_c: return KEY_C;
	case SDLK_d: return KEY_D;
	case SDLK_e: return KEY_E;
	case SDLK_f: return KEY_F;
	case SDLK_g: return KEY_G;
	case SDLK_h: return KEY_H;
	case SDLK_i: return KEY_I;
	case SDLK_j: return KEY_J;
	case SDLK_k: return KEY_K;
	case SDLK_l: return KEY_L;
	case SDLK_m: return KEY_M;
	case SDLK_n: return KEY_N;
	case SDLK_o: return KEY_O;
	case SDLK_p: return KEY_P;
	case SDLK_q: return KEY_Q;
	case SDLK_r: return KEY_R;
	case SDLK_s: return KEY_S;
	case SDLK_t: return KEY_T;
	case SDLK_u: return KEY_U;
	case SDLK_v: return KEY_V;
	case SDLK_w: return KEY_W;
	case SDLK_x: return KEY_X;
	case SDLK_y: return KEY_Y;
	case SDLK_z: return KEY_Z;
	case SDLK_RIGHT: return KEY_RIGHT;
	case SDLK_LEFT:  return KEY_LEFT;
	case SDLK_DOWN:  return KEY_DOWN;
	case SDLK_UP:    return KEY_UP;
	}
}

static void
window_poll_events(void)
{
	SDL_Event e;
	int key, mod, act;
	int w, h;

	SDL_GL_GetDrawableSize(window, &w, &h);
	width  = (w < 0) ? 0 : (int) w;
	height = (h < 0) ? 0 : (int) h;

	while (SDL_PollEvent(&e)) {
		switch(e.type) {
		case SDL_QUIT:
			should_close = 1;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			key = map_key(e.key.keysym.sym);
			mod = e.key.keysym.mod;
			act = e.key.state == SDL_PRESSED ? KEY_PRESSED : KEY_RELEASED;
			key_event(key, mod, act);
			break;
		case SDL_MOUSEMOTION:
			mouse_motion_event(e.motion.x, e.motion.y, e.motion.xrel, e.motion.yrel);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			act = e.button.state == SDL_PRESSED ? KEY_PRESSED : KEY_RELEASED;
			mouse_button_event(e.button.button - 1, act);
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				focus_event(1);
			else if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				focus_event(0);
			break;
		}
	}
}

struct libgame libgame = {
	.init = game_init,
	.step = game_step,
	.fini = game_fini,
};

static void
libgame_reload(void)
{
#ifndef STATIC
	time_t time;
	int ret;

	libgame.init = NULL;
	libgame.step = NULL;
	libgame.fini = NULL;

	if (libgame.handle) {
		ret = dlclose(libgame.handle);
		if (ret)
			fprintf(stderr, "dlclose failed\n");
	}

	time = file_time(CONFIG_LIBDIR"libgame.so");

	/* clear previous error message  */
	dlerror();

	libgame.handle = dlopen("libgame.so", RTLD_NOW);

	if (libgame.handle) {
		libgame.init = dlsym(libgame.handle, "game_init");
		libgame.step = dlsym(libgame.handle, "game_step");
		libgame.fini = dlsym(libgame.handle, "game_fini");
		libgame.time = time;
	}
#endif
}

static int
libgame_changed(void)
{
#ifndef STATIC
	time_t new_time;

	new_time = file_time(CONFIG_LIBDIR"libgame.so");

	return libgame.time < new_time;
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

static void
main_loop_step(void)
{
	window_poll_events();

	/* get an audio buffer */
	game_audio.size   = ring_buffer_write_size(&audio_state.buffer);
	game_audio.buffer = ring_buffer_write_addr(&audio_state.buffer);

	swap_input(&game_input, &game_input_next);
	if (libgame.step)
		libgame.step(&game_memory, &game_input, &game_audio);

	/* finalize audio write */
	ring_buffer_write_done(&audio_state.buffer, game_audio.size);
	audio_step(&audio_state);

	window_swap_buffers();
}

#ifdef __EMSCRIPTEN__
void emscripten_set_main_loop(void (* f)(), int, int);
#define main_loop_step() \
	{ emscripten_set_main_loop(main_loop_step, 0, 10); return 0; } while (0)
#endif

int
main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "-v") == 0) {
		printf("version %s\n", VERSION);
		return 0;
	}

	alloc_game_memory(&game_memory);

	libgame_reload();

	window_init(argv[0]);

	if (libgame.init)
		libgame.init(&game_memory, &file_io, &glfw_io);

	audio_state = audio_create(audio_config);
	audio_init(&audio_state);

	while (!window_should_close()) {
		if (libgame_changed())
			libgame_reload();
		main_loop_step();
		rate_limit(300);
	}

	if (libgame.fini)
		libgame.fini(&game_memory);

	window_fini();

	audio_fini(&audio_state);

	return 0;
}
