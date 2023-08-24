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

#include "BrowserAction.h"
#include "BrowserSettings.h"
#include "config-keepassx.h"
#include "core/Global.h"
#include "core/Tools.h"

#include <QJsonDocument>
#include <QLocalSocket>

const int BrowserAction::MaxUrlLength = 256;

static const QString BROWSER_REQUEST_ASSOCIATE = QStringLiteral("associate");
static const QString BROWSER_REQUEST_CHANGE_PUBLIC_KEYS = QStringLiteral("change-public-keys");
static const QString BROWSER_REQUEST_CREATE_CREDENTIALS = QStringLiteral("create-credentials");
static const QString BROWSER_REQUEST_CREATE_NEW_GROUP = QStringLiteral("create-new-group");
static const QString BROWSER_REQUEST_DELETE_ENTRY = QStringLiteral("delete-entry");
static const QString BROWSER_REQUEST_GENERATE_PASSWORD = QStringLiteral("generate-password");
static const QString BROWSER_REQUEST_GET_CREDENTIALS = QStringLiteral("get-credentials");
static const QString BROWSER_REQUEST_GET_DATABASE_ENTRIES = QStringLiteral("get-database-entries");
static const QString BROWSER_REQUEST_GET_DATABASE_GROUPS = QStringLiteral("get-database-groups");
static const QString BROWSER_REQUEST_GET_DATABASE_STATUSES = QStringLiteral("get-database-statuses");
static const QString BROWSER_REQUEST_GET_TOTP = QStringLiteral("get-totp");
static const QString BROWSER_REQUEST_LOCK_DATABASE = QStringLiteral("lock-database");
static const QString BROWSER_REQUEST_REQUEST_AUTOTYPE = QStringLiteral("request-autotype");

QJsonObject BrowserAction::processClientMessage(const QJsonObject& message, QLocalSocket* socket)
{
    if (message.isEmpty()) {
        return getErrorReply("", ERROR_KEEPASS_EMPTY_MESSAGE_RECEIVED);
    }

    const auto action = message.value("action").toString();
    const auto triggerUnlock = message.value("triggerUnlock").toBool();

    if (action.compare(BROWSER_REQUEST_CHANGE_PUBLIC_KEYS) != 0 && m_clientPublicKey.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED);
    }

    if (triggerUnlock && !browserService()->openDatabase(triggerUnlock)) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_NOT_OPENED);
    }

    return handleAction(message, socket);
}

// Private functions
///////////////////////

QJsonObject BrowserAction::handleAction(const QJsonObject& message, QLocalSocket* socket)
{
    // Handle unencrypted requests (change-public-keys)
    const auto action = message.value("action").toString();
    if (action.compare(BROWSER_REQUEST_CHANGE_PUBLIC_KEYS) == 0) {
        return handleChangePublicKeys(message);
    }

    // Decrypt
    const auto browserRequest = decodeRequest(message);
    qDebug() << "action: " << browserRequest.action << ", Request ID: " << browserRequest.requestId;
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    // Handle decrypted requests
    if (browserRequest.action.compare(BROWSER_REQUEST_GET_DATABASE_STATUSES) == 0) {
        return handleGetDatabaseStatuses(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_ASSOCIATE) == 0) {
        return handleAssociate(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_GET_CREDENTIALS) == 0) {
        return handleGetCredentials(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_GENERATE_PASSWORD) == 0) {
        return handleGeneratePassword(browserRequest, socket);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_CREATE_CREDENTIALS) == 0) {
        return handleCreateCredentials(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_LOCK_DATABASE) == 0) {
        return handleLockDatabase(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_GET_DATABASE_GROUPS) == 0) {
        return handleGetDatabaseGroups(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_CREATE_NEW_GROUP) == 0) {
        return handleCreateNewGroup(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_GET_TOTP) == 0) {
        return handleGetTotp(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_DELETE_ENTRY) == 0) {
        return handleDeleteEntry(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_GET_DATABASE_ENTRIES) == 0) {
        return handleGetDatabaseEntries(browserRequest);
    } else if (browserRequest.action.compare(BROWSER_REQUEST_REQUEST_AUTOTYPE) == 0) {
        return handleGlobalAutoType(browserRequest);
    }

    // Action was not recognized
    return buildErrorResponse(browserRequest, ERROR_KEEPASS_INCORRECT_ACTION);
}

QJsonObject BrowserAction::handleAssociate(const BrowserRequest& browserRequest)
{
    const auto publicKey = browserRequest.getString("publicKey");
    const auto idKey = browserRequest.getString("idKey");

    if (publicKey.isEmpty() || idKey.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    if (publicKey.compare(m_clientPublicKey) == 0) {
        const auto id = browserService()->storeKey(idKey);
        if (id.isEmpty()) {
            return buildErrorResponse(browserRequest, ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED);
        }

        const Parameters params{{"hash", browserRequest.hash}, {"id", id}};
        return buildResponse(browserRequest, params);
    }

    return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
}

QJsonObject BrowserAction::handleChangePublicKeys(const QJsonObject& message)
{
    const auto action = BROWSER_REQUEST_CHANGE_PUBLIC_KEYS;
    const auto nonce = message.value("nonce").toString();
    const auto clientPublicKey = message.value("publicKey").toString();
    const auto requestID = message.value("requestID").toString();

    if (clientPublicKey.isEmpty() || nonce.isEmpty() || requestID.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED);
    }

    auto keyPair = browserMessageBuilder()->getKeyPair();
    if (keyPair.first.isEmpty() || keyPair.second.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_ENCRYPTION_KEY_UNRECOGNIZED);
    }

    m_clientPublicKey = clientPublicKey;
    m_publicKey = keyPair.first;
    m_secretKey = keyPair.second;

    QJsonObject response;
    response["action"] = action;
    response["nonce"] = browserMessageBuilder()->incrementNonce(nonce);
    response["protocolVersion"] = 2;
    response["publicKey"] = keyPair.first;
    response["requestID"] = requestID;
    response["version"] = KEEPASSXC_VERSION;

    return response;
}

QJsonObject BrowserAction::handleCreateCredentials(const BrowserRequest& browserRequest)
{
    if (!isDatabaseConnected(browserRequest)) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto url = browserRequest.getString("url");
    if (url.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    const auto login = browserRequest.getString("login");
    const auto password = browserRequest.getString("password");
    const auto submitUrl = browserRequest.getString("submitUrl");
    const auto uuid = browserRequest.getString("uuid");
    const auto group = browserRequest.getString("group");
    const auto groupUuid = browserRequest.getString("groupUuid");
    const auto downloadFavicon = browserRequest.getBool("downloadFavicon");
    const QString realm;

    EntryParameters entryParameters;
    entryParameters.login = login;
    entryParameters.password = password;
    entryParameters.siteUrl = url;
    entryParameters.formUrl = submitUrl;
    entryParameters.realm = realm;

    bool result = true;
    if (uuid.isEmpty()) {
        browserService()->addEntry(entryParameters, group, groupUuid, downloadFavicon);
    } else {
        if (!Tools::isValidUuid(uuid)) {
            return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
        }

        result = browserService()->updateEntry(entryParameters, uuid);
    }

    const Parameters params{{"result", result}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleCreateNewGroup(const BrowserRequest& browserRequest)
{
    if (!isDatabaseConnected(browserRequest)) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto group = browserRequest.getString("groupName");
    const auto newGroup = browserService()->createNewGroup(group);
    if (newGroup.isEmpty() || newGroup["name"].toString().isEmpty() || newGroup["uuid"].toString().isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_CANNOT_CREATE_NEW_GROUP);
    }

    const Parameters params{{"name", newGroup["name"]}, {"uuid", newGroup["uuid"]}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleDeleteEntry(const BrowserRequest& browserRequest)
{
    if (!isDatabaseConnected(browserRequest)) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto uuid = browserRequest.getString("uuid");
    if (!Tools::isValidUuid(uuid)) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
    }

    const auto result = browserService()->deleteEntry(uuid);

    const Parameters params{{"result", result}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleGeneratePassword(const BrowserRequest& browserRequest, QLocalSocket* socket)
{
    // Do not allow multiple requests from the same client
    if (browserService()->isPasswordGeneratorRequested()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED);
    }

    KeyPairMessage keyPairMessage{
        socket, browserRequest.incrementedNonce, browserRequest.requestId, m_clientPublicKey, m_secretKey};

    browserService()->showPasswordGenerator(keyPairMessage);
    return {};
}

QJsonObject BrowserAction::handleGetCredentials(const BrowserRequest& browserRequest)
{
    const auto siteUrl = browserRequest.getString("url");
    if (siteUrl.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    const auto keyList = getConnectionKeys(browserRequest);
    if (keyList.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_SAVED_DATABASES_FOUND);
    }

    const auto formUrl = browserRequest.getString("submitUrl");
    const bool httpAuth = browserRequest.getBool("httpAuth");

    EntryParameters entryParameters;
    entryParameters.hash = browserRequest.hash;
    entryParameters.siteUrl = siteUrl;
    entryParameters.formUrl = formUrl;
    entryParameters.httpAuth = httpAuth;

    // TODO: Add autoLockRequested boolean
    bool entriesFound = false;
    const auto entries = browserService()->findEntries(entryParameters, keyList, &entriesFound);
    if (!entriesFound) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_LOGINS_FOUND);
    }

    const Parameters params{{"entries", entries}, {"hash", browserRequest.hash}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleGetDatabaseEntries(const BrowserRequest& browserRequest)
{
    if (!isDatabaseConnected(browserRequest)) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    if (!browserSettings()->allowGetDatabaseEntriesRequest()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ACCESS_TO_ALL_ENTRIES_DENIED);
    }

    const auto entries = browserService()->getDatabaseEntries();
    if (entries.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_GROUPS_FOUND);
    }

    const Parameters params{{"entries", entries}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleGetDatabaseGroups(const BrowserRequest& browserRequest)
{
    if (!isDatabaseConnected(browserRequest)) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto groups = browserService()->getDatabaseGroups();
    if (groups.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_GROUPS_FOUND);
    }

    const Parameters params{{"groups", groups}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleGetDatabaseStatuses(const BrowserRequest& browserRequest)
{
    const auto keyList = getConnectionKeys(browserRequest);
    const auto databaseStatuses = browserService()->getDatabaseStatuses(keyList);

    const Parameters params{{"hash", browserRequest.hash}, {"statuses", databaseStatuses}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleGetTotp(const BrowserRequest& browserRequest)
{
    const auto uuids = browserRequest.getArray("uuids");
    if (uuids.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
    }

    QStringList uuidList;
    for (const auto& u : uuids) {
        const auto uuid = u.toString();
        if (!Tools::isValidUuid(uuid)) {
            return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
        }

        uuidList << uuid;
    }

    const Parameters params{{"totpList", browserService()->getTotp(getConnectionKeys(browserRequest), uuidList)}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleGlobalAutoType(const BrowserRequest& browserRequest)
{
    const auto topLevelDomain = browserRequest.getString("search");
    if (topLevelDomain.length() > BrowserAction::MaxUrlLength) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    browserService()->requestGlobalAutoType(topLevelDomain);
    const Parameters params{{"result", true}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::handleLockDatabase(const BrowserRequest& browserRequest)
{
    if (browserRequest.hash.isEmpty()) {
        return buildErrorResponse(browserRequest, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
    }

    browserService()->lockDatabase();
    return buildResponse(browserRequest);
}

QJsonObject BrowserAction::decryptMessage(const QString& message, const QString& nonce) const
{
    return browserMessageBuilder()->decryptMessage(message, nonce, m_clientPublicKey, m_secretKey);
}

QJsonObject BrowserAction::getErrorReply(const QString& action, const int errorCode) const
{
    return browserMessageBuilder()->getErrorReply(action, errorCode);
}

QJsonObject BrowserAction::buildErrorResponse(const BrowserRequest& browserRequest, const int errorCode) const
{
    const Parameters params{{"errorCode", errorCode}, {"error", browserMessageBuilder()->getErrorMessage(errorCode)}};
    return buildResponse(browserRequest, params);
}

QJsonObject BrowserAction::buildResponse(const BrowserRequest& browserRequest, const Parameters& params) const
{
    return browserMessageBuilder()->buildResponse(browserRequest.action,
                                                  browserRequest.incrementedNonce,
                                                  browserRequest.requestId,
                                                  params,
                                                  m_clientPublicKey,
                                                  m_secretKey);
}

BrowserRequest BrowserAction::decodeRequest(const QJsonObject& message) const
{
    const auto nonce = message.value("nonce").toString();
    const auto encrypted = message.value("message").toString();
    const auto decryptedMessage = decryptMessage(encrypted, nonce);
    const auto action = decryptedMessage.value("action").toString();
    const auto requestId = message.value("requestID").toString();

    return {action,
            requestId,
            browserService()->getDatabaseHash(),
            nonce,
            browserMessageBuilder()->incrementNonce(nonce),
            decryptedMessage};
}

StringPairList BrowserAction::getConnectionKeys(const BrowserRequest& browserRequest) const
{
    const auto keys = browserRequest.getArray("keys");

    StringPairList keyList;
    for (const auto val : keys) {
        const auto keyObject = val.toObject();
        keyList.push_back(qMakePair(keyObject.value("id").toString(), keyObject.value("key").toString()));
    }

    return keyList;
}

bool BrowserAction::isDatabaseConnected(const BrowserRequest& browserRequest) const
{
    const auto keyList = getConnectionKeys(browserRequest);
    return browserService()->isDatabaseConnected(keyList, browserRequest.getString("hash"));
}
