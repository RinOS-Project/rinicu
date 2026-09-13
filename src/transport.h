/* SPDX-License-Identifier: MIT */
#ifndef RINICU_TRANSPORT_H
#define RINICU_TRANSPORT_H

/* These names describe the current local-socket transport only.  They are
 * intentionally excluded from <rin/icu_service_abi.h> so the wire protocol
 * can move to another IPC mechanism without changing the ABI. */
#define RIN_ICU_SOCKET_PATH "/run/rin/rinicu.sock"
#define RIN_ICU_SERVICE_EXECUTABLE_ID "rinicud"

#endif /* RINICU_TRANSPORT_H */
