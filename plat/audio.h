#include "../engine/ring_buffer.h"

enum audio_format {
	AUDIO_FORMAT_U8,
	AUDIO_FORMAT_S16,
	AUDIO_FORMAT_F32,
};

struct audio_config {
	enum audio_format format;
	unsigned int channels;
	unsigned int samplerate;
};

struct audio_state {
	struct audio_config config;
	struct ring_buffer buffer;
	void *priv;
};

typedef void (audio_init_t)(struct audio_state *);
typedef void (audio_fini_t)(struct audio_state *);
typedef void (audio_step_t)(struct audio_state *);

struct audio_io {
	audio_init_t *init;
	audio_fini_t *fini;
	audio_step_t *step;
};

struct audio_state audio_create(struct audio_config);
void audio_init(struct audio_state *);
void audio_fini(struct audio_state *);
void audio_step(struct audio_state *);
