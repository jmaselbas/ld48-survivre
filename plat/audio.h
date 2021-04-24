#include "../engine/ring_buffer.h"

typedef void (audio_init_t)(struct ring_buffer *);
typedef void (audio_fini_t)(void);
typedef void (audio_step_t)(void);

struct audio_io {
	audio_init_t *init;
	audio_fini_t *fini;
	audio_step_t *step;
};

void audio_init(struct ring_buffer *);
void audio_fini(void);
void audio_step(void);
