// THIS FILE IS GENERATED - ANY EDITS WILL BE OVERWRITTEN

#pragma once

#include <Quotient/csapi/definitions/third_party_signed.h>

#include <Quotient/jobs/basejob.h>

namespace Quotient {

//! \brief Start the requesting user participating in a particular room.
//!
//! *Note that this API requires a room ID, not alias.*
//! `/join/{roomIdOrAlias}` *exists if you have a room alias.*
//!
//! This API starts a user participating in a particular room, if that user
//! is allowed to participate in that room. After this call, the client is
//! allowed to see all current state events in the room, and all subsequent
//! events associated with the room until the user leaves the room.
//!
//! After a user has joined a room, the room will appear as an entry in the
//! response of the [`/initialSync`](/client-server-api/#get_matrixclientv3initialsync)
//! and [`/sync`](/client-server-api/#get_matrixclientv3sync) APIs.
class QUOTIENT_API JoinRoomByIdJob : public BaseJob {
public:
    //! \param roomId
    //!   The room identifier (not alias) to join.
    //!
    //! \param thirdPartySigned
    //!   If supplied, the homeserver must verify that it matches a pending
    //!   `m.room.third_party_invite` event in the room, and perform
    //!   key validity checking if required by the event.
    //!
    //! \param reason
    //!   Optional reason to be included as the `reason` on the subsequent
    //!   membership event.
    explicit JoinRoomByIdJob(const QString& roomId,
                             const std::optional<ThirdPartySigned>& thirdPartySigned = std::nullopt,
                             const QString& reason = {});

    // Result properties

    //! The joined room ID.
    QString roomId() const { return loadFromJson<QString>("room_id"_ls); }
};

inline auto collectResponse(const JoinRoomByIdJob* job) { return job->roomId(); }

//! \brief Start the requesting user participating in a particular room.
//!
//! *Note that this API takes either a room ID or alias, unlike* `/rooms/{roomId}/join`.
//!
//! This API starts a user participating in a particular room, if that user
//! is allowed to participate in that room. After this call, the client is
//! allowed to see all current state events in the room, and all subsequent
//! events associated with the room until the user leaves the room.
//!
//! After a user has joined a room, the room will appear as an entry in the
//! response of the [`/initialSync`](/client-server-api/#get_matrixclientv3initialsync)
//! and [`/sync`](/client-server-api/#get_matrixclientv3sync) APIs.
class QUOTIENT_API JoinRoomJob : public BaseJob {
public:
    //! \param roomIdOrAlias
    //!   The room identifier or alias to join.
    //!
    //! \param serverName
    //!   The servers to attempt to join the room through. One of the servers
    //!   must be participating in the room.
    //!
    //! \param thirdPartySigned
    //!   If a `third_party_signed` was supplied, the homeserver must verify
    //!   that it matches a pending `m.room.third_party_invite` event in the
    //!   room, and perform key validity checking if required by the event.
    //!
    //! \param reason
    //!   Optional reason to be included as the `reason` on the subsequent
    //!   membership event.
    explicit JoinRoomJob(const QString& roomIdOrAlias, const QStringList& serverName = {},
                         const std::optional<ThirdPartySigned>& thirdPartySigned = std::nullopt,
                         const QString& reason = {});

    // Result properties

    //! The joined room ID.
    QString roomId() const { return loadFromJson<QString>("room_id"_ls); }
};

inline auto collectResponse(const JoinRoomJob* job) { return job->roomId(); }

} // namespace Quotient
