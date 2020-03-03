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


#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <string>

#define SECTION_NAME "OstWS"
#define PARAM_ENABLE "ServerEnabled"
#define PARAM_PORT "ServerPort"
#define PARAM_DEBUG "DebugEnabled"
#define PARAM_ALERT "AlertsEnabled"

#include "Config.h"
#include "Utils.h"

#define QT_TO_UTF8(str) str.toUtf8().constData()

Config* Config::_instance = new Config();

Config::Config() :
    ServerEnabled(true),
    ServerPort(4445),
    DebugEnabled(false),
    AlertsEnabled(false),
    SettingsLoaded(false)
{
    
}

Config::~Config() {
    
}

void Config::Load() {


}

void Config::Save() {
}

Config* Config::Current() {
    return _instance;
}
