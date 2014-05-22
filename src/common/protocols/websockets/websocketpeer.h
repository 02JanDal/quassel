/***************************************************************************
 *   Copyright (C) 2014 by the Quassel Project                             *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3.                                           *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#ifndef WEBSOCKETPEER_H
#define WEBSOCKETPEER_H

#include "../../remotepeer.h"

class QDataStream;

class WebSocketPeer : public RemotePeer
{
    Q_OBJECT

public:
    WebSocketPeer(AuthHandler *authHandler, SocketInterface *socket, quint16 features, Compressor::CompressionLevel level, QObject *parent = 0);

    Protocol::Type protocol() const { return Protocol::WebSocketProtocol; }
    QString protocolName() const { return "the WebSocket protocol"; }

    enum Features {
        Raw = 0x1
    };

    static quint16 supportedFeatures();
    static bool acceptsFeatures(quint16 peerFeatures);
    quint16 enabledFeatures() const;

    void dispatch(const Protocol::RegisterClient &msg);
    void dispatch(const Protocol::ClientDenied &msg);
    void dispatch(const Protocol::ClientRegistered &msg);
    void dispatch(const Protocol::SetupData &msg);
    void dispatch(const Protocol::SetupFailed &msg);
    void dispatch(const Protocol::SetupDone &msg);
    void dispatch(const Protocol::Login &msg);
    void dispatch(const Protocol::LoginFailed &msg);
    void dispatch(const Protocol::LoginSuccess &msg);
    void dispatch(const Protocol::SessionState &msg);

    void dispatch(const Protocol::SyncMessage &msg);
    void dispatch(const Protocol::RpcCall &msg);
    void dispatch(const Protocol::InitRequest &msg);
    void dispatch(const Protocol::InitData &msg);

    void dispatch(const Protocol::HeartBeat &msg);
    void dispatch(const Protocol::HeartBeatReply &msg);

public slots:
    void send(const QJsonObject &obj);

signals:
    void protocolError(const QString &errorString);

private slots:
    void messageReceived(const QString &msg);

private:
    void processMessage(const QByteArray &msg);

    QJsonDocument parseJson(const QByteArray &data) const;

    quint16 _features;

    QWebSocket *socket();

    static inline QString datetimeFormat() { return QString("dd:MM:yyyy HH:mm:ss.zzz"); }
};

#endif
