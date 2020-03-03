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

#include <QtWebSockets/QWebSocket>
#include <QtCore/QThread>
#include <QtCore/QByteArray>
#include <QMainWindow>
#include <QMessageBox>
#include <QTimer>
#include <obs-frontend-api.h>

#include "WSServer.h"
#include "obs-ostws.h"
#include "Config.h"
#include "Utils.h"
#include "AudioFilter.h"

QT_USE_NAMESPACE
WSServer* WSServer::Instance = nullptr;
QHash<QWebSocket*, client_config> WSServer::client_config_map{};

WSServer::WSServer(QObject* parent)
	: QObject(parent),
	  _wsServer(Q_NULLPTR),
	  _clients(),
	  _clMutex(QMutex::Recursive)
{
	_wsServer = new QWebSocketServer(
		QStringLiteral("obs-ostws"),
		QWebSocketServer::NonSecureMode);
}

WSServer::~WSServer()
{
	Stop();
}

void WSServer::Start(quint16 port)
{
	if (port == _wsServer->serverPort())
		return;

	if (_wsServer->isListening())
		Stop();

	bool serverStarted = _wsServer->listen(QHostAddress::Any, port);
	if (serverStarted)
	{
		blog(LOG_INFO, "server started successfully on TCP port %d", port);

		connect(_wsServer, SIGNAL(newConnection()),
		        this, SLOT(onNewConnection()));

		QTimer* cycleTimer = new QTimer();
		connect(cycleTimer, SIGNAL(timeout()), this, SLOT(onCycle()));
		cycleTimer->start(33);

		QTimer* audioBroadcastCycleTimer = new QTimer();
		connect(audioBroadcastCycleTimer, SIGNAL(timeout()), this, SLOT(onAudioBroadcastCycle()));
		audioBroadcastCycleTimer->start(500);
	}
	else
	{
		QString errorString = _wsServer->errorString();
		blog(LOG_ERROR,
			"error: failed to start server on TCP port %d: %s",
			port, errorString.toUtf8().constData());

		QMainWindow* mainWindow = (QMainWindow*)obs_frontend_get_main_window();

		obs_frontend_push_ui_translation(obs_module_get_string);
		QString title = tr("OBSWebsocket.Server.StartFailed.Title");
		QString msg = tr("OBSWebsocket.Server.StartFailed.Message").arg(port);
		obs_frontend_pop_ui_translation();

		QMessageBox::warning(mainWindow, title, msg);
	}
}

void WSServer::Stop()
{
	QMutexLocker locker(&_clMutex);
	for (QWebSocket* pClient : _clients)
	{
		pClient->close();
	}
	locker.unlock();

	_wsServer->close();

	blog(LOG_INFO, "server stopped successfully");
}

void WSServer::onCycle()
{
	QMutexLocker locker(&_broadcastMutex);
	while (!_broadcastQueue.isEmpty())
		broadcast(_broadcastQueue.dequeue());
}

void WSServer::broadcast(QString message)
{
	QMutexLocker locker(&_clMutex);
	for (QWebSocket* pClient : _clients)
	{
		pClient->sendTextMessage(message);
	}
}

void WSServer::broadcast(broadcast_message message)
{
	QMutexLocker locker(&_clMutex);
	for (QWebSocket* pClient : _clients)
	{
		if (message.type == global ||
			(message.type == video && client_config_map[pClient].video_broadcast) ||
			(message.type == audio && client_config_map[pClient].audio_broadcast)
		)
		{
			pClient->sendTextMessage(message.message);
		}
	}
}

void WSServer::broadcast_thread_safe(QString message)
{
	QMutexLocker locker(&_broadcastMutex);
	_broadcastQueue.enqueue({message, global});
}

void WSServer::broadcast_thread_safe(broadcast_message message)
{
	QMutexLocker locker(&_broadcastMutex);
	_broadcastQueue.enqueue(message);
}

void WSServer::add_audio_filter(ostws_audiofilter* audio_filter)
{
	QMutexLocker locker(&_audioFilterMutex);
	_audioFilters.append(audio_filter);
}

void WSServer::remove_audio_filter(ostws_audiofilter* audio_filter)
{
	QMutexLocker locker(&_audioFilterMutex);
	_audioFilters.removeAll(audio_filter);
}

void WSServer::onAudioBroadcastCycle()
{
	QMutexLocker locker(&_audioFilterMutex);
	OBSDataAutoRelease obs_data = obs_data_create();
	obs_data_set_string(obs_data, "update-type", "AudioUpdate");

	const auto source_array = obs_data_array_create();

	for (auto audio_filter : _audioFilters)
	{
		const auto source_data = obs_data_create();
		obs_data_set_string(source_data, "source", audio_filter->source_name);
		obs_data_set_double(source_data, "magnitude", audio_filter->magnitude);
		obs_data_set_double(source_data, "mul", audio_filter->mul);
		obs_data_set_int(source_data, "nr_channels", audio_filter->nr_channels);
		audio_filter->magnitude = AUDIO_MIN;

		obs_data_array_push_back(source_array, source_data);
	}
	obs_data_set_array(obs_data, "sources", source_array);
	WSServer::Instance->broadcast_thread_safe({
		obs_data_get_json(obs_data),
		audio
	});
}

void WSServer::onNewConnection()
{
	QWebSocket* pSocket = _wsServer->nextPendingConnection();
	if (pSocket)
	{
		connect(pSocket, SIGNAL(textMessageReceived(const QString&)),
		        this, SLOT(onTextMessageReceived(QString)));
		connect(pSocket, SIGNAL(disconnected()),
		        this, SLOT(onSocketDisconnected()));

		QMutexLocker locker(&_clMutex);
		_clients << pSocket;
		client_config_map[pSocket] = {};
		locker.unlock();

		QHostAddress clientAddr = pSocket->peerAddress();
		QString clientIp = Utils::FormatIPAddress(clientAddr);

		blog(LOG_INFO, "new client connection from %s:%d",
			clientIp.toUtf8().constData(), pSocket->peerPort());

		// obs_frontend_push_ui_translation(obs_module_get_string);
		// QString title = tr("OBSWebsocket.NotifyConnect.Title");
		// QString msg = tr("OBSWebsocket.NotifyConnect.Message")
		// 	.arg(Utils::FormatIPAddress(clientAddr));
		// obs_frontend_pop_ui_translation();
		//
		// Utils::SysTrayNotify(msg, QSystemTrayIcon::Information, title);
	}
}

void WSServer::onTextMessageReceived(QString message)
{
	QWebSocket* pSocket = qobject_cast<QWebSocket*>(sender());
	if (pSocket)
	{
		WSRequestHandler handler(pSocket);
		handler.processIncomingMessage(message);
	}
}

void WSServer::onSocketDisconnected()
{
	QWebSocket* pSocket = qobject_cast<QWebSocket*>(sender());
	if (pSocket)
	{
		QMutexLocker locker(&_clMutex);
		_clients.removeAll(pSocket);
		locker.unlock();

		pSocket->deleteLater();

		QHostAddress clientAddr = pSocket->peerAddress();
		QString clientIp = Utils::FormatIPAddress(clientAddr);

		blog(LOG_INFO, "client %s:%d disconnected",
			clientIp.toUtf8().constData(), pSocket->peerPort());

		// obs_frontend_push_ui_translation(obs_module_get_string);
		// QString title = tr("OBSWebsocket.NotifyDisconnect.Title");
		// QString msg = tr("OBSWebsocket.NotifyDisconnect.Message")
		// 	.arg(Utils::FormatIPAddress(clientAddr));
		// obs_frontend_pop_ui_translation();
		//
		// Utils::SysTrayNotify(msg, QSystemTrayIcon::Information, title);
	}
}
