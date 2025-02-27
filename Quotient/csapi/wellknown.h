// THIS FILE IS GENERATED - ANY EDITS WILL BE OVERWRITTEN

#pragma once

#include <Quotient/csapi/definitions/wellknown/full.h>

#include <Quotient/jobs/basejob.h>

namespace Quotient {

//! \brief Gets Matrix server discovery information about the domain.
//!
//! Gets discovery information about the domain. The file may include
//! additional keys, which MUST follow the Java package naming convention,
//! e.g. `com.example.myapp.property`. This ensures property names are
//! suitably namespaced for each application and reduces the risk of
//! clashes.
//!
//! Note that this endpoint is not necessarily handled by the homeserver,
//! but by another webserver, to be used for discovering the homeserver URL.
class QUOTIENT_API GetWellknownJob : public BaseJob {
public:
    explicit GetWellknownJob();

    //! \brief Construct a URL without creating a full-fledged job object
    //!
    //! This function can be used when a URL for GetWellknownJob
    //! is necessary but the job itself isn't.
    static QUrl makeRequestUrl(const HomeserverData& hsData);

    // Result properties

    //! Server discovery information.
    DiscoveryInformation data() const { return fromJson<DiscoveryInformation>(jsonData()); }
};

inline auto collectResponse(const GetWellknownJob* job) { return job->data(); }

} // namespace Quotient
