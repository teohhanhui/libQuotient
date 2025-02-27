// SPDX-FileCopyrightText: 2018 Kitsune Ral <kitsune-ral@users.sf.net>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <Quotient/util.h>

class QJsonObject;
class QJsonArray;
class QJsonDocument;
class QIODevice;

namespace Quotient {
/**
 * A simple wrapper that represents the request body.
 * Provides a unified interface to dump an unstructured byte stream
 * as well as JSON (and possibly other structures in the future) to
 * a QByteArray consumed by QNetworkAccessManager request methods.
 */
class QUOTIENT_API RequestData {
public:
    Q_IMPLICIT RequestData(const QByteArray& a = {});
    Q_IMPLICIT RequestData(const QJsonObject& jo);
    Q_IMPLICIT RequestData(const QJsonArray& ja);
    Q_IMPLICIT RequestData(QIODevice* source);

    QIODevice* source() const { return _source.get(); }

private:
    ImplPtr<QIODevice> _source;
};
} // namespace Quotient
