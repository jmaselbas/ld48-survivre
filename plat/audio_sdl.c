#include <stddef.h>
#include <string.h>
#include "core.h"
#include "audio.h"

#include <SDL.h>

SDL_AudioDeviceID device;
SDL_AudioSpec spec;

static void
sdl_audio_callback(void *userdata, uint8_t *stream, int size)
{
	struct audio_state *audio = userdata;
	int channels = audio->config.channels;
	size_t frames;
	size_t count;
	float *data;

	count = ring_buffer_read_size(&audio->buffer);
	data  = ring_buffer_read_addr(&audio->buffer);

	frames = size / (channels * sizeof(float));
	count = MIN(count, frames);
	if (count > 0) {
		memcpy(stream, data, count * channels * sizeof(float));
		ring_buffer_read_done(&audio->buffer, count);

		stream += count * channels * sizeof(float);
		frames -= count;
	}
	if (frames > 0)
		memset(stream, 0, frames * channels * sizeof(float));
}

static void
sdl_audio_init(struct audio_state *audio)
{
	SDL_AudioSpec conf = {};

	if (SDL_InitSubSystem(SDL_INIT_AUDIO))
		die("Failed to initialize SDL audio\n");

	conf.format = AUDIO_F32SYS;
	conf.freq = audio->config.samplerate;
	conf.channels = audio->config.channels;
	conf.samples = 2048;
	conf.callback = sdl_audio_callback;
	conf.userdata = audio;
	audio->priv = &device;

	device = SDL_OpenAudioDevice(NULL, 0, &conf, &spec,
				     SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);

	if (device == 0)
		die("Failed to open an audio device\n");

	SDL_PauseAudioDevice(device, 0);
}

static void
sdl_audio_fini(struct audio_state *audio)
{
	UNUSED(audio);

	SDL_PauseAudioDevice(device, 1);
	SDL_CloseAudioDevice(device);
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static void
sdl_audio_step(struct audio_state *audio)
{
	UNUSED(audio);
}

struct audio_io *sdl_audio_io = &(struct audio_io) {
	.init = sdl_audio_init,
	.fini = sdl_audio_fini,
	.step = sdl_audio_step,
};
