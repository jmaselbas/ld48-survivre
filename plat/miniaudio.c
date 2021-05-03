#include <stddef.h>
#include <string.h>
#include "plat/core.h"
#include "plat/audio.h"

#include "miniaudio.h"

static ma_device device;

static volatile int quit;
static ma_event cond;

static struct audio_state *audio_state;

static void
miniaudio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	struct audio_state *audio = audio_state;
	int channels = audio->config.channels;
	size_t count;
	float *data;

	UNUSED(pDevice);
	UNUSED(pInput);

	while (frameCount > 0 && !quit) {
		count = ring_buffer_read_size(&audio->buffer);
		data  = ring_buffer_read_addr(&audio->buffer);
		count = MIN(count, frameCount);
		if (count > 0) {
			ma_copy_pcm_frames(pOutput, data, count, ma_format_f32, channels);
			ring_buffer_read_done(&audio->buffer, count);
			pOutput += count * ma_get_bytes_per_frame(ma_format_f32, channels);
			frameCount -= count;
		} else {
			/* no more audio to write */
			ma_event_wait(&cond);
		}
	}

	if (frameCount > 0)
		ma_silence_pcm_frames(pOutput, frameCount, ma_format_f32, channels);
}

static void
miniaudio_init(struct audio_state *audio)
{
	ma_device_config config  = ma_device_config_init(ma_device_type_playback);

        config.playback.format   = ma_format_f32; /* TODO: handle format type conversion */
        config.playback.channels = audio->config.channels;
        config.sampleRate        = audio->config.samplerate;
        config.dataCallback      = miniaudio_callback;
        config.pUserData         = audio;
	audio_state = audio;

        if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
		die("Failed to initialize miniaudio\n");
        }

	if (ma_event_init(&cond) != MA_SUCCESS)
		die("ma_event_init() error");
	quit = 0;

        ma_device_start(&device);
}

static void
miniaudio_fini(struct audio_state *audio)
{
	UNUSED(audio);
	quit = 1;
	ma_event_uninit(&cond);
}

static void
miniaudio_step(struct audio_state *audio)
{
	UNUSED(audio);
	/* signal audio thread */
	ma_event_signal(&cond);
}

struct audio_io *miniaudio_io = &(struct audio_io) {
	.init = miniaudio_init,
	.fini = miniaudio_fini,
	.step = miniaudio_step,
};
