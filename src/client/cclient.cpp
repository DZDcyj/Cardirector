/********************************************************************
    Copyright (c) 2013-2015 - Mogara

    This file is part of Cardirector.

    This game engine is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 3.0
    of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the LICENSE file for more details.

    Mogara
*********************************************************************/

#include "cclient.h"
#include "cpacketrouter.h"
#include "ctcpsocket.h"
#include "cjsonpacketparser.h"
#include "cprotocol.h"
#include "cclientuser.h"

class CClientPrivate
{
public:
    CPacketRouter *router;
    QMap<uint, CClientUser *> users;
    CClientUser *self;
    CAbstractPacketParser *parser;
};

static QHash<int, CPacketRouter::Callback> interactions;
static QHash<int, CPacketRouter::Callback> callbacks;

CClient::CClient(QObject *parent)
    : QObject(parent)
    , p_ptr(new CClientPrivate)
{
    p_ptr->self = NULL;

    p_ptr->parser = new CJsonPacketParser;
    CTcpSocket *socket = new CTcpSocket;
    p_ptr->router = new CPacketRouter(this, socket, p_ptr->parser);
    p_ptr->router->setInteractions(&interactions);
    p_ptr->router->setCallbacks(&callbacks);
    connect(socket, &CTcpSocket::connected, this, &CClient::connected);
}

CClient::~CClient()
{
    delete p_ptr->parser;
    delete p_ptr;
}

void CClient::setPacketParser(CAbstractPacketParser *parser)
{
    delete p_ptr->parser;
    p_ptr->parser = parser;
}

CAbstractPacketParser *CClient::packetParser() const
{
    return p_ptr->parser;
}

void CClient::AddInteraction(int command, void (*callback)(QObject *, const QVariant &))
{
    interactions.insert(command, callback);
}

void CClient::AddCallback(int command, void (*callback)(QObject *, const QVariant &))
{
    callbacks.insert(command, callback);
}

void CClient::connectToHost(const QHostAddress &server, ushort port)
{
    CTcpSocket *socket = p_ptr->router->socket();
    socket->connectToHost(server, port);
}

void CClient::signup(const QString &username, const QString &password, const QString &screenName, const QString &avatar)
{
    QVariantList arguments;
    arguments << username << password << screenName << avatar;
    notifyServer(S_COMMAND_SIGNUP, arguments);
}

void CClient::login(const QString &username, const QString &password)
{
    QVariantList arguments;
    arguments << username << password;
    notifyServer(S_COMMAND_LOGIN, arguments);
}

void CClient::createRoom()
{
    p_ptr->router->notify(S_COMMAND_CREATE_ROOM);
}

void CClient::enterRoom(uint id)
{
    p_ptr->router->notify(S_COMMAND_ENTER_ROOM, id);
}

void CClient::exitRoom()
{
    p_ptr->router->notify(S_COMMAND_ENTER_ROOM);
}

void CClient::speakToServer(const QString &message)
{
    p_ptr->self->speak(message);
    return p_ptr->router->notify(S_COMMAND_SPEAK, message);
}

void CClient::addRobot()
{
    p_ptr->router->notify(S_COMMAND_ADD_ROBOT);
}

void CClient::startGame()
{
    p_ptr->router->notify(S_COMMAND_START_GAME);
}

const CClientUser *CClient::findUser(uint id) const
{
    return p_ptr->users.value(id);
}

QList<const CClientUser *> CClient::users() const
{
    QList<const CClientUser *> userList;
    foreach (const CClientUser *user, p_ptr->users)
        userList << user;
    return userList;
}

CClientUser *CClient::findUser(uint id)
{
    return p_ptr->users.value(id);
}

CClientUser *CClient::self() const
{
    return p_ptr->self;
}

void CClient::fetchRoomList()
{
    notifyServer(S_COMMAND_SET_ROOM_LIST);
}

CClientUser *CClient::addUser(const QVariant &data)
{
    QVariantList arguments = data.toList();
    if (arguments.length() < 3)
        return NULL;

    uint userId = arguments.at(0).toUInt();
    if (userId > 0) {
        CClientUser *user = new CClientUser(userId, this);
        user->setScreenName(arguments.at(1).toString());
        user->setAvatar(arguments.at(2).toString());

        p_ptr->users.insert(userId, user);
        return user;
    }

    return NULL;
}

void CClient::requestServer(int command, const QVariant &data, int timeout)
{
    p_ptr->router->request(command, data, timeout);
}

void CClient::replyToServer(int command, const QVariant &data)
{
    p_ptr->router->reply(command, data);
}

void CClient::notifyServer(int command, const QVariant &data)
{
    p_ptr->router->notify(command, data);
}

int CClient::requestTimeout() const
{
    return p_ptr->router->requestTimeout();
}

QVariant CClient::waitForReply()
{
    return p_ptr->router->waitForReply();
}

QVariant CClient::waitForReply(int timeout)
{
    return p_ptr->router->waitForReply(timeout);
}

/* Callbacks */

void CClient::SetUserListCommand(QObject *receiver, const QVariant &data)
{
    QVariantList userList(data.toList());
    CClient *client = qobject_cast<CClient *>(receiver);

    if (!client->p_ptr->users.isEmpty()) {
        foreach (CClientUser *user, client->p_ptr->users) {
            if (user != client->self())
                user->deleteLater();
        }
        client->p_ptr->users.clear();
        client->p_ptr->users.insert(client->self()->id(), client->self());
    }

    foreach (const QVariant &user, userList)
        client->addUser(user);
}

void CClient::AddUserCommand(QObject *receiver, const QVariant &data)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    CClientUser *user = client->addUser(data);
    emit client->userAdded(user);
}

void CClient::RemoveUserCommand(QObject *receiver, const QVariant &data)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    uint userId = data.toUInt();
    CClientUser *user = client->findUser(userId);
    if (user != NULL) {
        emit client->userRemoved(user);
        client->p_ptr->users.remove(userId);
        user->deleteLater();
    }
}

void CClient::LoginCommand(QObject *receiver, const QVariant &data)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    client->p_ptr->self = client->addUser(data);
    emit client->loggedIn();
}

void CClient::SetRoomListCommand(QObject *receiver, const QVariant &data)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    emit client->roomListUpdated(data);
}

void CClient::SpeakCommand(QObject *receiver, const QVariant &data)
{
    QVariantList arguments = data.toList();
    if (arguments.size() < 2)
        return;

    CClient *client = qobject_cast<CClient *>(receiver);
    const QVariant who = arguments.at(0);
    QString message = arguments.at(1).toString();
    if (who.isNull()) {
        emit client->systemMessage(message);
    } else {
        CClientUser *user = client->findUser(who.toUInt());
        if (user != NULL)
            emit user->speak(message);
    }
}

void CClient::EnterRoomCommand(QObject *receiver, const QVariant &data)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    emit client->roomEntered(data);
}

void CClient::NetworkDelayCommand(QObject *receiver, const QVariant &data)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    client->notifyServer(S_COMMAND_NETWORK_DELAY, data);
}

void CClient::StartGameCommand(QObject *receiver, const QVariant &)
{
    CClient *client = qobject_cast<CClient *>(receiver);
    emit client->gameStarted();
}

void CClient::Init()
{
    AddCallback(S_COMMAND_SPEAK, &SpeakCommand);
    AddCallback(S_COMMAND_SET_USER_LIST, &SetUserListCommand);
    AddCallback(S_COMMAND_ADD_USER, &AddUserCommand);
    AddCallback(S_COMMAND_REMOVE_USER, &RemoveUserCommand);
    AddCallback(S_COMMAND_LOGIN, &LoginCommand);
    AddCallback(S_COMMAND_SET_ROOM_LIST, &SetRoomListCommand);
    AddCallback(S_COMMAND_ENTER_ROOM, &EnterRoomCommand);
    AddCallback(S_COMMAND_NETWORK_DELAY, &NetworkDelayCommand);
    AddCallback(S_COMMAND_START_GAME, &StartGameCommand);
}
C_INITIALIZE_CLASS(CClient)
