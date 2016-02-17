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

#include "cprotocol.h"
#include "cserveragent.h"
#include "cserverrobot.h"
#include "cserver.h"
#include "cserveruser.h"
#include "croom.h"

class CServerAgentPrivate
{
public:
    CServer *server;
    CRoom *room;

    int requestCommand;
    QVariant requestData;

    bool ready;
};

CServerAgent::CServerAgent(CServer *server)
    : CAbstractUser(server)
{
    p_ptr = new CServerAgentPrivate;

    p_ptr->server = server;
    p_ptr->room = NULL;
    p_ptr->ready = false;
}

CServerAgent::~CServerAgent()
{
    delete p_ptr;
}

CServer *CServerAgent::server() const
{
    return p_ptr->server;
}

CRoom *CServerAgent::room() const
{
    return p_ptr->room;
}

void CServerAgent::setRoom(CRoom *room)
{
    p_ptr->room = room;
}

QVariant CServerAgent::briefIntroduction() const
{
    QVariantList arguments;
    arguments << id();
    arguments << screenName();
    arguments << avatar();
    return arguments;
}

void CServerAgent::prepareRequest(int command, const QVariant &data)
{
    p_ptr->requestCommand = command;
    p_ptr->requestData = data;
}

CServerUser *CServerAgent::toUser()
{
    return qobject_cast<CServerUser *>(this);
}

CServerRobot *CServerAgent::toRobot()
{
    return qobject_cast<CServerRobot *>(this);
}

void CServerAgent::broadcastProperty(const char *name)
{
    if (p_ptr->room) {
        QVariantList data;
        data << name;
        data << property(name);
        data << id();
        p_ptr->room->broadcastNotification(S_COMMAND_UPDATE_USER_PROPERTY, data);
    } else {
        QVariantList data;
        data << name;
        data << property(name);
        notify(S_COMMAND_UPDATE_USER_PROPERTY, data);
    }
}

int CServerAgent::requestCommand() const
{
    return p_ptr->requestCommand;
}

QVariant CServerAgent::requestData() const
{
    return p_ptr->requestData;
}
