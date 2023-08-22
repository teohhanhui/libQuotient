// SPDX-FileCopyrightText: 2023 James Graham <james.h.graham@protonmail.com>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "quotient_common.h"
#include "util.h"

#include <QtCore/QObject>

namespace Quotient {
class Room;
class RoomMemberEvent;

/**
 * @class RoomMember
 *
 * This class is for visualizing a user in a room context.
 *
 * The class is intentionally a read-only data object that is effectively a wrapper
 * around an m.room.member event for the desired user. This is designed provide the
 * data in a format ready for visualizing a user (avatar or name) in the context
 * of the room it was generated in. This means that if a user has set a unique
 * name or avatar for a particular room that is what will be returned.
 *
 * @note The RoomMember class is not intended for interacting with a User's profile.
 *       For that a Quotient::User object should be obtained from a
 *       Quotient::Connection as that has the support functions for modifying profile
 *       information.
 *
 * @sa Quotient::User
 */
class QUOTIENT_API RoomMember {
    Q_GADGET
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString displayName READ displayName)
    Q_PROPERTY(QString fullName READ fullName)
    Q_PROPERTY(int hue READ hue CONSTANT)
    Q_PROPERTY(qreal hueF READ hueF CONSTANT)
    Q_PROPERTY(QColor color READ color CONSTANT)
    Q_PROPERTY(QUrl avatarUrl READ avatarUrl)

public:
    RoomMember(const RoomMemberEvent* member, const Room* room);

    /**
     * @brief Get unique stable user id.
     *
     * The Matrix user ID is generated by the server and is never changed.
     */
    QString id() const;

    //! \brief The membership state of the member.
    Membership membershipState() const;

    /**
     * @brief The raw unmodified display name for the user in the given room.
     *
     * The value will be empty if no display name has been set.
     *
     * @warning This value is not sanitized or HTML escape so use appropriately.
     *          For ready to display values use displayName() or fullName() for
     *          plain text and htmlSafeDisplayName() or htmlSafeFullName() fo
     *          rich text.
     *
     * @sa displayName(), htmlSafeDisplayName(), fullName(), htmlSafeFullName()
     */
    QString name() const;

    /**
     * @brief Get the user display name ready for display.
     *
     * This function always aims to return something that can be displayed in a
     * UI, so if no display name is set the user's Matrix ID will be returned.
     *
     * The output is sanitized and suitable for a plain text field. For a rich
     * field use htmlSafeDisplayName().
     *
     * @sa htmlSafeDisplayName()
     */
    QString displayName() const;

    /**
     * @brief Get the user display name ready for display.
     *
     * This function always aims to return something that can be displayed in a
     * UI, so if no display name is set the user's Matrix ID will be returned.
     *
     * The output is sanitized and html escaped ready for a rich text field. For
     * a plain field use displayName().
     *
     * @sa displayName()
     */
    QString htmlSafeDisplayName() const;

    /**
     * @brief Get user name and id in a single string.
     *
     * This function always aims to return something that can be displayed in a
     * UI, so if no display name is set the just user's Matrix ID will be returned.
     * The constructed string follows the format 'name (id)' which the spec
     * recommends for users disambiguation in a room context and in other places.
     *
     * The output is sanitized and suitable for a plain text field. For a rich
     * field use htmlSafeFullName().
     *
     * @sa htmlSafeFullName()
     */
    QString fullName() const;

    /**
     * @brief Get user name and id in a single string.
     *
     * This function always aims to return something that can be displayed in a
     * UI, so if no display name is set the just user's Matrix ID will be returned.
     * The constructed string follows the format 'name (id)' which the spec
     * recommends for users disambiguation in a room context and in other places.
     *
     * The output is sanitized and html escaped ready for a rich text field. For
     * a plain field use fullName().
     *
     * @sa fullName()
     */
    QString htmlSafeFullName() const;

    /**
     * @brief Hue color component of this user based on the user's Matrix ID.
     *
     * The implementation is based on XEP-0392:
     * https://xmpp.org/extensions/xep-0392.html
     * Naming and ranges are the same as QColor's hue methods:
     * https://doc.qt.io/qt-5/qcolor.html#integer-vs-floating-point-precision
     */
    int hue() const;

    /**
     * @brief HueF color component of this user based on the user's Matrix ID.
     *
     * The implementation is based on XEP-0392:
     * https://xmpp.org/extensions/xep-0392.html
     * Naming and ranges are the same as QColor's hue methods:
     * https://doc.qt.io/qt-5/qcolor.html#integer-vs-floating-point-precision
     */
    qreal hueF() const;

    /**
     * @brief Color based on the user's Matrix ID.
     *
     * See https://github.com/quotient-im/libQuotient/wiki/User-color-coding-standard-draft-proposal
     * for the methodology.
     */
    QColor color() const;

    /**
     * @brief The mxc URL as a string for the user avatar in the room.
     *
     * This can be empty if none set.
     */
    QString avatarMediaId() const;

    /**
     * @brief The mxc URL for the user avatar in the room.
     *
     * This can be empty if none set.
     */
    QUrl avatarUrl() const;

private:
    class Private;
    ImplPtr<Private> d;
};
} // namespace Quotient
