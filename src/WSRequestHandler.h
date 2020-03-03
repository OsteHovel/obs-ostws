/*
obs-ostws
Copyright (C) 2016-2017	Stéphane Lepin <stephane.lepin@gmail.com>
Copyright (C) 2017	Mikhail Swift <https://github.com/mikhailswift>

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

#ifndef WSREQUESTHANDLER_H
#define WSREQUESTHANDLER_H

#include <QHash>
#include <QSet>
#include <QWebSocket>
#include <QWebSocketServer>

#include <obs.hpp>
#include <obs-frontend-api.h>

#include "obs-ostws.h"

class WSRequestHandler : public QObject {
  Q_OBJECT

  public:
    explicit WSRequestHandler(QWebSocket* client);
    ~WSRequestHandler();
    void processIncomingMessage(QString textMessage);
    bool hasField(QString name);

  private:
    QWebSocket* _client;
    const char* _messageId;
    const char* _requestType;
    OBSDataAutoRelease data;

    void SendOKResponse(obs_data_t* additionalFields = NULL);
    void SendErrorResponse(const char* errorMessage);
    void SendErrorResponse(obs_data_t* additionalFields = NULL);
    void SendResponse(obs_data_t* response);

    static QHash<QString, void(*)(WSRequestHandler*)> messageMap;

    static void HandleGetVersion(WSRequestHandler* req);
    static void HandleGetAuthRequired(WSRequestHandler* req);
    static void HandleAuthenticate(WSRequestHandler* req);

    static void HandleSetHeartbeat(WSRequestHandler* req);

    static void HandleSetVideo(WSRequestHandler* req);
    static void HandleSetAudio(WSRequestHandler* req);

	static void HandleGetSourceFilters(WSRequestHandler* req);
	static void HandleAddFilterToSource(WSRequestHandler* req);
	static void HandleRemoveFilterFromSource(WSRequestHandler* req);
	static void HandleReorderSourceFilter(WSRequestHandler* req);
	static void HandleMoveSourceFilter(WSRequestHandler* req);
	static void HandleSetSourceFilterSettings(WSRequestHandler* req);
	static void HandleSetSourceFilterVisibility(WSRequestHandler* req);
	static void HandleTriggerHotkeyOnSource(WSRequestHandler* req);
	static void HandleSendCaptions(WSRequestHandler * req);
};

#endif // WSPROTOCOL_H
