// THIS FILE IS GENERATED - ANY EDITS WILL BE OVERWRITTEN

#pragma once

#include <Quotient/jobs/basejob.h>

namespace Quotient {

//! \brief Searches the user directory.
//!
//! Performs a search for users. The homeserver may
//! determine which subset of users are searched, however the homeserver
//! MUST at a minimum consider the users the requesting user shares a
//! room with and those who reside in public rooms (known to the homeserver).
//! The search MUST consider local users to the homeserver, and SHOULD
//! query remote users as part of the search.
//!
//! The search is performed case-insensitively on user IDs and display
//! names preferably using a collation determined based upon the
//! `Accept-Language` header provided in the request, if present.
class QUOTIENT_API SearchUserDirectoryJob : public BaseJob {
public:
    // Inner data structures

    struct QUOTIENT_API User {
        //! The user's matrix user ID.
        QString userId;

        //! The display name of the user, if one exists.
        QString displayName{};

        //! The avatar url, as an [`mxc://` URI](/client-server-api/#matrix-content-mxc-uris), if
        //! one exists.
        QUrl avatarUrl{};
    };

    // Construction/destruction

    //! \param searchTerm
    //!   The term to search for
    //!
    //! \param limit
    //!   The maximum number of results to return. Defaults to 10.
    explicit SearchUserDirectoryJob(const QString& searchTerm,
                                    std::optional<int> limit = std::nullopt);

    // Result properties

    //! Ordered by rank and then whether or not profile info is available.
    QVector<User> results() const { return loadFromJson<QVector<User>>("results"_ls); }

    //! Indicates if the result list has been truncated by the limit.
    bool limited() const { return loadFromJson<bool>("limited"_ls); }

    struct Response {
        //! Ordered by rank and then whether or not profile info is available.
        QVector<User> results{};

        //! Indicates if the result list has been truncated by the limit.
        bool limited{};
    };
};

template <std::derived_from<SearchUserDirectoryJob> JobT>
constexpr inline auto doCollectResponse<JobT> =
    [](JobT* j) -> SearchUserDirectoryJob::Response { return { j->results(), j->limited() }; };

template <>
struct QUOTIENT_API JsonObjectConverter<SearchUserDirectoryJob::User> {
    static void fillFrom(const QJsonObject& jo, SearchUserDirectoryJob::User& result)
    {
        fillFromJson(jo.value("user_id"_ls), result.userId);
        fillFromJson(jo.value("display_name"_ls), result.displayName);
        fillFromJson(jo.value("avatar_url"_ls), result.avatarUrl);
    }
};

} // namespace Quotient
