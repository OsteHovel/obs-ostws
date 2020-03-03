/*
obs-ostws
Copyright (C) 2016-2017	Stéphane Lepin <stephane.lepin@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include <QTimer>

#include "obs-ostws.h"
#include "WSServer.h"
#include "WSEvents.h"
#include "Config.h"

void ___source_dummy_addref(obs_source_t*) {}
void ___sceneitem_dummy_addref(obs_sceneitem_t*) {}
void ___data_dummy_addref(obs_data_t*) {}
void ___data_array_dummy_addref(obs_data_array_t*) {}
void ___output_dummy_addref(obs_output_t*) {}

extern struct obs_source_info create_ostws_filter_info();
struct obs_source_info ostws_filter_info;

extern struct obs_source_info create_ostws_audiofilter_info();
struct obs_source_info ostws_audiofilter_info;

extern struct obs_source_info create_alpha_filter_info();
struct obs_source_info alpha_filter_info;

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("OsteHovel")
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ostws", "en-US")


bool obs_module_load(void) {
	blog(LOG_INFO, "Initializing");
    blog(LOG_INFO, "WebSocket version %s", OST_WEBSOCKET_VERSION);
    blog(LOG_INFO, "Qt version (compile-time): %s ; Qt version (run-time): %s",
        QT_VERSION_STR, qVersion());

    // Core setup
    Config* config = Config::Current();
    config->Load();

    WSServer::Instance = new WSServer();
    WSEvents::Instance = new WSEvents(WSServer::Instance);

    if (config->ServerEnabled)
        WSServer::Instance->Start(config->ServerPort);

	ostws_filter_info = create_ostws_filter_info();
	obs_register_source(&ostws_filter_info);

	ostws_audiofilter_info = create_ostws_audiofilter_info();
	obs_register_source(&ostws_audiofilter_info);

    
    blog(LOG_INFO, "Initialized");
    return true;
}

void obs_module_unload() {
    blog(LOG_INFO, "Unloaded");
}

