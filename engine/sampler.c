#include "engine.h"

void
sampler_init(struct sampler *sampler, struct wav *wav)
{
	/* Playback */
	sampler->wav      = wav;
	sampler->state    = STOP;
	sampler->pb_start = 0;
	sampler->pb_end   = wav->extras.nb_frames;
	sampler->pb_head  = 0;
	/* Loop */
	sampler->loop_on  = 0;
	sampler->loop_start = 0;
	sampler->loop_end  = sampler->pb_end;

	sampler->trig_on = 0;
	sampler->vol = 1;
}

float step_sampler(struct sampler *sampler)
{
	int16_t *samples = (int16_t *) sampler->wav->audio_data;
	float vol = sampler->vol;
	int trig_on = sampler->trig_on;
	size_t offset = sampler->pb_head;
	int pb_fini = (sampler->pb_head >= sampler->wav->extras.nb_frames);
	float x = 0;

	if (sampler->loop_start >= sampler->pb_end)
		sampler->loop_on = 0;

	switch(sampler->state){
	case STOP:
		if(trig_on) {
			sampler->state = PLAY;
			sampler->trig_on = 0;
			sampler->pb_head++;
			offset = sampler->pb_head;
			x = vol * (float) (samples[offset] / (float) INT16_MAX);
			return x;
		} else {
			return 0;
		}
		break;
	case PLAY:
		if(pb_fini) {
			if (sampler->loop_on) {
				sampler->pb_head = sampler->loop_start;
			} else {
				sampler->state = STOP;
				sampler->pb_head = sampler->pb_start;
			}
		}

		if (sampler->state == STOP ) {
			return 0;
		} else {
			sampler->pb_head ++;
			offset = sampler->pb_head;
			x = vol * (float) (samples[offset] / (float) INT16_MAX);
			return x;
		}

		break;
	}
	return 0;
}
