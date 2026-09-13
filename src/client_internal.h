/* SPDX-License-Identifier: MIT */
#ifndef RINICU_CLIENT_INTERNAL_H
#define RINICU_CLIENT_INTERNAL_H

#include <stdint.h>
#include <string.h>

#include <rinicu/rinicu.h>
#include "transport.h"

#define RIN_ICU_CLIENT_STATE_MAGIC UINT32_C(0x52494332) /* "RIC2" */

typedef struct rin_icu_client_internal {
    uint32_t state_magic;
    int32_t fd;
    uint32_t next_request_id;
    uint32_t reserved0;
} rin_icu_client_internal_t;

#if defined(__cplusplus)
static_assert(sizeof(rin_icu_client_internal_t) <= RIN_ICU_CLIENT_STORAGE_SIZE,
              "RinICU internal client exceeds public storage");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(rin_icu_client_internal_t) <= RIN_ICU_CLIENT_STORAGE_SIZE,
               "RinICU internal client exceeds public storage");
#endif

static inline rin_icu_client_internal_t*
rin_icu_client_internal(rin_icu_client_t* client)
{
    return client ? (rin_icu_client_internal_t*)(void*)client->opaque : NULL;
}

static inline const rin_icu_client_internal_t*
rin_icu_client_internal_const(const rin_icu_client_t* client)
{
    return client ? (const rin_icu_client_internal_t*)(const void*)client->opaque : NULL;
}

static inline void rin_icu_client_storage_zero(rin_icu_client_t* client)
{
    if (client) memset(client, 0, sizeof(*client));
}

#endif /* RINICU_CLIENT_INTERNAL_H */
