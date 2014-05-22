/***************************************************************************
 *   Copyright (C) 2005-2014 by the Quassel Project                        *
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

#include <QtEndian>

#include <QHostAddress>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "websocketpeer.h"

#include "logger.h"
#include "identity.h"
#include "bufferinfo.h"

using namespace Protocol;

WebSocketPeer::WebSocketPeer(::AuthHandler *authHandler, SocketInterface *socket, quint16 features, Compressor::CompressionLevel level, QObject *parent)
    : RemotePeer(authHandler, socket, level, parent), _features(features)
{
    connect(qobject_cast<WebSocketSocket *>(socket)->_socket, SIGNAL(textMessageReceived(QString)), this, SLOT(messageReceived(QString)));
}

quint16 WebSocketPeer::supportedFeatures()
{
    return Raw;
}
bool WebSocketPeer::acceptsFeatures(quint16 peerFeatures)
{
    if (~peerFeatures & ~supportedFeatures()) {
        return true;
    }
    return false;
}
quint16 WebSocketPeer::enabledFeatures() const
{
    return _features;
}

void WebSocketPeer::processMessage(const QByteArray &msg)
{
    const QJsonObject obj = parseJson(msg).object();
    if (!signalProxy()) {
        const QString type = obj.value("type").toString();
        qDebug() << "received message:" << type << msg;
        if (type.isEmpty()) {
            emit protocolError(tr("Invalid handshake message!"));
            return;
        }
        if (type == "ClientInit") {
            handle(RegisterClient(obj.value("clientVersion").toString(), obj.value("buildDate").toString()));
        }
        else if (type == "ClientInitReject") {
            handle(ClientDenied(obj.value("errorString").toString()));
        }
        else if (type == "ClientInitAck") {
            handle(ClientRegistered(obj.value("coreFeatures").toInt(), obj.value("coreConfigured").toBool(), obj.value("backendInfo").toArray().toVariantList(), false, QDateTime::fromString(obj.value("coreStartTime").toString(), Qt::ISODate)));
        }
        else if (type == "CoreSetupData") {
            handle(SetupData(obj.value("adminUser").toString(), obj.value("adminPassword").toString(), obj.value("backend").toString(), obj.value("setupData").toObject().toVariantMap()));
        }
        else if (type == "CoreSetupReject") {
            handle(SetupFailed(obj.value("errorString").toString()));
        }
        else if (type == "CoreSetupAck") {
            handle(SetupDone());
        }
        else if (type == "ClientLogin") {
            handle(Login(obj.value("user").toString(), obj.value("password").toString()));
        }
        else if (type == "ClientLoginReject") {
            handle(LoginFailed(obj.value("errorString").toString()));
        }
        else if (type == "ClientLoginAck") {
            handle(LoginSuccess());
        }
        else if (type == "SessionInit") {
            handle(SessionState(obj.value("identities").toArray().toVariantList(), obj.value("bufferInfos").toArray().toVariantList(), obj.value("networkIds").toArray().toVariantList()));
        }
        else {
            emit protocolError(tr("Unknown protocol message of type %1").arg(type));
        }
    }
    else {
        const QString type = obj.value("type").toString();
        if (type.isEmpty()) {
            emit protocolError(tr("Invalid request message!"));
        }
        if (type == "Sync") {
            handle(SyncMessage(obj.value("className").toString().toUtf8(), obj.value("objectName").toString(), obj.value("slotName").toString().toUtf8(), obj.value("params").toArray().toVariantList()));
        }
        else if (type == "RpcCall") {
            handle(RpcCall(obj.value("slotName").toString().toUtf8(), obj.value("params").toArray().toVariantList()));
        }
        else if (type == "InitRequest") {
            handle(InitRequest(obj.value("className").toString().toUtf8(), obj.value("objectName").toString()));
        }
        else if (type == "InitData") {
            handle(InitData(obj.value("className").toString().toUtf8(), obj.value("objectName").toString(), obj.value("params").toObject().toVariantMap()));
        }
        else if (type == "HeartBeat") {
            handle(HeartBeat(QDateTime::fromString(obj.value("timestamp").toString(), datetimeFormat())));
        }
        else if (type == "HeartBeatReply") {
            handle(HeartBeatReply(QDateTime::fromString(obj.value("timestamp").toString(), datetimeFormat())));
        }
    }
}

QJsonDocument WebSocketPeer::parseJson(const QByteArray &data) const
{
    if (_features & Raw) {
        return QJsonDocument::fromBinaryData(data);
    }
    else {
        return QJsonDocument::fromJson(data);
    }
}

QJsonValue variantToValue(const QVariant &in)
{
    if (in.canConvert<QVariantList>()) {
        QJsonArray array;
        QSequentialIterable it = in.value<QSequentialIterable>();
        foreach (const QVariant &val, it) {
            array.append(variantToValue(val));
        }
        return array;
    }
    else if (in.canConvert<QVariantHash>()) {
        QJsonObject object;
        QAssociativeIterable iterable = in.value<QAssociativeIterable>();
        for (QAssociativeIterable::const_iterator it = iterable.begin(); it != iterable.end(); ++it) {
            object.insert(it.key().toString(), variantToValue(it.value()));
        }
        return object;
    }
    else if (in.type() == QVariant::Bool) {
        return QJsonValue(in.toBool());
    }
    else if (in.type() == QVariant::Double) {
        return QJsonValue(in.toDouble());
    }
    else if (in.canConvert<qint64>()) {
        return QJsonValue(in.toLongLong());
    }
    else if (in.canConvert<QString>()) {
        return QJsonValue(in.toString());
    }
    else if (in.canConvert<IdentityId>()) {
        return variantToValue(in.value<IdentityId>().toVariant());
    }
    else {
        quWarning() << "Unknown variant type" << in.type();
        return QJsonValue();
    }
}

template<class T>
QJsonArray listToJson(const QVariantList &in)
{
    QJsonArray out;
    foreach (const QVariant &item, in) {
        out.append(variantToValue(item.value<T>().toVariant()));
    }
    return out;
}

QWebSocket *WebSocketPeer::socket()
{
    return qobject_cast<WebSocketSocket *>(RemotePeer::socket())->_socket;
}

void WebSocketPeer::dispatch(const RegisterClient &msg)
{
    QJsonObject obj;
    obj.insert("type", "RegisterClient");
    obj.insert("clientVersion", msg.clientVersion);
    obj.insert("buildDate", msg.buildDate);
    send(obj);
}
void WebSocketPeer::dispatch(const ClientDenied &msg)
{
    QJsonObject obj;
    obj.insert("type", "ClientDenied");
    obj.insert("errorString", msg.errorString);
    send(obj);
}
void WebSocketPeer::dispatch(const ClientRegistered &msg)
{
    QJsonObject obj;
    obj.insert("type", "ClientRegistered");
    obj.insert("coreFeatures", QJsonValue((qint64)msg.coreFeatures));
    obj.insert("coreConfigured", msg.coreConfigured);
    obj.insert("backendInfo", QJsonArray::fromVariantList(msg.backendInfo));
    obj.insert("coreStartTime", msg.coreStartTime.toString(Qt::ISODate));
    send(obj);
}
void WebSocketPeer::dispatch(const SetupData &msg)
{
    QJsonObject obj;
    obj.insert("type", "SetupData");
    obj.insert("adminUser", msg.adminUser);
    obj.insert("adminPassword", msg.adminPassword);
    obj.insert("backend", msg.backend);
    obj.insert("setupData", QJsonObject::fromVariantMap(msg.setupData));
    send(obj);
}
void WebSocketPeer::dispatch(const SetupFailed &msg)
{
    QJsonObject obj;
    obj.insert("type", "SetupFailed");
    obj.insert("errorString", msg.errorString);
    send(obj);
}
void WebSocketPeer::dispatch(const SetupDone &msg)
{
    QJsonObject obj;
    obj.insert("type", "SetupDone");
    send(obj);
}
void WebSocketPeer::dispatch(const Login &msg)
{
    QJsonObject obj;
    obj.insert("type", "Login");
    obj.insert("user", msg.user);
    obj.insert("password", msg.password);
    send(obj);
}
void WebSocketPeer::dispatch(const LoginFailed &msg)
{
    QJsonObject obj;
    obj.insert("type", "LoginFailed");
    obj.insert("errorString", msg.errorString);
    send(obj);
}
void WebSocketPeer::dispatch(const LoginSuccess &msg)
{
    QJsonObject obj;
    obj.insert("type", "LoginSuccess");
    send(obj);
}
void WebSocketPeer::dispatch(const SessionState &msg)
{
    QJsonObject obj;
    obj.insert("type", "SessionState");
    obj.insert("identities", listToJson<Identity>(msg.identities));
    obj.insert("bufferInfos", listToJson<BufferInfo>(msg.bufferInfos));
    obj.insert("networkIds", listToJson<NetworkId>(msg.networkIds));
    send(obj);
}


/*** Standard messages ***/

void WebSocketPeer::dispatch(const SyncMessage &msg)
{
    QJsonObject obj;
    obj.insert("type", "SyncMessage");
    obj.insert("className", QString::fromUtf8(msg.className));
    obj.insert("objectName", msg.objectName);
    obj.insert("slotName", QString::fromUtf8(msg.slotName));
    obj.insert("params", QJsonArray::fromVariantList(msg.params));
    send(obj);
}
void WebSocketPeer::dispatch(const RpcCall &msg)
{
    QJsonObject obj;
    obj.insert("type", "RpcCall");
    obj.insert("slotName", QString::fromUtf8(msg.slotName));
    obj.insert("params", QJsonArray::fromVariantList(msg.params));
    send(obj);
}
void WebSocketPeer::dispatch(const InitRequest &msg)
{
    QJsonObject obj;
    obj.insert("type", "InitRequest");
    obj.insert("className", QString::fromUtf8(msg.className));
    obj.insert("objectName", msg.objectName);
    send(obj);
}
void WebSocketPeer::dispatch(const InitData &msg)
{
    QJsonObject obj;
    obj.insert("type", "InitData");
    obj.insert("className", QString::fromUtf8(msg.className));
    obj.insert("objectName", msg.objectName);
    obj.insert("initData", QJsonObject::fromVariantMap(msg.initData));
    send(obj);
}

void WebSocketPeer::dispatch(const HeartBeat &msg)
{
    QJsonObject obj;
    obj.insert("type", "HeartBeat");
    obj.insert("timestamp", msg.timestamp.toString(datetimeFormat()));
    send(obj);
}
void WebSocketPeer::dispatch(const HeartBeatReply &msg)
{
    QJsonObject obj;
    obj.insert("type", "HeartBeatReply");
    obj.insert("timestamp", msg.timestamp.toString(datetimeFormat()));
    send(obj);
}

void WebSocketPeer::send(const QJsonObject &obj)
{
    if (_features & Raw) {
        socket()->sendBinaryMessage(QJsonDocument(obj).toBinaryData());
    }
    else {
        socket()->sendTextMessage(QString::fromUtf8(QJsonDocument(obj).toJson()));
    }
    socket()->flush();
}

void WebSocketPeer::messageReceived(const QString &msg)
{
    processMessage(msg.toUtf8());
}
