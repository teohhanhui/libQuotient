// SPDX-FileCopyrightText: 2019 Black Hat <bhat@encom.eu.org>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "stateevent.h"

namespace Quotient {

//! \brief Default power levels used in absence of a power level event
//!
//! It's not recommended to use this directly; Room::can*() functions calculate effective ability
//! to perform certain operations, and Room::powerLevelFor*() returns the necessary power level for
//! an operation given the current room state.
template <EventClass EvT>
constexpr inline auto DefaultPowerLevel = std::is_base_of_v<StateEvent, EvT> ? 50 : 0;

struct QUOTIENT_API PowerLevelsEventContent {
    struct Notifications {
        int room;
    };

    explicit PowerLevelsEventContent(const QJsonObject& json);
    QJsonObject toJson() const;

    int invite;
    int kick;
    int ban;

    int redact;

    QHash<QString, int> events;
    int eventsDefault;
    int stateDefault;

    QHash<QString, int> users;
    int usersDefault;

    Notifications notifications;
};

class QUOTIENT_API RoomPowerLevelsEvent
    : public KeylessStateEventBase<RoomPowerLevelsEvent, PowerLevelsEventContent> {
public:
    QUO_EVENT(RoomPowerLevelsEvent, "m.room.power_levels")

    using KeylessStateEventBase::KeylessStateEventBase;

    int invite() const { return content().invite; }
    int kick() const { return content().kick; }
    int ban() const { return content().ban; }

    int redact() const { return content().redact; }

    QHash<QString, int> events() const { return content().events; }
    int eventsDefault() const { return content().eventsDefault; }
    int stateDefault() const { return content().stateDefault; }

    QHash<QString, int> users() const { return content().users; }
    int usersDefault() const { return content().usersDefault; }

    int roomNotification() const { return content().notifications.room; }

    //! \brief Get the power level for message events of a given type
    //!
    //! \note You normally should not compare power levels returned from this
    //!       and other powerLevelFor*() functions directly; use
    //!       Room::canSendEvents() instead
    int powerLevelForEvent(const QString& eventTypeId) const;

    //! \brief Get the power level for state events of a given type
    //!
    //! \note You normally should not compare power levels returned from this
    //!       and other powerLevelFor*() functions directly; use
    //!       Room::canSetState() instead
    int powerLevelForState(const QString& eventTypeId) const;

    //! \brief Get the power level for a given user
    //!
    //! \note You normally should not directly use power levels returned by this
    //!       and other powerLevelFor*() functions; use Room API instead
    //! \sa Room::canSend, Room::canSendEvents, Room::canSetState,
    //!     Room::effectivePowerLevel
    int powerLevelForUser(const QString& userId) const;

    template <EventClass EvT>
    int powerLevelForEventType() const
    {
        if constexpr (std::is_base_of_v<StateEvent, EvT>) {
            return powerLevelForState(EvT::TypeId);
        } else {
            return powerLevelForEvent(EvT::TypeId);
        }
    }
};
} // namespace Quotient
