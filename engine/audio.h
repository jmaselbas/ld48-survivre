#ifndef AUDIO_H
#define AUDIO_H

#include "wav.h"
#include "sampler.h"

struct sample {
	float l;
	float r;
};

struct audio {
	size_t size; /* in sample */
	struct sample *buffer;
};

#endif
