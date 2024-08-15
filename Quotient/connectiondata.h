// SPDX-FileCopyrightText: 2015 Felix Rohrbach <kde@fxrh.de>
// SPDX-FileCopyrightText: 2016 Kitsune Ral <Kitsune-Ral@users.sf.net>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "util.h"

#include <QtCore/QUrl>

#include <chrono>

namespace Quotient {

class NetworkAccessManager;
class BaseJob;
class Connection;

class QUOTIENT_API ConnectionData {
public:
    explicit ConnectionData(QUrl baseUrl);
    Q_DISABLE_COPY_MOVE(ConnectionData)
    virtual ~ConnectionData();

    void submit(BaseJob* job);
    void limitRate(std::chrono::milliseconds nextCallAfter);

    QByteArray accessToken() const;
    QUrl baseUrl() const;
    const QString& deviceId() const;
    const QString& userId() const;
    HomeserverData homeserverData() const;
    Quotient::NetworkAccessManager *nam() const;

    void setBaseUrl(QUrl baseUrl, Connection* connection);
    void setToken(QByteArray accessToken);
    void setDeviceId(const QString& deviceId);
    void setUserId(const QString& userId, Connection* connection);
    void setSupportedSpecVersions(QStringList versions);

    QString lastEvent() const;
    void setLastEvent(QString identifier);

    QString generateTxnId() const;

private:
    class Private;
    ImplPtr<Private> d;
};
} // namespace Quotient
