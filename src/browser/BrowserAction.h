/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#ifndef BROWSERACTION_H
#define BROWSERACTION_H

#include "BrowserMessageBuilder.h"
#include "BrowserService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class QLocalSocket;

struct BrowserRequest
{
    QString action;
    QString requestId;
    QString hash;
    QString nonce;
    QString incrementedNonce;
    QJsonObject decrypted;

    inline bool isEmpty() const
    {
        return decrypted.isEmpty();
    }

    inline QJsonArray getArray(const QString& param) const
    {
        return decrypted.value(param).toArray();
    }

    inline bool getBool(const QString& param) const
    {
        return decrypted.value(param).toBool();
    }

    inline QString getString(const QString& param) const
    {
        return decrypted.value(param).toString();
    }
};

class BrowserAction
{
public:
    explicit BrowserAction() = default;
    ~BrowserAction() = default;

    QJsonObject processClientMessage(const QJsonObject& message, QLocalSocket* socket);

private:
    QJsonObject handleAction(const QJsonObject& message, QLocalSocket* socket);
    QJsonObject handleAssociate(const BrowserRequest& browserRequest);
    QJsonObject handleChangePublicKeys(const QJsonObject& message);
    QJsonObject handleCreateCredentials(const BrowserRequest& browserRequest);
    QJsonObject handleCreateNewGroup(const BrowserRequest& browserRequest);
    QJsonObject handleGetDatabaseStatuses(const BrowserRequest& browserRequest);
    QJsonObject handleDeleteEntry(const BrowserRequest& browserRequest);
    QJsonObject handleGeneratePassword(const BrowserRequest& browserRequest, QLocalSocket* socket);
    QJsonObject handleGetCredentials(const BrowserRequest& browserRequest);
    QJsonObject handleGetDatabaseEntries(const BrowserRequest& browserRequest);
    QJsonObject handleGetDatabaseGroups(const BrowserRequest& browserRequest);
    QJsonObject handleGetTotp(const BrowserRequest& browserRequest);
    QJsonObject handleGlobalAutoType(const BrowserRequest& browserRequest);
    QJsonObject handleLockDatabase(const BrowserRequest& browserRequest);

private:
    QJsonObject buildResponse(const BrowserRequest& browserRequest, const Parameters& params = {}) const;
    QJsonObject buildErrorResponse(const BrowserRequest& browserRequest, const int errorCode) const;
    QJsonObject getErrorReply(const QString& action, const int errorCode) const;
    QJsonObject decryptMessage(const QString& message, const QString& nonce) const;
    BrowserRequest decodeRequest(const QJsonObject& message) const;
    StringPairList getConnectionKeys(const BrowserRequest& browserRequest) const;
    bool isDatabaseConnected(const BrowserRequest& browserRequest) const;

private:
    static const int MaxUrlLength;

    QString m_clientPublicKey;
    QString m_publicKey;
    QString m_secretKey;

    friend class TestBrowser;
};

#endif // BROWSERACTION_H
