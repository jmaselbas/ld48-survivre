#ifndef GAME_H
#define GAME_H

#include <time.h>

#include "engine/engine.h"

typedef int64_t (file_size_t)(const char *path);
typedef int64_t (file_read_t)(const char *path, void *buf, size_t size);
typedef time_t (file_time_t)(const char *path);

struct file_io {
	file_size_t *size;
	file_read_t *read;
	file_time_t *time;
};

typedef void (window_close_t)(void);
typedef void (window_cursor_t)(int show);
struct window_io {
	window_close_t *close;  /* request window to be closed */
	window_cursor_t *cursor; /* request cursor to be shown */
};

struct game_memory {
	struct memory_zone state;
	struct memory_zone asset;
	struct memory_zone scrap;
	struct memory_zone audio;
};

/* typedef for function type */
typedef void (game_init_t)(struct game_memory *memory, struct file_io *file_io, struct window_io *win_io);
typedef void (game_step_t)(struct game_memory *memory, struct input *input, struct audio *audio);
typedef void (game_fini_t)(struct game_memory *memory);

/* declare functions signature */
game_init_t game_init;
game_step_t game_step;
game_fini_t game_fini;
#endif
