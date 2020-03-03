/*
obs-ostws
Copyright (C) 2016-2018 Stéphane Lepin <steph  name of author

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; If not, see <https://www.gnu.org/licenses/>
*/

#ifdef _WIN32
#include <Windows.h>
#endif

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include <util/threading.h>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include <media-io/audio-resampler.h>
#include "WSServer.h"
#include "AudioFilter.h"

#define CLAMP(x, min, max) ((x) < min ? min : ((x) > max ? max : (x)))


static inline float mul_to_db(const float mul)
{
	return (mul == 0.0f) ? -INFINITY : (20.0f * log10f(mul));
}

static inline float db_to_mul(const float db)
{
	return isfinite((double)db) ? powf(10.0f, db / 20.0f) : 0.0f;
}

const char* ostws_audiofilter_getname(void* data)
{
	UNUSED_PARAMETER(data);
	return obs_module_text("OstWS (Audio)");
}

void ostws_audiofilter_update(void* data, obs_data_t* settings);

void ostws_audiofilter_getdefaults(obs_data_t* defaults)
{
}

void* ostws_filter_create_audioonly(obs_data_t* settings, obs_source_t* source)
{
	auto s = (struct ostws_audiofilter*)bzalloc(sizeof(struct ostws_audiofilter));
	s->is_audioonly = true;
	s->context = source;

	obs_get_audio_info(&s->oai);

	ostws_audiofilter_update(s, settings);

	WSServer::Instance->add_audio_filter(s);
	return s;
}

void ostws_filter_destroy_audioonly(void* data)
{
	auto s = (struct ostws_audiofilter*)data;
	WSServer::Instance->remove_audio_filter(s);
	bfree(s);
}

float process_magnitude(const struct obs_audio_data* data, int nr_channels)
{
	size_t nr_samples = data->frames;

	float magnitude = AUDIO_MIN;

	int channel_nr = 0;
	for (int plane_nr = 0; channel_nr < nr_channels; plane_nr++)
	{
		float* samples = (float *)data->data[plane_nr];
		if (!samples)
		{
			continue;
		}

		float sum = 0.0;
		for (size_t i = 0; i < nr_samples; i++)
		{
			float sample = samples[i];
			sum += sample * sample;
		}
		float yx = sqrtf(sum / nr_samples);
		magnitude = fmax(magnitude, yx);

		channel_nr++;
	}

	return magnitude;
}

static int get_nr_channels_from_audio_data(const struct obs_audio_data* data)
{
	int nr_channels = 0;
	for (int i = 0; i < MAX_AV_PLANES; i++)
	{
		if (data->data[i])
			nr_channels++;
	}
	return CLAMP(nr_channels, 0, MAX_AUDIO_CHANNELS);
}

struct obs_audio_data* ostws_filter_asyncaudio(void* data, struct obs_audio_data* audio_data)
{
	auto s = (struct ostws_audiofilter*)data;

	obs_get_audio_info(&s->oai);

	obs_source* parentSource = obs_filter_get_parent(s->context);
	int nr_channels = get_nr_channels_from_audio_data(audio_data);
	float mul = obs_source_muted(parentSource) ? 0.0f : obs_source_get_volume(parentSource);
	float magnitude = mul_to_db(process_magnitude(audio_data, nr_channels) * mul);

	s->source_name = obs_source_get_name(parentSource);
	s->magnitude = fmax(s->magnitude, magnitude);
	s->mul = mul;
	s->nr_channels = nr_channels;

	return audio_data;
}

void ostws_audiofilter_update(void* data, obs_data_t* settings)
{
	UNUSED_PARAMETER(settings);
	auto s = (struct ostws_audiofilter*)data;
}

struct obs_source_info create_ostws_audiofilter_info()
{
	struct obs_source_info ostws_filter_info = {};
	ostws_filter_info.id = "ostws_audiofilter";
	ostws_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	ostws_filter_info.output_flags = OBS_SOURCE_AUDIO;

	ostws_filter_info.get_name = ostws_audiofilter_getname;
	ostws_filter_info.get_defaults = ostws_audiofilter_getdefaults;

	ostws_filter_info.create = ostws_filter_create_audioonly;
	ostws_filter_info.destroy = ostws_filter_destroy_audioonly;
	ostws_filter_info.update = ostws_audiofilter_update;

	ostws_filter_info.filter_audio = ostws_filter_asyncaudio;

	return ostws_filter_info;
}
