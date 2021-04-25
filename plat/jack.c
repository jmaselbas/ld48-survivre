#include <string.h>
#include <jack/jack.h>
#include "core.h"
#include "audio.h"

jack_client_t *jack;
jack_port_t *outL_port;
jack_port_t *outR_port;

static int  jack_proc(jack_nframes_t nframes, void *arg);
static void jack_init(struct ring_buffer *ring_buffer);
static void jack_fini(void);

static int
jack_proc(jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t *outL, *outR;
	struct ring_buffer *ring_buffer = arg;
	size_t count = ring_buffer_read_size(ring_buffer);

	outL = jack_port_get_buffer(outL_port, nframes);
	outR = jack_port_get_buffer(outR_port, nframes);

	memset(outL, 0, nframes * sizeof(float));
	memset(outR, 0, nframes * sizeof(float));

	if (count > nframes)
		count = nframes;
	if (count > 0) {
		float *audio = ring_buffer_read_addr(ring_buffer);
		memcpy(outL, audio, count * sizeof(float));
		memcpy(outR, audio, count * sizeof(float));
		ring_buffer_read_done(ring_buffer, count);
	}

	return 0;
}

static void
jack_fini(void)
{
	jack_deactivate(jack);
	jack_client_close(jack);
}

static void
jack_init(struct ring_buffer *ring_buffer)
{
	const size_t nb_channel = 1;
	const char **ports;
	size_t buffersize; /* In samples */
	size_t samplerate;
	size_t buffer_count;
	size_t audio_size; /* In samples */
	void *audio_base;
	double min_fps;
	int ret;

	/* TODO: replace JackNullOption with JackNoStartServer */
	jack = jack_client_open("drone", JackNullOption, NULL);
	if (!jack)
		die("jack client open failed\n");

	/* Audio buffer size in samples */
	buffersize = jack_get_buffer_size(jack);
	samplerate = jack_get_sample_rate(jack);
	min_fps = 30.0;

	/* Ring size in nb of audio buffers */
	#if 0
	buffer_count = 0.5 + 2 * (1.0 / min_fps) * (samplerate / (double)buffersize);
	#else
	buffer_count = 4;
	#endif
	audio_size = buffer_count * buffersize * nb_channel;
	audio_base = xvmalloc(NULL, 0, audio_size * sizeof(float));
	*ring_buffer = ring_buffer_init(audio_base, audio_size, sizeof(float));

	/* /!\ set callback AFTER initializing the ringbuffer */
	ret = jack_set_process_callback(jack, jack_proc, ring_buffer);
	if (ret)
		die("jack set process callback failed \n");

	outL_port = jack_port_register(jack, "out_L",
				   JACK_DEFAULT_AUDIO_TYPE,
				   JackPortIsOutput, 0);
	outR_port = jack_port_register(jack, "out_R",
				   JACK_DEFAULT_AUDIO_TYPE,
				   JackPortIsOutput, 0);
	if ((outL_port == NULL) || (outR_port == NULL))
		die("failed to create jack out ports\n");

	ret = jack_activate(jack);
	if (ret)
		die("failed to activate jack client\n");

	ports = jack_get_ports(jack, NULL, NULL,
			       JackPortIsPhysical|JackPortIsInput);
	if (ports == NULL)
		die("no physical playback ports\n");

	if (jack_connect(jack, jack_port_name (outL_port), ports[0]))
		die("cannot connect output ports\n");
	if (jack_connect(jack, jack_port_name (outR_port), ports[1]))
		die("cannot connect output ports\n");

	jack_free(ports);
}

static void
jack_step(void)
{
}

struct audio_io *jack_io = &(struct audio_io) {
	.init = jack_init,
	.fini = jack_fini,
	.step = jack_step,
};
