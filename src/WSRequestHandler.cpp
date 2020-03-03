/**
 * obs-ostws
 * Copyright (C) 2016-2017	Stéphane Lepin <stephane.lepin@gmail.com>
 * Copyright (C) 2017	Mikhail Swift <https://github.com/mikhailswift>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>
 */

#include <obs-data.h>

#include "Config.h"
#include "Utils.h"

#include "WSRequestHandler.h"

QHash<QString, void(*)(WSRequestHandler*)> WSRequestHandler::messageMap{
	{"GetVersion", WSRequestHandler::HandleGetVersion},
	{"GetAuthRequired", WSRequestHandler::HandleGetAuthRequired},
	{"Authenticate", WSRequestHandler::HandleAuthenticate},

	{"SetHeartbeat", WSRequestHandler::HandleSetHeartbeat},

	{"SetVideo", WSRequestHandler::HandleSetVideo},
	{"SetAudio", WSRequestHandler::HandleSetAudio},

	{"GetSourceFilters", WSRequestHandler::HandleGetSourceFilters},
	{"AddFilterToSource", WSRequestHandler::HandleAddFilterToSource},
	{"RemoveFilterFromSource", WSRequestHandler::HandleRemoveFilterFromSource},
	{"ReorderSourceFilter", WSRequestHandler::HandleReorderSourceFilter},
	{"MoveSourceFilter", WSRequestHandler::HandleMoveSourceFilter},
	{"SetSourceFilterSettings", WSRequestHandler::HandleSetSourceFilterSettings},
	{"SetSourceFilterVisibility", WSRequestHandler::HandleSetSourceFilterVisibility},
	{"TriggerHotkeyOnSource", WSRequestHandler::HandleTriggerHotkeyOnSource},
	{"SendCaptions", WSRequestHandler::HandleSendCaptions},
};

WSRequestHandler::WSRequestHandler(QWebSocket* client) :
	_messageId(0),
	_requestType(""),
	data(nullptr),
	_client(client)
{
}

void WSRequestHandler::processIncomingMessage(QString textMessage)
{
	QByteArray msgData = textMessage.toUtf8();
	const char* msg = msgData.constData();

	data = obs_data_create_from_json(msg);
	if (!data)
	{
		if (!msg)
			msg = "<null pointer>";

		blog(LOG_ERROR, "invalid JSON payload received for '%s'", msg);
		SendErrorResponse("invalid JSON payload");
		return;
	}

	if (Config::Current()->DebugEnabled)
	{
		blog(LOG_DEBUG, "Request >> '%s'", msg);
	}

	if (!hasField("request-type")
		|| !hasField("message-id"))
	{
		SendErrorResponse("missing request parameters");
		return;
	}

	_requestType = obs_data_get_string(data, "request-type");
	_messageId = obs_data_get_string(data, "message-id");

	void (*handlerFunc)(WSRequestHandler*) = (messageMap[_requestType]);

	if (handlerFunc != nullptr)
		handlerFunc(this);
	else
		SendErrorResponse("invalid request type");
}

WSRequestHandler::~WSRequestHandler()
{
}

void WSRequestHandler::SendOKResponse(obs_data_t* additionalFields)
{
	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "status", "ok");
	obs_data_set_string(response, "message-id", _messageId);
	obs_data_set_string(response, "request-type", _requestType);

	if (additionalFields)
		obs_data_apply(response, additionalFields);

	SendResponse(response);
}

void WSRequestHandler::SendErrorResponse(const char* errorMessage)
{
	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "status", "error");
	obs_data_set_string(response, "error", errorMessage);
	obs_data_set_string(response, "message-id", _messageId);

	SendResponse(response);
}

void WSRequestHandler::SendErrorResponse(obs_data_t* additionalFields)
{
	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "status", "error");
	obs_data_set_string(response, "message-id", _messageId);

	if (additionalFields)
		obs_data_set_obj(response, "error", additionalFields);

	SendResponse(response);
}

void WSRequestHandler::SendResponse(obs_data_t* response)
{
	QString json = obs_data_get_json(response);
	_client->sendTextMessage(json);

	if (Config::Current()->DebugEnabled)
		blog(LOG_DEBUG, "Response << '%s'", json.toUtf8().constData());
}

bool WSRequestHandler::hasField(QString name)
{
	if (!data || name.isEmpty() || name.isNull())
		return false;

	return obs_data_has_user_value(data, name.toUtf8());
}
