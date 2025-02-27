// THIS FILE IS GENERATED - ANY EDITS WILL BE OVERWRITTEN

#include "logout.h"

using namespace Quotient;

QUrl LogoutJob::makeRequestUrl(const HomeserverData& hsData)
{
    return BaseJob::makeRequestUrl(hsData, makePath("/_matrix/client/v3", "/logout"));
}

LogoutJob::LogoutJob()
    : BaseJob(HttpVerb::Post, QStringLiteral("LogoutJob"), makePath("/_matrix/client/v3", "/logout"))
{}

QUrl LogoutAllJob::makeRequestUrl(const HomeserverData& hsData)
{
    return BaseJob::makeRequestUrl(hsData, makePath("/_matrix/client/v3", "/logout/all"));
}

LogoutAllJob::LogoutAllJob()
    : BaseJob(HttpVerb::Post, QStringLiteral("LogoutAllJob"),
              makePath("/_matrix/client/v3", "/logout/all"))
{}
