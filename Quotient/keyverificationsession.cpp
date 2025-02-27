// SPDX-FileCopyrightText: 2022 Tobias Fella <fella@posteo.de>
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "keyverificationsession.h"

#include "connection.h"
#include "database.h"
#include "logging_categories_p.h"
#include "csapi/cross_signing.h"
#include "room.h"

#include "e2ee/cryptoutils.h"
#include "e2ee/sssshandler.h"
#include "e2ee/qolmaccount.h"

#include "events/event.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QTimer>
#include <QtCore/QUuid>

#include <olm/sas.h>

#include <chrono>

using namespace Quotient;
using namespace std::chrono;

const QStringList supportedMethods = { SasV1Method };

QStringList commonSupportedMethods(const QStringList& remoteMethods)
{
    QStringList result;
    for (const auto& method : remoteMethods) {
        if (supportedMethods.contains(method)) {
            result += method;
        }
    }
    return result;
}

CStructPtr<OlmSAS> KeyVerificationSession::makeOlmData()
{
    auto data = makeCStruct(olm_sas, olm_sas_size, olm_clear_sas);

    const auto randomLength = olm_create_sas_random_length(data.get());
    olm_create_sas(data.get(), getRandom(randomLength).data(), randomLength);
    return data;
}

KeyVerificationSession::KeyVerificationSession(QString remoteUserId,
                                               const KeyVerificationRequestEvent& event,
                                               Connection* connection, bool encrypted)
    : KeyVerificationSession(std::move(remoteUserId), connection, event.fromDevice(), encrypted,
                             event.methods(), event.timestamp(), event.transactionId())
{}

KeyVerificationSession::KeyVerificationSession(const RoomMessageEvent* event, Room* room)
    : KeyVerificationSession(event->senderId(), room->connection(),
                             event->contentPart<QString>("from_device"_ls), room->usesEncryption(),
                             event->contentPart<QStringList>("methods"_ls),
                             event->originTimestamp(), {}, room, event->id())
{}

KeyVerificationSession::KeyVerificationSession(QString remoteUserId, Connection* connection,
                                               QString remoteDeviceId, bool encrypted,
                                               QStringList methods, QDateTime startTimestamp,
                                               QString transactionId, Room* room,
                                               QString requestEventId)
    : QObject(connection)
    , m_connection(connection)
    , m_room(room)
    , m_remoteUserId(std::move(remoteUserId))
    , m_remoteDeviceId(std::move(remoteDeviceId))
    , m_transactionId(std::move(transactionId))
    , m_encrypted(encrypted)
    , m_remoteSupportedMethods(std::move(methods))
    , m_requestEventId(std::move(requestEventId)) // TODO: Consider merging with transactionId
{
    if (m_connection->hasConflictingDeviceIdsAndCrossSigningKeys(m_remoteUserId)) {
        qCWarning(E2EE) << "Remote user has conflicting device ids and cross signing keys; refusing to verify.";
        return;
    }
    const auto& currentTime = QDateTime::currentDateTime();
    const auto timeoutTime =
        std::min(startTimestamp.addSecs(600), currentTime.addSecs(120));
    const milliseconds timeout{ currentTime.msecsTo(timeoutTime) };
    if (timeout > 5s) {
        setupTimeout(timeout);
    } else {
        // Otherwise don't even bother starting up
        deleteLater();
    }
}

KeyVerificationSession::KeyVerificationSession(QString userId, QString deviceId,
                                               Connection* connection)
    : KeyVerificationSession(std::move(userId), connection, nullptr, std::move(deviceId),
                             QUuid::createUuid().toString())
{}

KeyVerificationSession::KeyVerificationSession(Room* room)
    : KeyVerificationSession(room->members()[room->members()[0].isLocalMember() ? 1 : 0].id(),
                             room->connection(),
                             room)
{}

KeyVerificationSession::KeyVerificationSession(QString remoteUserId, Connection* connection,
                                               Room* room, QString remoteDeviceId,
                                               QString transactionId)
    : QObject(connection)
    , m_connection(connection)
    , m_room(room)
    , m_remoteUserId(std::move(remoteUserId))
    , m_remoteDeviceId(std::move(remoteDeviceId))
    , m_transactionId(std::move(transactionId))
{
    if (m_connection->hasConflictingDeviceIdsAndCrossSigningKeys(m_remoteUserId)) {
        qCWarning(E2EE) << "Remote user has conflicting device ids and cross signing keys; refusing to verify.";
        return;
    }
    setupTimeout(600s);
    QMetaObject::invokeMethod(this, &KeyVerificationSession::sendRequest);
}

void KeyVerificationSession::setupTimeout(milliseconds timeout)
{
    QTimer::singleShot(timeout, this, [this] { cancelVerification(TIMEOUT); });
}

void KeyVerificationSession::handleEvent(const KeyVerificationEvent& baseEvent)
{
    if (!switchOnType(
            baseEvent,
            [this](const KeyVerificationCancelEvent& event) {
                setError(stringToError(event.code()));
                setState(CANCELED);
                return true;
            },
            [this](const KeyVerificationStartEvent& event) {
                if (state() != WAITINGFORREADY && state() != READY && state() != WAITINGFORACCEPT)
                    return false;
                handleStart(event);
                return true;
            },
            [this](const KeyVerificationReadyEvent& event) {
                if (state() == WAITINGFORREADY)
                    handleReady(event);
                // ACCEPTED is also fine here because it's possible to receive
                // ready and start in the same sync, in which case start might
                // be handled before ready.
                return state() == READY || state() == WAITINGFORACCEPT || state() == ACCEPTED;
            },
            [this](const KeyVerificationAcceptEvent& event) {
                if (state() != WAITINGFORACCEPT)
                    return false;
                const auto& theirMac = event.messageAuthenticationCode();
                for (const auto& mac : SupportedMacs) {
                    if (mac == theirMac) {
                        m_commonMacCodes.push_back(theirMac);
                    }
                }
                if (m_commonMacCodes.isEmpty()) {
                    cancelVerification(UNKNOWN_METHOD);
                    return false;
                }
                m_commitment = event.commitment();
                sendKey();
                setState(WAITINGFORKEY);
                return true;
            },
            [this](const KeyVerificationKeyEvent& event) {
                if (state() != ACCEPTED && state() != WAITINGFORKEY)
                    return false;
                handleKey(event);
                return true;
            },
            [this](const KeyVerificationMacEvent& event) {
                if (state() != WAITINGFORMAC && state() != WAITINGFORVERIFICATION)
                    return false;
                handleMac(event);
                return true;
            },
            [this](const KeyVerificationDoneEvent&) { return state() == DONE; }))
        cancelVerification(UNEXPECTED_MESSAGE);
}

struct EmojiStoreEntry : EmojiEntry {
    QHash<QString, QString> translatedDescriptions;

    explicit EmojiStoreEntry(const QJsonObject& json)
        : EmojiEntry{ fromJson<QString>(json["emoji"_ls]),
                      fromJson<QString>(json["description"_ls]) }
        , translatedDescriptions{ fromJson<QHash<QString, QString>>(
              json["translated_descriptions"_ls]) }
    {}
};

using EmojiStore = QVector<EmojiStoreEntry>;

EmojiStore loadEmojiStore()
{
    Q_INIT_RESOURCE(libquotientemojis);
    QFile dataFile(":/sas-emoji.json"_ls);
    dataFile.open(QFile::ReadOnly);
    auto data = dataFile.readAll();
    Q_CLEANUP_RESOURCE(libquotientemojis);
    return fromJson<EmojiStore>(
        QJsonDocument::fromJson(data).array());
}

EmojiEntry emojiForCode(int code, const QString& language)
{
    static const EmojiStore emojiStore = loadEmojiStore();
    const auto& entry = emojiStore[code];
    if (!language.isEmpty())
        if (const auto translatedDescription =
            emojiStore[code].translatedDescriptions.value(language);
            !translatedDescription.isNull())
            return { entry.emoji, translatedDescription };

    return SLICE(entry, EmojiEntry);
}

void KeyVerificationSession::handleKey(const KeyVerificationKeyEvent& event)
{
    auto eventKey = event.key().toLatin1();
    olm_sas_set_their_key(olmData, eventKey.data(), unsignedSize(eventKey));

    if (startSentByUs) {
        const auto paddedCommitment =
            QCryptographicHash::hash((event.key() % m_startEvent).toLatin1(),
                                     QCryptographicHash::Sha256)
                .toBase64();
        const QLatin1String unpaddedCommitment(paddedCommitment.constData(),
                                               QString::fromLatin1(paddedCommitment).indexOf(u'='));
        if (unpaddedCommitment != m_commitment) {
            qCWarning(E2EE) << "Commitment mismatch; aborting verification";
            cancelVerification(MISMATCHED_COMMITMENT);
            return;
        }
    } else {
        sendKey();
    }

    std::string key(olm_sas_pubkey_length(olmData), '\0');
    olm_sas_get_pubkey(olmData, key.data(), key.size());

    std::array<std::byte, 6> output{};
    const auto infoTemplate =
        startSentByUs ? "MATRIX_KEY_VERIFICATION_SAS|%1|%2|%3|%4|%5|%6|%7"_ls
                      : "MATRIX_KEY_VERIFICATION_SAS|%4|%5|%6|%1|%2|%3|%7"_ls;

    const auto info = infoTemplate
                          .arg(m_connection->userId(), m_connection->deviceId(),
                               QString::fromLatin1(key.data()), m_remoteUserId, m_remoteDeviceId,
                               event.key(), m_room ? m_requestEventId : m_transactionId)
                          .toLatin1();

    olm_sas_generate_bytes(olmData, info.data(), unsignedSize(info),
                           output.data(), output.size());

    static constexpr auto x3f = std::byte{ 0x3f };
    const std::array<std::byte, 7> code{
        output[0] >> 2,
        (output[0] << 4 & x3f) | output[1] >> 4,
        (output[1] << 2 & x3f) | output[2] >> 6,
        output[2] & x3f,
        output[3] >> 2,
        (output[3] << 4 & x3f) | output[4] >> 4,
        (output[4] << 2 & x3f) | output[5] >> 6
    };

    const auto uiLanguages = QLocale().uiLanguages();
    const auto preferredLanguage = uiLanguages.isEmpty()
                                       ? QString()
                                       : uiLanguages.front().section(u'-', 0, 0);
    for (const auto& c : code)
        m_sasEmojis += emojiForCode(std::to_integer<int>(c), preferredLanguage);

    emit sasEmojisChanged();
    emit keyReceived();
    setState(WAITINGFORVERIFICATION);
}

QString KeyVerificationSession::calculateMac(const QString& input,
                                             bool verifying,
                                             const QString& keyId)
{
    const auto inputBytes = input.toLatin1();
    const auto macLength = olm_sas_mac_length(olmData);
    auto macChars = byteArrayForOlm(macLength);
    const auto macInfo =
        (verifying ? "MATRIX_KEY_VERIFICATION_MAC%3%4%1%2%5%6"_ls
                   : "MATRIX_KEY_VERIFICATION_MAC%1%2%3%4%5%6"_ls)
            .arg(m_connection->userId(), m_connection->deviceId(),
                 m_remoteUserId, m_remoteDeviceId, m_room ? m_requestEventId : m_transactionId, input.contains(u',') ? QStringLiteral("KEY_IDS") : keyId)
            .toLatin1();
    if (m_commonMacCodes.contains(HmacSha256V2Code))
        olm_sas_calculate_mac_fixed_base64(olmData, inputBytes.data(),
                                           unsignedSize(inputBytes),
                                           macInfo.data(), unsignedSize(macInfo),
                                           macChars.data(), macLength);
    else
        olm_sas_calculate_mac(olmData, inputBytes.data(),
                              unsignedSize(inputBytes), macInfo.data(),
                              unsignedSize(macInfo), macChars.data(), macLength);
    return QString::fromLatin1(macChars.data(), macChars.indexOf('='));
}

void KeyVerificationSession::sendMac()
{
    QString keyId{ "ed25519:"_ls % m_connection->deviceId() };

    const auto &masterKey = m_connection->isUserVerified(m_connection->userId()) ? m_connection->masterKeyForUser(m_connection->userId()) : QString();

    QStringList keyList{keyId};
    if (!masterKey.isEmpty()) {
        keyList += "ed25519:"_ls + masterKey;
    }
    keyList.sort();

    auto keys = calculateMac(keyList.join(u','), false);

    QJsonObject mac;
    auto key = m_connection->olmAccount()->deviceKeys().keys.value(keyId);
    mac[keyId] = calculateMac(key, false, keyId);
    if (!masterKey.isEmpty()) {
        mac["ed25519:"_ls + masterKey] = calculateMac(masterKey, false, "ed25519:"_ls + masterKey);
    }

    sendEvent(m_remoteUserId, m_remoteDeviceId,
                               KeyVerificationMacEvent(m_transactionId, keys,
                                                       mac),
                               m_encrypted);
    setState (macReceived ? DONE : WAITINGFORMAC);
    m_verified = true;
    if (!m_pendingEdKeyId.isEmpty()) {
        trustKeys();
    }
}

void KeyVerificationSession::sendDone()
{
    sendEvent(m_remoteUserId, m_remoteDeviceId,
                               KeyVerificationDoneEvent(m_transactionId),
                               m_encrypted);
}

void KeyVerificationSession::sendKey()
{
    const auto pubkeyLength = olm_sas_pubkey_length(olmData);
    auto keyBytes = byteArrayForOlm(pubkeyLength);
    olm_sas_get_pubkey(olmData, keyBytes.data(), pubkeyLength);
    sendEvent(m_remoteUserId, m_remoteDeviceId,
                               KeyVerificationKeyEvent(m_transactionId,
                                                       QString::fromLatin1(keyBytes)),
                               m_encrypted);
}


void KeyVerificationSession::cancelVerification(Error error)
{
    sendEvent(m_remoteUserId, m_remoteDeviceId, KeyVerificationCancelEvent(m_transactionId,
                                                          errorToString(error)), m_encrypted);
    setState(CANCELED);
    setError(error);
    emit finished();
    deleteLater();
}

void KeyVerificationSession::sendReady()
{
    auto methods = commonSupportedMethods(m_remoteSupportedMethods);

    if (methods.isEmpty()) {
        cancelVerification(UNKNOWN_METHOD);
        return;
    }

    sendEvent(
        m_remoteUserId, m_remoteDeviceId,
        KeyVerificationReadyEvent(m_transactionId, m_connection->deviceId(),
                                  methods),
        m_encrypted);
    setState(READY);

    if (methods.size() == 1) {
        sendStartSas();
    }
}

void KeyVerificationSession::sendStartSas()
{
    startSentByUs = true;
    KeyVerificationStartEvent event(m_transactionId, m_connection->deviceId());
    auto fixedJson = event.contentJson();
    if (m_room) {
        fixedJson.remove("transaction_id"_ls);
        fixedJson["m.relates_to"_ls] = QJsonObject {
            {"event_id"_ls, m_requestEventId},
            {"rel_type"_ls, "m.reference"_ls},
        };
    }
    m_startEvent = QString::fromUtf8(
        QJsonDocument(fixedJson).toJson(QJsonDocument::Compact));
    sendEvent(m_remoteUserId, m_remoteDeviceId, event,
                               m_encrypted);
    setState(WAITINGFORACCEPT);
}

void KeyVerificationSession::handleReady(const KeyVerificationReadyEvent& event)
{
    setState(READY);
    m_remoteSupportedMethods = event.methods();
    auto methods = commonSupportedMethods(m_remoteSupportedMethods);

    // This happens for outgoing user verification
    if (m_remoteDeviceId.isEmpty()) {
        m_remoteDeviceId = event.fromDevice();
    }

    if (methods.isEmpty())
        cancelVerification(UNKNOWN_METHOD);
    else if (methods.size() == 1)
        sendStartSas(); // -> WAITINGFORACCEPT
}

void KeyVerificationSession::handleStart(const KeyVerificationStartEvent& event)
{
    if (startSentByUs) {
        if (m_remoteUserId > m_connection->userId()
            || (m_remoteUserId == m_connection->userId()
                && m_remoteDeviceId > m_connection->deviceId())) {
            return;
        }
        startSentByUs = false;
    }
    const auto& theirMacs = event.messageAuthenticationCodes();
    for (const auto& macCode : SupportedMacs)
        if (theirMacs.contains(macCode))
            m_commonMacCodes.push_back(macCode);
    if (m_commonMacCodes.isEmpty()) {
        cancelVerification(UNKNOWN_METHOD);
        return;
    }
    const auto pubkeyLength = olm_sas_pubkey_length(olmData);
    auto publicKey = byteArrayForOlm(pubkeyLength);
    olm_sas_get_pubkey(olmData, publicKey.data(), pubkeyLength);
    const auto canonicalEvent = QJsonDocument(event.contentJson()).toJson(QJsonDocument::Compact);
    const auto commitment =
        QString::fromLatin1(QCryptographicHash::hash(publicKey + canonicalEvent,
                                                     QCryptographicHash::Sha256)
                                .toBase64(QByteArray::OmitTrailingEquals));

    sendEvent(m_remoteUserId, m_remoteDeviceId,
                               KeyVerificationAcceptEvent(m_transactionId,
                                                          commitment),
                               m_encrypted);
    setState(ACCEPTED);
}

void KeyVerificationSession::handleMac(const KeyVerificationMacEvent& event)
{
    QStringList keys = event.mac().keys();
    keys.sort();
    const auto& key = keys.join(","_ls);
    const QString edKeyId = "ed25519:"_ls % m_remoteDeviceId;

    if (calculateMac(m_connection->edKeyForUserDevice(m_remoteUserId,
                                                      m_remoteDeviceId),
                     true, edKeyId)
        != event.mac().value(edKeyId)) {
        cancelVerification(KEY_MISMATCH);
        return;
    }

    auto masterKey = m_connection->masterKeyForUser(m_remoteUserId);
    if (event.mac().contains("ed25519:"_ls % masterKey)
        && calculateMac(masterKey, true, "ed25519:"_ls % masterKey)
               != event.mac().value("ed25519:"_ls % masterKey)) {
        cancelVerification(KEY_MISMATCH);
        return;
    }

    if (calculateMac(key, true) != event.keys()) {
        cancelVerification(KEY_MISMATCH);
        return;
    }

    m_pendingEdKeyId = edKeyId;
    m_pendingMasterKey = masterKey;

    if (m_verified) {
        trustKeys();
    }
}

void KeyVerificationSession::trustKeys()
{
    m_connection->database()->setSessionVerified(m_pendingEdKeyId);
    m_connection->database()->setMasterKeyVerified(m_pendingMasterKey);
    if (m_remoteUserId == m_connection->userId()) {
        m_connection->reloadDevices();
    }

    if (m_pendingMasterKey.length() > 0) {
        if (m_remoteUserId == m_connection->userId()) {
            const auto selfSigningKey = m_connection->database()->loadEncrypted("m.cross_signing.self_signing"_ls);
            if (!selfSigningKey.isEmpty()) {
                QHash<QString, QHash<QString, QJsonObject>> signatures;
                auto json = QJsonObject {
                    {"keys"_ls, QJsonObject {
                        {"ed25519:"_ls + m_remoteDeviceId, m_connection->edKeyForUserDevice(m_remoteUserId, m_remoteDeviceId)},
                        {"curve25519:"_ls + m_remoteDeviceId, m_connection->curveKeyForUserDevice(m_remoteUserId, m_remoteDeviceId)},
                    }},
                    {"algorithms"_ls, QJsonArray {"m.olm.v1.curve25519-aes-sha2"_ls, "m.megolm.v1.aes-sha2"_ls}},
                    {"device_id"_ls, m_remoteDeviceId},
                    {"user_id"_ls, m_remoteUserId},
                };
                auto signature = sign(selfSigningKey, QJsonDocument(json).toJson(QJsonDocument::Compact));
                json["signatures"_ls] = QJsonObject {
                    {m_connection->userId(), QJsonObject {
                        {"ed25519:"_ls + m_connection->database()->selfSigningPublicKey(), QString::fromLatin1(signature)},
                    }},
                };
                signatures[m_remoteUserId][m_remoteDeviceId] = json;
                auto uploadSignatureJob = m_connection->callApi<UploadCrossSigningSignaturesJob>(signatures);
                connect(uploadSignatureJob, &BaseJob::finished, m_connection, [uploadSignatureJob]() {
                    if (uploadSignatureJob->error() != BaseJob::Success) {
                        qCWarning(E2EE) << "Failed to upload self-signing signature" << uploadSignatureJob << uploadSignatureJob->error() << uploadSignatureJob->errorString();
                    }
                });
            } else {
                // Not parenting to this since the session is going to be destroyed soon
                auto handler = new SSSSHandler(m_connection);
                handler->setConnection(m_connection);
                handler->unlockSSSSFromCrossSigning();
                connect(handler, &SSSSHandler::finished, handler, &QObject::deleteLater);
            }
        } else {
            const auto userSigningKey = m_connection->database()->loadEncrypted("m.cross_signing.user_signing"_ls);
            if (!userSigningKey.isEmpty()) {
                QHash<QString, QHash<QString, QJsonObject>> signatures;
                auto json = QJsonObject {
                    {"keys"_ls, QJsonObject {
                        {"ed25519:"_ls + m_pendingMasterKey, m_pendingMasterKey},
                    }},
                    {"usage"_ls, QJsonArray {"master"_ls}},
                    {"user_id"_ls, m_remoteUserId},
                };
                auto signature = sign(userSigningKey, QJsonDocument(json).toJson(QJsonDocument::Compact));
                json["signatures"_ls] = QJsonObject {
                    {m_connection->userId(), QJsonObject {
                        {"ed25519:"_ls + m_connection->database()->userSigningPublicKey(), QString::fromLatin1(signature)},
                    }},
                };
                signatures[m_remoteUserId][m_pendingMasterKey] = json;
                auto uploadSignatureJob = m_connection->callApi<UploadCrossSigningSignaturesJob>(signatures);
                connect(uploadSignatureJob, &BaseJob::finished, m_connection, [uploadSignatureJob, userId = m_remoteUserId](){
                    if (uploadSignatureJob->error() != BaseJob::Success) {
                        qCWarning(E2EE) << "Failed to upload user-signing signature for" << userId << uploadSignatureJob << uploadSignatureJob->error() << uploadSignatureJob->errorString();
                    }
                });
            }
        }
        emit m_connection->userVerified(m_remoteUserId);
    }

    emit m_connection->sessionVerified(m_remoteUserId, m_remoteDeviceId);
    macReceived = true;

    if (state() == WAITINGFORMAC) {
        setState(DONE);
        sendDone();
        emit finished();
        deleteLater();
    }
}

QVector<EmojiEntry> KeyVerificationSession::sasEmojis() const
{
    return m_sasEmojis;
}

void KeyVerificationSession::sendRequest()
{
    sendEvent(
        m_remoteUserId, m_remoteDeviceId,
        KeyVerificationRequestEvent(m_transactionId, m_connection->deviceId(),
                                    supportedMethods,
                                    QDateTime::currentDateTime()),
        m_encrypted);
    setState(WAITINGFORREADY);
}

KeyVerificationSession::State KeyVerificationSession::state() const
{
    return m_state;
}

void KeyVerificationSession::setState(KeyVerificationSession::State state)
{
    qCDebug(E2EE) << "KeyVerificationSession state" << m_state << "->" << state;
    m_state = state;
    emit stateChanged();
}

KeyVerificationSession::Error KeyVerificationSession::error() const
{
    return m_error;
}

void KeyVerificationSession::setError(Error error)
{
    m_error = error;
    emit errorChanged();
}

QString KeyVerificationSession::errorToString(Error error)
{
    switch(error) {
        case NONE:
            return "none"_ls;
        case TIMEOUT:
            return "m.timeout"_ls;
        case USER:
            return "m.user"_ls;
        case UNEXPECTED_MESSAGE:
            return "m.unexpected_message"_ls;
        case UNKNOWN_TRANSACTION:
            return "m.unknown_transaction"_ls;
        case UNKNOWN_METHOD:
            return "m.unknown_method"_ls;
        case KEY_MISMATCH:
            return "m.key_mismatch"_ls;
        case USER_MISMATCH:
            return "m.user_mismatch"_ls;
        case INVALID_MESSAGE:
            return "m.invalid_message"_ls;
        case SESSION_ACCEPTED:
            return "m.accepted"_ls;
        case MISMATCHED_COMMITMENT:
            return "m.mismatched_commitment"_ls;
        case MISMATCHED_SAS:
            return "m.mismatched_sas"_ls;
        default:
            return "m.user"_ls;
    }
}

KeyVerificationSession::Error KeyVerificationSession::stringToError(const QString& error)
{
    if (error == "m.timeout"_ls)
        return REMOTE_TIMEOUT;
    if (error == "m.user"_ls)
        return REMOTE_USER;
    if (error == "m.unexpected_message"_ls)
        return REMOTE_UNEXPECTED_MESSAGE;
    if (error == "m.unknown_message"_ls)
        return REMOTE_UNEXPECTED_MESSAGE;
    if (error == "m.unknown_transaction"_ls)
        return REMOTE_UNKNOWN_TRANSACTION;
    if (error == "m.unknown_method"_ls)
        return REMOTE_UNKNOWN_METHOD;
    if (error == "m.key_mismatch"_ls)
        return REMOTE_KEY_MISMATCH;
    if (error == "m.user_mismatch"_ls)
        return REMOTE_USER_MISMATCH;
    if (error == "m.invalid_message"_ls)
        return REMOTE_INVALID_MESSAGE;
    if (error == "m.accepted"_ls)
        return REMOTE_SESSION_ACCEPTED;
    if (error == "m.mismatched_commitment"_ls)
        return REMOTE_MISMATCHED_COMMITMENT;
    if (error == "m.mismatched_sas"_ls)
        return REMOTE_MISMATCHED_SAS;
    return NONE;
}

QString KeyVerificationSession::remoteDeviceId() const
{
    return m_remoteDeviceId;
}

QString KeyVerificationSession::transactionId() const
{
    return m_transactionId;
}

void KeyVerificationSession::sendEvent(const QString &userId, const QString &deviceId, const KeyVerificationEvent &event, bool encrypted)
{
    if (m_room) {
        auto json = event.contentJson();
        json.remove("transaction_id"_ls);
        if (event.metaType().matrixId == KeyVerificationRequestEvent::TypeId) {
            json["msgtype"_ls] = event.matrixType();
            json["body"_ls] = QStringLiteral("%1 sent a verification request").arg(m_connection->userId());
            json["to"_ls] = m_remoteUserId;
            m_room->postJson("m.room.message"_ls, json);
        } else {
            json["m.relates_to"_ls] = QJsonObject {
                {"event_id"_ls, m_requestEventId},
                {"rel_type"_ls, "m.reference"_ls}
            };
            m_room->postJson(event.matrixType(), json);
        }
    } else {
        m_connection->sendToDevice(userId, deviceId, event, encrypted);
    }
}

bool KeyVerificationSession::userVerification() const
{
    return m_room;
}

void KeyVerificationSession::setRequestEventId(const QString& eventId)
{
    m_requestEventId = eventId;
}
