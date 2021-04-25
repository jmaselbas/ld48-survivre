#include <stddef.h>
#include <string.h>
#include "core.h"
#include "audio.h"

#include "miniaudio.h"

static ma_device device;
struct ring_buffer *ring_buffer;
size_t channels = 1;
size_t samplerate = 48000;

static void
miniaudio_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
        // In playback mode copy data to pOutput. In capture mode read data from pInput. In full-duplex mode, both
        // pOutput and pInput will be valid and you can move data from pInput into pOutput. Never process more than
        // frameCount frames.
	size_t count;
	float *audio;

	UNUSED(pDevice);
	UNUSED(pInput);

	count = ring_buffer_read_size(ring_buffer);
	audio = ring_buffer_read_addr(ring_buffer);
	count = MIN(count, frameCount);
	if (count > 0) {
		ma_copy_pcm_frames(pOutput, audio, count, ma_format_f32, channels);
		ring_buffer_read_done(ring_buffer, count);
		frameCount -= count;
	}

	if (frameCount > 0)
		ma_silence_pcm_frames(pOutput, frameCount, ma_format_f32, channels);
}

static void
miniaudio_init(struct ring_buffer *audio_buffer)
{
	ma_device_config config  = ma_device_config_init(ma_device_type_playback);
	size_t buffersize = 512; /* In samples */
	size_t size;
	void *base;
	/* Allocate ring buffer */
	size = 4 * channels * buffersize;
	base = xvmalloc(NULL, 0, size * sizeof(float));
	*audio_buffer = ring_buffer_init(base, size, sizeof(float));
	ring_buffer = audio_buffer;

        config.playback.format   = ma_format_f32;
        config.playback.channels = channels;
        config.sampleRate        = samplerate;
        config.dataCallback      = miniaudio_callback;
        config.pUserData         = NULL;

        if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
		die("Failed to initialize miniaudio\n");
        }

        ma_device_start(&device);
}

static void
miniaudio_fini(void)
{
	ma_device_uninit(&device);
}

static void
miniaudio_step(void)
{
	
}

struct audio_io *miniaudio_io = &(struct audio_io) {
	.init = miniaudio_init,
	.fini = miniaudio_fini,
	.step = miniaudio_step,
};
