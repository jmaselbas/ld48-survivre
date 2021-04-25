#include <stddef.h>
#include <stdlib.h>
#include "core.h"
#include "audio.h"

struct ring_buffer *dummy_buffer;

void
dummy_init(struct ring_buffer *audio_buffer)
{
	size_t size = 512 * 2;
	float *base = xvmalloc(NULL, 0, size * sizeof(float));
	*audio_buffer = ring_buffer_init(base, size, sizeof(float));
	dummy_buffer = audio_buffer;
}

void
dummy_fini(void)
{
	free(dummy_buffer->base);
}

void
dummy_step(void)
{
	size_t count = ring_buffer_read_size(dummy_buffer);
	ring_buffer_read_done(dummy_buffer, count);
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

struct audio_io *audio_io;

void
audio_init(struct ring_buffer *audio_buffer)
{
	/* grab the first available audio backend */
	if (!audio_io && jack_io)
		audio_io = jack_io;
	if (!audio_io && pulse_io)
		audio_io = pulse_io;
	if (!audio_io && miniaudio_io)
		audio_io = miniaudio_io;
	if (!audio_io)
		audio_io = dummy_io;

	audio_io->init(audio_buffer);
}

void
audio_fini(void)
{
	audio_io->fini();
}

void
audio_step(void)
{
	audio_io->step();
}
