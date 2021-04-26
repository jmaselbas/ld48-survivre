#ifndef SAMPLER_H
#define SAMPLER_H

enum sampler_state{
	STOP,
	PLAY,
};

struct sampler {
	struct wav *wav;
	enum sampler_state state;
	size_t pb_start;
	size_t pb_end;
	size_t pb_head; /* playhead position */

	size_t loop_start;
	size_t loop_end;
	int    loop_on;

	/* Trig */
	unsigned char trig_on; /* input trig signal */

	/* Amp */
	float vol;
};

void sampler_init(struct sampler *sampler, struct wav *wav);

float step_sampler(struct sampler *sampler);

#endif
