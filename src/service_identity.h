/* SPDX-License-Identifier: MIT */
#ifndef RINICU_SERVICE_IDENTITY_H
#define RINICU_SERVICE_IDENTITY_H

#include <rin/socket_abi.h>

/* The socket path is only a locator.  The kernel-published identity is the
 * admission boundary for the public client. */
static int rin_icu_service_identity_valid(
    const rin_unix_service_identity_v1* identity)
{
    return identity != NULL && identity->slot_id != 0u &&
           identity->owner_uid == 0u && identity->scope == 1u &&
           identity->flags == RIN_UNIX_SERVICE_IDENTITY_FLAG_PUBLISHED;
}

#endif /* RINICU_SERVICE_IDENTITY_H */
