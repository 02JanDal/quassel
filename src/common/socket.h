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

#ifndef SOCKET_H
#define SOCKET_H

#include <QTcpSocket>
#include <QHostAddress>
#ifdef WITH_WEBSOCKETS
#  include <QWebSocket>
#endif

class SocketInterface : public QObject
{
    Q_OBJECT
public:
    explicit SocketInterface() : QObject(0)
    {
        qRegisterMetaType<QAbstractSocket::SocketError>();
        qRegisterMetaType<QAbstractSocket::SocketState>();
    }
    virtual ~SocketInterface()
    {
    }
    virtual bool exists() const = 0;
    virtual QHostAddress peerAddress() const = 0;
    virtual bool isOpen() const = 0;
    virtual bool isValid() const = 0;
    virtual QString errorString() const = 0;
    virtual void close() = 0;
    virtual bool flush() = 0;
    virtual QAbstractSocket::SocketState state() const = 0;
    virtual void disconnectFromHost() = 0;
    virtual qint64 bytesAvailable() const { return -1; }

signals:
    void disconnected();
    void error(QAbstractSocket::SocketError);
    void stateChanged(QAbstractSocket::SocketState);
};
class TcpSocket : public SocketInterface
{
    Q_OBJECT
public:
    TcpSocket(QTcpSocket *socket) : SocketInterface(), _socket(socket)
    {
        socket->setParent(this);
        connect(socket, SIGNAL(disconnected()), SIGNAL(disconnected()));
        connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), SIGNAL(error(QAbstractSocket::SocketError)));
        connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), SIGNAL(stateChanged(QAbstractSocket::SocketState)));
    }
    QTcpSocket *_socket;
    bool exists() const { return !!_socket; }
    QHostAddress peerAddress() const { return _socket->peerAddress(); }
    bool isOpen() const { return _socket->isOpen(); }
    bool isValid() const { return _socket->isValid(); }
    QString errorString() const { return _socket->errorString(); }
    void close() { _socket->close(); }
    bool flush() { return _socket->flush(); }
    QAbstractSocket::SocketState state() const { return _socket->state(); }
    void disconnectFromHost() { _socket->disconnectFromHost(); }
    qint64 bytesAvailable() const { return _socket->bytesAvailable(); }
};
#ifdef WITH_WEBSOCKETS
class WebSocketSocket : public SocketInterface
{
    Q_OBJECT
public:
    WebSocketSocket(QWebSocket *socket) : SocketInterface(), _socket(socket)
    {
        socket->setParent(this);
        connect(socket, SIGNAL(disconnected()), SIGNAL(disconnected()));
        connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), SIGNAL(error(QAbstractSocket::SocketError)));
        connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), SIGNAL(stateChanged(QAbstractSocket::SocketState)));
    }
    QWebSocket *_socket;
    bool exists() const { return !!_socket; }
    QHostAddress peerAddress() const { return _socket->peerAddress(); }
    bool isOpen() const { return _socket->isValid(); }
    bool isValid() const { return _socket->isValid(); }
    QString errorString() const { return _socket->errorString(); }
    void close() { _socket->close(); }
    bool flush() { return _socket->flush(); }
    QAbstractSocket::SocketState state() const { return _socket->state(); }
    void disconnectFromHost() { _socket->close(QWebSocketProtocol::CloseCodeGoingAway, tr("Disconnecting")); }
};
#endif

#endif
