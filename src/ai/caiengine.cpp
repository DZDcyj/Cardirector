/********************************************************************
    Copyright (c) 2013-2016 - Mogara

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

#include "caiengine.h"

#include <QFile>
#include <QAtomicInt>
#include <QDebug>

class CAiEnginePrivate
{
public:
    QAtomicInt available;

    CAiEnginePrivate()
        : available(0)
    {
    }

    void aiEngineError(const QJSValue &error) {
        if (!error.isError())
            return;

        qDebug() << "AI Engine Error!!"
                 << error.toString()
                 << QString()
                 << ("name: " + error.property("name").toString())
                 << ("message: " + error.property("message").toString())
                 << ("fileName: " + error.property("fileName").toString())
                 << ("lineNumber: " + QString::number(error.property("lineNumber").toInt()))
                 << ("stack: " + error.property("stack").toString());
    }
};

CAiEngine::CAiEngine()
    : p_ptr(new CAiEnginePrivate)
{
    QJSValue functions = newQObject(new CAiEngineFunctions(this));
    globalObject().setProperty("CAi", functions);
}

CAiEngine::~CAiEngine()
{
    C_P(CAiEngine);
    delete p;
}

void CAiEngine::request(int command, QVariant data)
{
    C_P(CAiEngine);
    if (p->available.load() == 0) {
        qDebug() << "AI engine not initialized when requesting";
        emit replyReady(QVariant());
        return;
    }
    QJSValue commandValue(command);
    QJSValue dataValue = toScriptValue(data);
    QJSValue requestFunction = globalObject().property("request");
    if (requestFunction.isCallable()) {
        QJSValue r = requestFunction.call(QJSValueList() << commandValue << dataValue);
        if (r.isError()) {
            qDebug() << "AI error when requesting";
            p->aiEngineError(r);
            emit replyReady(QVariant());
            return;
        }
        emit replyReady(r.toVariant());
    } else {
        qDebug() << "the function for requesting doesn't exist or is broken";
        emit replyReady(QVariant());
    }
}

void CAiEngine::reply(int command, QVariant data)
{
    C_P(CAiEngine);
    if (p->available.load() == 0) {
        qDebug() << "AI engine not initialized when replying";
        return;
    }
    QJSValue commandValue(command);
    QJSValue dataValue = toScriptValue(data);
    QJSValue replyFunction = globalObject().property("reply");
    if (replyFunction.isCallable()) {
        QJSValue r = replyFunction.call(QJSValueList() << commandValue << dataValue);
        if (r.isError()) {
            qDebug() << "AI error when replying";
            p->aiEngineError(r);
        }
    } else
        qDebug() << "the function for replying doesn't exist or is broken";
}

void CAiEngine::notify(int command, QVariant data)
{
    C_P(CAiEngine);
    if (p->available.load() == 0) {
        qDebug() << "AI engine not initialized when notifying";
        return;
    }
    QJSValue commandValue(command);
    QJSValue dataValue = toScriptValue(data);
    QJSValue notifyFunction = globalObject().property("notify");
    if (notifyFunction.isCallable()) {
        QJSValue r = notifyFunction.call(QJSValueList() << commandValue << dataValue);
        if (r.isError()) {
            qDebug() << "AI error when notifying";
            p->aiEngineError(r);
        }
    } else
        qDebug() << "the function for noitfying doesn't exist or is broken";
}

void CAiEngine::init(QString startScriptFile)
{
    C_P(CAiEngine);
    if (p->available.load() != 0) {
        qDebug() << "AI engine is initialized when initializing";
        emit initFinish(false);
        return;
    }

    QFile file(startScriptFile);
    if (file.open(QFile::ReadOnly)) {
        QString s = file.readAll();
        file.close();

        QJSValue r = evaluate(s, startScriptFile);
        if (r.isError()) {
            qDebug() << "AI engine initialization failed in reading the script file.";
            p->aiEngineError(r);
            emit initFinish(false);
            return;
        }

        QJSValue initFunction = globalObject().property("init");
        if (initFunction.isCallable()) {
            QJSValue callResult = initFunction.call();
            if (callResult.isError()) {
                qDebug() << "AI engine initialization failed in executing the initialization function";
                p->aiEngineError(callResult);
                emit initFinish(false);
                return;
            }
            if (callResult.toBool()) {
                p->available.store(1);
                emit initFinish(true);
                return;
            } else {
                qDebug() << "AI script initialization function returned false or returned an invalid value";
                emit initFinish(false);
                return;
            }
        } else {
            qDebug() << "AI script has no initialization function or the initialization function is broken";
            emit initFinish(false);
            return;
        }
    } else {
        qDebug() << ("AI engine initialization failed because open " + startScriptFile + " failed");
        emit initFinish(false);
        return;
    }

    qDebug() << "unhandled error in AI engine initialization";
    emit initFinish(false);
}

bool CAiEngine::avaliable() const
{
    C_P(const CAiEngine);
    return p->available.load() != 0;
}

CAiEngineFunctions::CAiEngineFunctions(CAiEngine *aiEngine)
    : QObject(aiEngine)
    , m_aiEngine(aiEngine)
{

}

void CAiEngineFunctions::notifyToRobot(const QJSValue &command, const QJSValue &data)
{
    emit m_aiEngine->notifyToRobot(command.toInt(), data.toVariant());
}
