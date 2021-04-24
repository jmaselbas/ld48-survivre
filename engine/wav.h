#ifndef WAV_H
#define WAV_H

/* All sizes in Byte */
struct header {
	unsigned char riff_str[4];
	uint32_t      filesize   ;
	unsigned char wave_str[4];
	unsigned char frmt_str[4];
	uint32_t      frmtsize   ; /* length of the format data */
	uint16_t      encoding   ; /* 1-PCM, 3- IEEE float, 6-8bit A law, 7-8bit mu law */
	uint16_t      channels   ; /* nb channels */
	uint32_t      samplerate ;
	uint32_t      byterate   ;
	uint16_t      blockalign ; /* NumChannels * BitsPerSample/8 */
	uint16_t      sampledpth ; /* Bits per sample */
	unsigned char data_str[4]; /* DATA string or FLLR string */
	uint32_t      datasize   ; /* NumSamples * NumChannels * BitsPerSample/8 */
};

struct extras {
	size_t samplesize;
	size_t nb_frames;
	size_t nb_samples;
	size_t frame_size;
};

struct wav{
	struct header header;
	struct extras extras;
	void *audio_data;
};

#endif
