/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NativeMessagingHost.h"
#include <QCoreApplication>
#include "core/Tools.h"

#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

namespace Tools {
    void sleep(int ms)
    {
        Q_ASSERT(ms >= 0);

        if (ms == 0) {
            return;
        }

#ifdef Q_OS_WIN
        Sleep(uint(ms));
#else
        timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000 * 1000;
        nanosleep(&ts, nullptr);
#endif
    }
}

NativeMessagingHost::NativeMessagingHost()
    : NativeMessagingBase(true)
    , m_connected(false)
{
    connect(this, SIGNAL(reconnect()), this, SLOT(connectSocket()));

    m_localSocket = new QLocalSocket();
    m_localSocket->setReadBufferSize(NATIVE_MSG_MAX_LENGTH);

    int socketDesc = m_localSocket->socketDescriptor();
    if (socketDesc) {
        int max = NATIVE_MSG_MAX_LENGTH;
        setsockopt(socketDesc, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&max), sizeof(max));
    }
#ifdef Q_OS_WIN
    m_running.store(1);
    m_future =
        QtConcurrent::run(this, static_cast<void (NativeMessagingHost::*)()>(&NativeMessagingHost::readNativeMessages));
#endif

    connect(m_localSocket, SIGNAL(connected()), this, SLOT(newConnection()));
    connect(m_localSocket, SIGNAL(readyRead()), this, SLOT(newLocalMessage()));
    connect(m_localSocket, SIGNAL(disconnected()), this, SLOT(deleteSocket()));
    connect(m_localSocket,
            SIGNAL(stateChanged(QLocalSocket::LocalSocketState)),
            SLOT(socketStateChanged(QLocalSocket::LocalSocketState)));

    emit reconnect();
}

NativeMessagingHost::~NativeMessagingHost()
{
#ifdef Q_OS_WIN
    m_future.waitForFinished();
#endif
}

void NativeMessagingHost::readNativeMessages()
{
#ifdef Q_OS_WIN
    quint32 length = 0;
    while (m_running.load() && !std::cin.eof()) {
        length = 0;
        std::cin.read(reinterpret_cast<char*>(&length), 4);
        if (!readStdIn(length)) {
            QCoreApplication::quit();
        }
        QThread::msleep(1);
    }
#endif
}

void NativeMessagingHost::readLength()
{
    quint32 length = 0;
    std::cin.read(reinterpret_cast<char*>(&length), 4);
    if (!std::cin.eof() && length > 0) {
        readStdIn(length);
    } else {
        QCoreApplication::quit();
    }
}

bool NativeMessagingHost::readStdIn(const quint32 length)
{
    if (length <= 0) {
        return false;
    }

    QByteArray arr;
    arr.reserve(length);

    for (quint32 i = 0; i < length; ++i) {
        int c = std::getchar();
        if (c == EOF) {
            // message ended prematurely, ignore it and return
            return false;
        }
        arr.append(static_cast<char>(c));
    }

    if (arr.length() > 0 && m_connected && m_localSocket && m_localSocket->state() == QLocalSocket::ConnectedState) {
        m_localSocket->write(arr.constData(), arr.length());
        m_localSocket->flush();
    }

    return true;
}

void NativeMessagingHost::newLocalMessage()
{
    if (!m_localSocket || m_localSocket->bytesAvailable() <= 0) {
        return;
    }

    QByteArray arr = m_localSocket->readAll();
    if (!arr.isEmpty()) {
        sendReply(arr);
    }
}

void NativeMessagingHost::newConnection()
{
    auto s = m_localSocket->socketDescriptor();
    qDebug("New connection ID: %d", s);
    
    QJsonObject reply;
    reply["action"] = "reconnected";
    sendReply(reply);
    m_connected = true;
}

void NativeMessagingHost::connectSocket()
{
    m_localSocket->connectToServer(getLocalServerPath());
}

void NativeMessagingHost::deleteSocket()
{
    /*if (m_notifier) {
        m_notifier->setEnabled(false);
    }*/
    /*m_localSocket->deleteLater();
    QCoreApplication::quit();*/
    m_connected = false;
}


void NativeMessagingHost::socketStateChanged(QLocalSocket::LocalSocketState socketState)
{
    QString state;

    switch (socketState) {
        case QLocalSocket::UnconnectedState: state = "QLocalSocket::UnconnectedState"; break;
        case QLocalSocket::ConnectingState: state = "QLocalSocket::ConnectingState"; break;
        case QLocalSocket::ConnectedState: state = "QLocalSocket::ConnectedState"; break;
        case QLocalSocket::ClosingState: state = "QLocalSocket::ClosingState"; break;
    }

    auto s = m_localSocket->socketDescriptor();
    qDebug("socketStateChanged %d to: %s", s, state.toStdString().c_str());

#ifdef Q_OS_WIN
    if (socketState == QLocalSocket::UnconnectedState || socketState == QLocalSocket::ClosingState) {
        m_running.testAndSetOrdered(true, false);
    }
#endif
    if (socketState == QLocalSocket::UnconnectedState) {
        qDebug("Reconnect");
        Tools::sleep(1000);
        emit reconnect();
    }
}
