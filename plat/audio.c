#include <stddef.h>
#include <stdlib.h>
#include "core.h"
#include "audio.h"

void
dummy_init(struct audio_state *audio)
{
	UNUSED(audio);
}

void
dummy_fini(struct audio_state *audio)
{
	UNUSED(audio);
}

void
dummy_step(struct audio_state *audio)
{
	size_t count = ring_buffer_read_size(&audio->buffer);
	ring_buffer_read_done(&audio->buffer, count);
}

struct audio_io *dummy_io = &(struct audio_io){
	.init = dummy_init,
	.fini = dummy_fini,
	.step = dummy_step,
};

#ifdef CONFIG_JACK
extern struct audio_io *jack_io;
#else
#define jack_io NULL
#endif

#ifdef CONFIG_PULSE
extern struct audio_io *pulse_io;
#else
#define pulse_io NULL
#endif

#ifdef CONFIG_MINIAUDIO
extern struct audio_io *miniaudio_io;
#else
#define miniaudio_io NULL
#endif

#ifdef CONFIG_SDL_AUDIO
extern struct audio_io *sdl_audio_io;
#else
#define sdl_audio_io NULL
#endif

struct audio_io *audio_io;

static size_t
frame_size(enum audio_format format)
{
	switch (format) {
	case AUDIO_FORMAT_U8:
		return sizeof(unsigned char);
	case AUDIO_FORMAT_S16:
		return sizeof(short);
	case AUDIO_FORMAT_F32:
		return sizeof(float);
	}
	die("wrong audio format\n");
	return 0;
}

struct audio_state
audio_create(struct audio_config config)
{
	struct audio_state audio = { .config = config };
	size_t frame = config.channels * frame_size(config.format);
	size_t count = 8 * 512;
	void *data = xvmalloc(NULL, 0, count * frame);

	audio.buffer = ring_buffer_init(data, count, frame);

	return audio;
}

void
audio_init(struct audio_state *audio)
{
	/* grab the first available audio backend */
	if (!audio_io && jack_io)
		audio_io = jack_io;
	if (!audio_io && pulse_io)
		audio_io = pulse_io;
	if (!audio_io && miniaudio_io)
		audio_io = miniaudio_io;
	if (!audio_io && sdl_audio_io)
		audio_io = sdl_audio_io;
	if (!audio_io)
		audio_io = dummy_io;

	audio_io->init(audio);
}

void
audio_fini(struct audio_state *audio)
{
	audio_io->fini(audio);

	free(audio->buffer.base);
}

void
audio_step(struct audio_state *audio)
{
	audio_io->step(audio);
}
