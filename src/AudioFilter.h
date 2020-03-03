#ifndef AUDIOFILTER_H
#define AUDIOFILTER_H
#include <obs.h>

#define AUDIO_MIN -10000000

struct ostws_audiofilter
{
	obs_source_t* context;
	struct obs_audio_info oai;

	bool is_audioonly;

	const char* source_name;
	float magnitude;
	float peak;
	float mul;
	int nr_channels;
};

#endif // AUDIOFILTER_H
