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

#ifndef WSSERVER_H
#define WSSERVER_H

#include <QObject>
#include <QList>
#include <QMutex>
#include <QQueue>

#include "WSRequestHandler.h"

struct ostws_audiofilter;

enum broadcast_type
{
	global,
	video,
	audio
};

struct client_config
{
	bool video_broadcast = false;
	bool audio_broadcast = false;
};

struct broadcast_message
{
	QString message;
	broadcast_type type = global;
};


QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

class WSServer : public QObject
{
Q_OBJECT
public:
	explicit WSServer(QObject* parent = Q_NULLPTR);
	virtual ~WSServer();
	void Start(quint16 port);
	void Stop();
	void broadcast(QString message);
	void broadcast(broadcast_message message);
	void broadcast_thread_safe(QString message);
	void broadcast_thread_safe(broadcast_message message);
	void add_audio_filter(ostws_audiofilter* audio_filter);
	void remove_audio_filter(ostws_audiofilter* audio_filter);
	static QHash<QWebSocket*, client_config> client_config_map;
	static WSServer* Instance;

private slots:
	void onCycle();
	void onNewConnection();
	void onTextMessageReceived(QString message);
	void onSocketDisconnected();
	void onAudioBroadcastCycle();

private:
	QWebSocketServer* _wsServer;
	QList<QWebSocket*> _clients;
	QMutex _clMutex;
	QQueue<broadcast_message> _broadcastQueue;
	QMutex _broadcastMutex;
	QList<ostws_audiofilter*> _audioFilters;
	QMutex _audioFilterMutex;
};

#endif // WSSERVER_H
