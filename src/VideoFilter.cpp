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
#include <QList>

#define TEXFORMAT GS_BGRA

struct video_rectangle
{
	const char* name;
	uint32_t x = 0;
	uint32_t y = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t color = 0;
	uint32_t tolerance = 0;
	uint32_t maxR = 0;
	uint32_t maxG = 0;
	uint32_t maxB = 0;
	uint32_t maxA = 0;
	uint32_t minR = 0;
	uint32_t minG = 0;
	uint32_t minB = 0;
	uint32_t minA = 0;
	bool invert = false;
	bool output = false;
	uint64_t outputRate;
	mutable uint64_t nextVideoUpdate;
	mutable bool state = false;
};

struct video_group
{
	const char* name;
	bool individual = false;
	mutable bool state = false;
	QList<video_rectangle>* rectangles;
};

struct ostws_filter
{
	obs_source_t* context;
	pthread_mutex_t ostws_sender_video_mutex;
	struct obs_video_info ovi;
	struct obs_audio_info oai;

	uint32_t known_width;
	uint32_t known_height;

	gs_texrender_t* texrender;
	gs_stagesurf_t* stagesurface;
	uint8_t* video_data;
	uint32_t video_linesize;

	video_t* video_output;
	bool is_audioonly;

	uint64_t nextVideoUpdate;

	QList<video_group>* groups;
};

const char* ostws_filter_getname(void* data)
{
	UNUSED_PARAMETER(data);
	return obs_module_text("OstWS (Video)");
}

void ostws_filter_update(void* data, obs_data_t* settings);

void ostws_filter_getdefaults(obs_data_t* defaults)
{
}

void ostws_filter_raw_video(void* data, video_data* frame)
{
	auto s = (struct ostws_filter*)data;

	if (!frame || !frame->data[0] || s->groups->count() == 0)
		return;

	uint32_t linesize = frame->linesize[0];
	uint32_t linesizeForLong = frame->linesize[0] / 4;

	pthread_mutex_lock(&s->ostws_sender_video_mutex);

	uint8_t* frameData = frame->data[0];
	uint32_t* frameLongData = (uint32_t*)(frameData);
	for (int groupIndex = 0; groupIndex < s->groups->count(); groupIndex++)
	{
		const video_group* group = &s->groups->at(groupIndex);
		bool state = true;

		for (int rectangleIndex = 0; rectangleIndex < group->rectangles->count(); rectangleIndex++)
		{
			bool individualState = true;
			const video_rectangle* rectangle = &group->rectangles->at(rectangleIndex);
			for (size_t y = rectangle->y; y < rectangle->y + rectangle->height; y++)
			{
				if (y >= s->known_height || !individualState)
				{
					break;
				}
				const size_t y_pos = y * linesizeForLong;
				for (size_t x = rectangle->x; x < rectangle->x + rectangle->width; x++)
				{
					if (x >= s->known_width)
					{
						break;
					}
					// If rectangle is NOT the color mark it and abort
					// Or if invert is ON and pixel is the color its not suposed to be, state and break
					const uint32_t color = *(frameLongData + y_pos + x);
					const auto colorMatch = (
						((color & 0xFF) <= rectangle->maxB) &&
						((color & 0xFF) >= rectangle->minB) &&
						(((color >> 8) & 0xFF) <= rectangle->maxG) &&
						(((color >> 8) & 0xFF) >= rectangle->minG) &&
						(((color >> 16) & 0xFF) <= rectangle->maxR) &&
						(((color >> 16) & 0xFF) >= rectangle->minR) &&
						(((color >> 24) & 0xFF) <= rectangle->maxA) &&
						(((color >> 24) & 0xFF) >= rectangle->minA)
					);

					if (!rectangle->invert && !colorMatch || rectangle->invert && colorMatch)
					{
						individualState = false;
						state = false;
						break;
					}
				}
			}

			if (group->individual && individualState != rectangle->state)
			{
				OBSDataAutoRelease obs_data = obs_data_create();
				obs_data_set_string(obs_data, "update-type", "RectangleUpdate");
				obs_data_set_string(obs_data, "name", rectangle->name);
				obs_data_set_string(obs_data, "group", group->name);
				obs_data_set_bool(obs_data, "state", individualState);
				obs_data_set_bool(obs_data, "lastState", rectangle->state);
				obs_data_set_int(obs_data, "timestamp", frame->timestamp);
				WSServer::Instance->broadcast_thread_safe({
					obs_data_get_json(obs_data),
					video
				});
				rectangle->state = individualState;
			}
		}

		if (state != group->state)
		{
			OBSDataAutoRelease obs_data = obs_data_create();
			obs_data_set_string(obs_data, "update-type", "GroupUpdate");
			obs_data_set_string(obs_data, "name", group->name);
			obs_data_set_bool(obs_data, "state", state);
			obs_data_set_bool(obs_data, "lastState", group->state);
			obs_data_set_int(obs_data, "timestamp", frame->timestamp);
			WSServer::Instance->broadcast_thread_safe({
				obs_data_get_json(obs_data),
				video
			});
			group->state = state;
		}
	}

	for (int groupIndex = 0; groupIndex < s->groups->count(); groupIndex++)
	{
		const video_group* group = &s->groups->at(groupIndex);

		for (int rectangleIndex = 0; rectangleIndex < group->rectangles->count(); rectangleIndex++)
		{
			const video_rectangle* rectangle = &group->rectangles->at(rectangleIndex);
			if (rectangle->output && rectangle->nextVideoUpdate < frame->timestamp)
			{
				rectangle->nextVideoUpdate = frame->timestamp + (rectangle->outputRate * 1000000);

				OBSDataAutoRelease obs_data = obs_data_create();
				obs_data_set_string(obs_data, "update-type", "VideoUpdate");
				obs_data_set_string(obs_data, "name", rectangle->name);
				obs_data_set_string(obs_data, "group", group->name);
				OBSDataArrayAutoRelease array = obs_data_array_create();

				for (size_t y = rectangle->y; y < rectangle->y + rectangle->height; y++)
				{
					if (y >= s->known_height)
					{
						break;
					}
					const size_t y_pos = y * linesizeForLong;
					for (size_t x = rectangle->x; x < rectangle->x + rectangle->width; x++)
					{
						if (x >= s->known_width)
						{
							break;
						}

						OBSDataAutoRelease pixelData = obs_data_create();
						obs_data_set_int(pixelData, "x", x);
						obs_data_set_int(pixelData, "y", y);
						obs_data_set_int(pixelData, "color", *(frameLongData + y_pos + x));
						obs_data_array_push_back(array, pixelData);
					}
				}

				obs_data_set_array(obs_data, "pixels", array);
				obs_data_set_int(obs_data, "timestamp", frame->timestamp);
				obs_data_set_int(obs_data, "maxB", rectangle->maxB);
				obs_data_set_int(obs_data, "minB", rectangle->minB);
				obs_data_set_int(obs_data, "maxG", rectangle->maxG);
				obs_data_set_int(obs_data, "minG", rectangle->minG);
				obs_data_set_int(obs_data, "maxR", rectangle->maxR);
				obs_data_set_int(obs_data, "minR", rectangle->minR);
				obs_data_set_int(obs_data, "maxA", rectangle->maxA);
				obs_data_set_int(obs_data, "minA", rectangle->minA);
				WSServer::Instance->broadcast_thread_safe(obs_data_get_json(obs_data));
			}
		}
	}
	pthread_mutex_unlock(&s->ostws_sender_video_mutex);
}

void ostws_filter_offscreen_render(void* data, uint32_t cx, uint32_t cy)
{
	auto s = (struct ostws_filter*)data;

	obs_source_t* target = obs_filter_get_parent(s->context);
	if (!target)
	{
		return;
	}

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);

	gs_texrender_reset(s->texrender);

	if (gs_texrender_begin(s->texrender, width, height))
	{
		struct vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_video_render(target);

		gs_blend_state_pop();
		gs_texrender_end(s->texrender);

		if (s->known_width != width || s->known_height != height)
		{
			gs_stagesurface_destroy(s->stagesurface);
			s->stagesurface = gs_stagesurface_create(width, height, TEXFORMAT);

			video_output_info vi = {0};
			vi.format = VIDEO_FORMAT_BGRA;
			vi.width = width;
			vi.height = height;
			vi.fps_den = s->ovi.fps_den;
			vi.fps_num = s->ovi.fps_num;
			vi.cache_size = 16;
			vi.colorspace = VIDEO_CS_DEFAULT;
			vi.range = VIDEO_RANGE_DEFAULT;
			vi.name = obs_source_get_name(s->context);

			video_output_close(s->video_output);
			video_output_open(&s->video_output, &vi);
			video_output_connect(s->video_output,
			                     nullptr, ostws_filter_raw_video, s);

			s->known_width = width;
			s->known_height = height;
		}

		struct video_frame output_frame;
		if (video_output_lock_frame(s->video_output,
		                            &output_frame, 1, os_gettime_ns()))
		{
			if (s->video_data)
			{
				gs_stagesurface_unmap(s->stagesurface);
				s->video_data = nullptr;
			}

			gs_stage_texture(s->stagesurface,
			                 gs_texrender_get_texture(s->texrender));
			gs_stagesurface_map(s->stagesurface,
			                    &s->video_data, &s->video_linesize);

			uint32_t linesize = output_frame.linesize[0];
			for (uint32_t i = 0; i < s->known_height; ++i)
			{
				uint32_t dst_offset = linesize * i;
				uint32_t src_offset = s->video_linesize * i;
				memcpy(output_frame.data[0] + dst_offset,
				       s->video_data + src_offset,
				       linesize);
			}

			video_output_unlock_frame(s->video_output);
		}
	}
}

void ostws_filter_update(void* data, obs_data_t* settings)
{
	UNUSED_PARAMETER(settings);
	auto s = (struct ostws_filter*)data;

	obs_remove_main_render_callback(ostws_filter_offscreen_render, s);

	blog(LOG_INFO, "ostws_filter_update called");

	OBSDataAutoRelease obs_data = obs_data_create();
	obs_data_set_string(obs_data, "update-type", "info");
	obs_data_set_string(obs_data, "message", "ostws_filter_update called");
	QString json = obs_data_get_json(obs_data);
	WSServer::Instance->broadcast_thread_safe(json);

	pthread_mutex_lock(&s->ostws_sender_video_mutex);
	s->groups->clear();

	OBSDataArrayAutoRelease groups = obs_data_get_array(settings, "groups");
	for (size_t i = 0; i < obs_data_array_count(groups); ++i)
	{
		OBSDataAutoRelease group = obs_data_array_item(groups, i);
		video_group video_group;
		video_group.name = obs_data_get_string(group, "name");
		video_group.individual = obs_data_get_bool(group, "individual");
		video_group.rectangles = new QList<video_rectangle>();

		OBSDataArrayAutoRelease rectangles = obs_data_get_array(group, "rectangles");
		for (size_t i = 0; i < obs_data_array_count(rectangles); ++i)
		{
			OBSDataAutoRelease rectangle = obs_data_array_item(rectangles, i);

			video_rectangle video_rectangle1;
			video_rectangle1.name = obs_data_get_string(rectangle, "name");
			video_rectangle1.x = obs_data_get_int(rectangle, "x");
			video_rectangle1.y = obs_data_get_int(rectangle, "y");
			video_rectangle1.width = obs_data_get_int(rectangle, "width");
			video_rectangle1.height = obs_data_get_int(rectangle, "height");
			video_rectangle1.color = obs_data_get_int(rectangle, "color");
			video_rectangle1.tolerance = obs_data_get_int(rectangle, "tolerance");
			video_rectangle1.invert = obs_data_get_bool(rectangle, "invert");
			video_rectangle1.output = obs_data_get_bool(rectangle, "output");
			video_rectangle1.outputRate = obs_data_get_int(rectangle, "outputRate");

			video_rectangle1.maxB = min((video_rectangle1.color & 0xFF) + video_rectangle1.tolerance, 255);
			video_rectangle1.maxG = min(((video_rectangle1.color >> 8) & 0xFF) + video_rectangle1.tolerance, 255);
			video_rectangle1.maxR = min(((video_rectangle1.color >> 16) & 0xFF) + video_rectangle1.tolerance, 255);
			video_rectangle1.maxA = min(((video_rectangle1.color >> 24) & 0xFF) + video_rectangle1.tolerance, 255);

			video_rectangle1.minB = max((video_rectangle1.color & 0xFF) - video_rectangle1.tolerance, 0);
			video_rectangle1.minG = max(((video_rectangle1.color >> 8) & 0xFF) - video_rectangle1.tolerance, 0);
			video_rectangle1.minR = max(((video_rectangle1.color >> 16) & 0xFF) - video_rectangle1.tolerance, 0);
			video_rectangle1.minA = max(((video_rectangle1.color >> 24) & 0xFF) - video_rectangle1.tolerance, 0);
			if (video_rectangle1.minB > 255)
			{
				video_rectangle1.minB = 0;
			}
			if (video_rectangle1.minG > 255)
			{
				video_rectangle1.minG = 0;
			}
			if (video_rectangle1.minR > 255)
			{
				video_rectangle1.minR = 0;
			}
			if (video_rectangle1.minA > 255)
			{
				video_rectangle1.minA = 0;
			}
			video_group.rectangles->append(video_rectangle1);
		}

		s->groups->append(video_group);
	}
	pthread_mutex_unlock(&s->ostws_sender_video_mutex);

	if (!s->is_audioonly)
	{
		obs_add_main_render_callback(ostws_filter_offscreen_render, s);
	}
}

void* ostws_filter_create(obs_data_t* settings, obs_source_t* source)
{
	auto s = (struct ostws_filter*)bzalloc(sizeof(struct ostws_filter));
	s->groups = new QList<video_group>();
	s->is_audioonly = false;
	s->context = source;
	s->texrender = gs_texrender_create(TEXFORMAT, GS_ZS_NONE);
	s->video_data = nullptr;
	pthread_mutex_init(&s->ostws_sender_video_mutex, NULL);

	obs_get_video_info(&s->ovi);
	obs_get_audio_info(&s->oai);

	OBSDataAutoRelease obs_data = obs_data_create();
	obs_data_set_string(obs_data, "update-type", "info");
	obs_data_set_string(obs_data, "message", "ostws_filter_create called");
	QString json = obs_data_get_json(obs_data);
	WSServer::Instance->broadcast_thread_safe(json);

	ostws_filter_update(s, settings);
	return s;
}

void ostws_filter_destroy(void* data)
{
	auto s = (struct ostws_filter*)data;

	OBSDataAutoRelease obs_data = obs_data_create();
	obs_data_set_string(obs_data, "update-type", "info");
	obs_data_set_string(obs_data, "message", "ostws_filter_destroy called");
	QString json = obs_data_get_json(obs_data);
	WSServer::Instance->broadcast_thread_safe(json);

	obs_remove_main_render_callback(ostws_filter_offscreen_render, s);
	video_output_close(s->video_output);

	// pthread_mutex_lock(&s->ostws_sender_video_mutex);
	// pthread_mutex_lock(&s->ostws_sender_audio_mutex);

	// ndiLib->NDIlib_send_destroy(s->ostws_sender);

	// pthread_mutex_unlock(&s->ostws_sender_audio_mutex);
	// pthread_mutex_unlock(&s->ostws_sender_video_mutex);

	gs_stagesurface_unmap(s->stagesurface);
	gs_stagesurface_destroy(s->stagesurface);
	gs_texrender_destroy(s->texrender);

	bfree(s);
}


void ostws_filter_tick(void* data, float seconds)
{
	auto s = (struct ostws_filter*)data;
	obs_get_video_info(&s->ovi);
}

void ostws_filter_videorender(void* data, gs_effect_t* effect)
{
	UNUSED_PARAMETER(effect);
	auto s = (struct ostws_filter*)data;
	obs_source_skip_video_filter(s->context);
}

struct obs_source_info create_ostws_filter_info()
{
	struct obs_source_info ostws_filter_info = {};
	ostws_filter_info.id = "ostws_videofilter";
	ostws_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	ostws_filter_info.output_flags = OBS_SOURCE_VIDEO;

	ostws_filter_info.get_name = ostws_filter_getname;
	ostws_filter_info.get_defaults = ostws_filter_getdefaults;

	ostws_filter_info.create = ostws_filter_create;
	ostws_filter_info.destroy = ostws_filter_destroy;
	ostws_filter_info.update = ostws_filter_update;

	ostws_filter_info.video_tick = ostws_filter_tick;
	ostws_filter_info.video_render = ostws_filter_videorender;

	// Audio is available only with async sources
	//ostws_filter_info.filter_audio = ostws_filter_asyncaudio;

	return ostws_filter_info;
}
