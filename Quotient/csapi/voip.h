// THIS FILE IS GENERATED - ANY EDITS WILL BE OVERWRITTEN

#pragma once

#include <Quotient/jobs/basejob.h>

namespace Quotient {

//! \brief Obtain TURN server credentials.
//!
//! This API provides credentials for the client to use when initiating
//! calls.
class QUOTIENT_API GetTurnServerJob : public BaseJob {
public:
    explicit GetTurnServerJob();

    //! \brief Construct a URL without creating a full-fledged job object
    //!
    //! This function can be used when a URL for GetTurnServerJob
    //! is necessary but the job itself isn't.
    static QUrl makeRequestUrl(const HomeserverData& hsData);

    // Result properties

    //! The TURN server credentials.
    QJsonObject data() const { return fromJson<QJsonObject>(jsonData()); }
};

inline auto collectResponse(const GetTurnServerJob* job) { return job->data(); }

} // namespace Quotient
