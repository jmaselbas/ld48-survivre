#include <string.h>
#include <stdio.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>

/* https://docs.huihoo.com/maemo/5.0/pulseaudio/paplay_8c-example.html */
/* https://gavv.github.io/articles/pulseaudio-under-the-hood/#about-pulseaudio */

#undef MIN
#undef MAX
#include "core.h"
#include "audio.h"

static pa_context *context = NULL;
static pa_stream *stream = NULL;
static pa_volume_t volume = PA_VOLUME_NORM;

static pa_sample_spec sample_spec = {
	.format = PA_SAMPLE_S16LE,
	.rate = 48000,
	.channels = 2,
};

static pthread_mutex_t mutex;
static pthread_cond_t cond;
static volatile int quit;
struct sample {
	float l;
	float r;
};

static void stream_write_callback(pa_stream *s, size_t nframes, void *userdata)
{
	struct audio_state *audio = userdata;
	size_t k = pa_frame_size(&sample_spec);
	int16_t buf[sample_spec.channels *  512];
	size_t count, i;
	struct sample *data;

	while (nframes > 0) {
		count = ring_buffer_read_size(&audio->buffer);
		data = ring_buffer_read_addr(&audio->buffer);
		if (count > 0) {
			count = MIN(count, 512);
			count = MIN(count, nframes);
			for (i = 0; i < count; i++) {
				buf[i * 2 + 0] = INT16_MAX * data[i].l;
				buf[i * 2 + 1] = INT16_MAX * data[i].r;
			}
			pa_stream_write(s, buf, k * count, NULL, 0, PA_SEEK_RELATIVE);
			ring_buffer_read_done(&audio->buffer, count);
			nframes -= count;
		} else {
			/* no more audio to write to pulseaudio server */
			pthread_cond_wait(&cond, &mutex);
			if (quit)
				return;
		}
	}
}

/* This routine is called whenever the stream state changes */
static void stream_state_callback(pa_stream *s, void *userdata) {
	assert(s);
	(void) userdata; /* unused */

	switch (pa_stream_get_state(s)) {
	case PA_STREAM_CREATING:
	case PA_STREAM_TERMINATED:
		break;

	case PA_STREAM_READY:
		fprintf(stderr, "Stream successfully created\n");
		break;

	case PA_STREAM_FAILED:
	default:
		fprintf(stderr, "Stream errror: %s\n", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
	}
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata)
{
	char *dev = NULL; /* device name */
	pa_cvolume cv;
	pa_buffer_attr attr;

	assert(c);

	switch (pa_context_get_state(c)) {
	case PA_CONTEXT_CONNECTING:
	case PA_CONTEXT_AUTHORIZING:
	case PA_CONTEXT_SETTING_NAME:
		break;

	case PA_CONTEXT_READY:
		stream = pa_stream_new(c, "out_stream", &sample_spec, NULL);
		assert(stream);

		/* frament size = number of frame * frame_size */
		attr.fragsize = -1; /* (recording only) */
		attr.maxlength = -1;

		/* tlength: target length aka latency */
		/* latency * frame_per_sec * sizeof_frame */
		attr.tlength = 0.050 * 48000 * 2 * sizeof(int16_t);
		attr.prebuf = -1; /* pre-buffering */
		attr.minreq = -1;//512 * sizeof(int16_t);

		pa_stream_set_state_callback(stream, stream_state_callback, NULL);
		pa_stream_set_write_callback(stream, stream_write_callback, userdata);
		pa_stream_connect_playback(stream, dev, &attr, 0, pa_cvolume_set(&cv, sample_spec.channels, volume), NULL);

		break;
	case PA_CONTEXT_TERMINATED:
		fprintf(stderr, "PA_CONTEXT_TERMINATED\n");
		break;

	case PA_CONTEXT_FAILED:
	default:
		fprintf(stderr, "Connection failure: %s\n", pa_strerror(pa_context_errno(c)));
		break;
	}
}

static void *
pulse_main(void *arg)
{
	pa_mainloop *mainloop = arg;
	int ret;

	if (pa_mainloop_run(mainloop, &ret) < 0) {
		fprintf(stderr, "pa_mainloop_run() failed\n");
	}
	fprintf(stderr, "pulse exit\n");

	return NULL;
}

pthread_t pulse_thread;
pa_mainloop *mainloop;

static void
pulse_step(struct audio_state *audio)
{
	UNUSED(audio);
	/* signal audio thread */
	pthread_cond_signal(&cond);
}

static void
pulse_init(struct audio_state *audio)
{
	int ret;
	char *client_name = "drone";
	char *server_name = NULL;

	/* Set up a new main loop */
	mainloop = pa_mainloop_new();
	if (!mainloop) {
		fprintf(stderr, "pa_mainloop_new() failed\n");
		goto quit;
	}

	static pa_mainloop_api *mainloop_api = NULL;
	mainloop_api = pa_mainloop_get_api(mainloop);

	/* Create a new connection context */
	context = pa_context_new(mainloop_api, client_name);
	if (!context) {
		fprintf(stderr, "pa_context_new() failed\n");
		goto quit;
	}

	pa_context_set_state_callback(context, context_state_callback, audio);

	/* Connect the context */
	if (pa_context_connect(context, server_name, 0, NULL) < 0) {
		fprintf(stderr, "pa_context_connect() failed: %s\n", pa_strerror(pa_context_errno(context)));
		goto quit;
	}

	if (pthread_mutex_init(&mutex, NULL) != 0)
		die("pthread_mutex_init() error");

	if (pthread_cond_init(&cond, NULL) != 0)
		die("pthread_cond_init() error");

	/* Run the main loop */
	ret = pthread_create(&pulse_thread, NULL, pulse_main, mainloop);
	if (ret)
		die("pthread_create failed\n");
	quit = 0;
quit:
	return;
}

static void
pulse_fini(struct audio_state *audio)
{
	UNUSED(audio);
	quit = 1;
	if (mainloop) {
		pa_mainloop_quit(mainloop, 0);
		pthread_cond_signal(&cond);
	}
	if (stream) {
		pa_stream_disconnect(stream);
		pa_stream_unref(stream);
	}
	if (context) {
		pa_context_disconnect(context);
		pa_context_unref(context);
	}
	if (mainloop)
		pa_mainloop_free(mainloop);
	if (pulse_thread)
		pthread_join(pulse_thread, NULL);
}

struct audio_io *pulse_io = &(struct audio_io) {
	.init = pulse_init,
	.fini = pulse_fini,
	.step = pulse_step,
};
