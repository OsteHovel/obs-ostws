#include <QString>

#include "Config.h"
#include "Utils.h"
#include "WSEvents.h"

#include "WSRequestHandler.h"

/**
 * Returns the latest version of the plugin and the API.
 *
 * @return {double} `version` OBSRemote compatible API version. Fixed to 1.1 for retrocompatibility.
 * @return {String} `obs-ostws-version` obs-ostws plugin version.
 * @return {String} `obs-studio-version` OBS Studio program version.
 * @return {String} `available-requests` List of available request types, formatted as a comma-separated list string (e.g. : "Method1,Method2,Method3").
 *
 * @api requests
 * @name GetVersion
 * @category general
 * @since 0.3
 */
void WSRequestHandler::HandleGetVersion(WSRequestHandler* req)
{
	QString obsVersion = Utils::OBSVersionString();

	QList<QString> names = req->messageMap.keys();
	names.sort(Qt::CaseInsensitive);

	// (Palakis) OBS' data arrays only support object arrays, so I improvised.
	QString requests;
	requests += names.takeFirst();
	for (QString reqName : names)
	{
		requests += ("," + reqName);
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "obs-ostws-version", OST_WEBSOCKET_VERSION);
	obs_data_set_string(data, "obs-studio-version", obsVersion.toUtf8());
	obs_data_set_string(data, "available-requests", requests.toUtf8());

	req->SendOKResponse(data);
}

/**
 * Tells the client if authentication is required. If so, returns authentication parameters `challenge`
 * and `salt` (see "Authentication" for more information).
 *
 * @return {boolean} `authRequired` Indicates whether authentication is required.
 * @return {String (optional)} `challenge`
 * @return {String (optional)} `salt`
 *
 * @api requests
 * @name GetAuthRequired
 * @category general
 * @since 0.3
 */
void WSRequestHandler::HandleGetAuthRequired(WSRequestHandler* req)
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_bool(data, "authRequired", false);
	req->SendOKResponse(data);
}

/**
 * Attempt to authenticate the client to the server.
 *
 * @param {String} `auth` Response to the auth challenge (see "Authentication" for more information).
 *
 * @api requests
 * @name Authenticate
 * @category general
 * @since 0.3
 */
void WSRequestHandler::HandleAuthenticate(WSRequestHandler* req)
{
	req->SendOKResponse();
}

/**
 * Enable/disable sending of the Heartbeat event
 *
 * @param {boolean} `enable` Starts/Stops emitting heartbeat messages
 *
 * @api requests
 * @name SetHeartbeat
 * @category general
 * @since 4.3.0
 */
void WSRequestHandler::HandleSetHeartbeat(WSRequestHandler* req)
{
	if (!req->hasField("enable"))
	{
		req->SendErrorResponse("Heartbeat <enable> parameter missing");
		return;
	}

	WSEvents::Instance->HeartbeatIsActive =
		obs_data_get_bool(req->data, "enable");

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_bool(response, "enable",
	                  WSEvents::Instance->HeartbeatIsActive);
	req->SendOKResponse(response);
}

void WSRequestHandler::HandleSetVideo(WSRequestHandler* req)
{
	if (!req->hasField("enable"))
	{
		req->SendErrorResponse("Video <enable> parameter missing");
		return;
	}
	WSServer::client_config_map[req->_client].video_broadcast =
		obs_data_get_bool(req->data, "enable");

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_bool(response, "enable",
	                  WSServer::client_config_map[req->_client].video_broadcast);
	req->SendOKResponse(response);
}

void WSRequestHandler::HandleSetAudio(WSRequestHandler* req)
{
	if (!req->hasField("enable"))
	{
		req->SendErrorResponse("Audio <enable> parameter missing");
		return;
	}

	WSServer::client_config_map[req->_client].audio_broadcast =
		obs_data_get_bool(req->data, "enable");

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_bool(response, "enable",
	                  WSServer::client_config_map[req->_client].audio_broadcast);
	req->SendOKResponse(response);
}

/**
 * Send the provided text as embedded CEA-608 caption data
 *
 * @api requests
 * @name SendCaptions
 * @category streaming
 */
void WSRequestHandler::HandleSendCaptions(WSRequestHandler* req) {
	if (!req->hasField("text") || !req->hasField("displayDuration")) {
		return req->SendErrorResponse("missing request parameters");
	}

	OBSOutputAutoRelease output = obs_frontend_get_streaming_output();
	if (output) {
		const char* caption = obs_data_get_string(req->data, "text");
		double display_duration = obs_data_get_double(req->data, "displayDuration");
		obs_output_output_caption_text2(output, caption, display_duration);
	}

	return req->SendOKResponse();
}
