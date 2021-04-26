#include <stddef.h>
#include <string.h>
#include "core.h"
#include "audio.h"

#include "miniaudio.h"

static ma_device device;

static pthread_mutex_t mutex;
static pthread_cond_t cond;
static volatile int quit;

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
			pthread_cond_wait(&cond, &mutex);
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

	if (pthread_mutex_init(&mutex, NULL) != 0)
		die("pthread_mutex_init() error");

	if (pthread_cond_init(&cond, NULL) != 0)
		die("pthread_cond_init() error");
	quit = 0;

        ma_device_start(&device);
}

static void
miniaudio_fini(struct audio_state *audio)
{
	UNUSED(audio);
	quit = 1;
	pthread_cond_signal(&cond);
	ma_device_uninit(&device);
}

static void
miniaudio_step(struct audio_state *audio)
{
	UNUSED(audio);
	/* signal audio thread */
	pthread_cond_signal(&cond);	
}

struct audio_io *miniaudio_io = &(struct audio_io) {
	.init = miniaudio_init,
	.fini = miniaudio_fini,
	.step = miniaudio_step,
};
