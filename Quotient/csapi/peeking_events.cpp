// THIS FILE IS GENERATED - ANY EDITS WILL BE OVERWRITTEN

#include "peeking_events.h"

using namespace Quotient;

auto queryToPeekEvents(const QString& from, std::optional<int> timeout, const QString& roomId)
{
    QUrlQuery _q;
    addParam<IfNotEmpty>(_q, QStringLiteral("from"), from);
    addParam<IfNotEmpty>(_q, QStringLiteral("timeout"), timeout);
    addParam<IfNotEmpty>(_q, QStringLiteral("room_id"), roomId);
    return _q;
}

QUrl PeekEventsJob::makeRequestUrl(const HomeserverData& hsData, const QString& from,
                                   std::optional<int> timeout, const QString& roomId)
{
    return BaseJob::makeRequestUrl(hsData, makePath("/_matrix/client/v3", "/events"),
                                   queryToPeekEvents(from, timeout, roomId));
}

PeekEventsJob::PeekEventsJob(const QString& from, std::optional<int> timeout, const QString& roomId)
    : BaseJob(HttpVerb::Get, QStringLiteral("PeekEventsJob"),
              makePath("/_matrix/client/v3", "/events"), queryToPeekEvents(from, timeout, roomId))
{}
