/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Include the implementation so the host-only test can exercise private
 * wire-shape validators without adding them to the public API. */
#include "../src/client.c"

int main(void)
{
    rin_icu_client_t client;
    rin_icu_client_internal_t* internal;
    unsigned char text_payload[sizeof(RinIcuBytesResponse) + 3u];
    unsigned char bulk_payload[sizeof(RinIcuBytesResponse) +
                               sizeof(RinIcuBulkBytesResponse) +
                               sizeof(uint32_t) + 2u];
    RinIcuBytesResponse outer;
    RinIcuBulkBytesResponse inner;
    RinIcuCompareResponse compare;

    memset(&client, 0, sizeof(client));
    rin_icu_client_init(&client);
    internal = rin_icu_client_internal(&client);
    internal->next_request_id = UINT32_MAX;
    assert(rin_icu_next_request_id(internal) == UINT32_MAX);
    assert(internal->next_request_id == 1u);
    assert(rin_icu_next_request_id(internal) == 1u);
    assert(!rin_icu_status_valid(-999));

    memset(text_payload, 0, sizeof(text_payload));
    ((RinIcuBytesResponse*)text_payload)->data_len = 3u;
    memcpy(text_payload + sizeof(RinIcuBytesResponse), "abc", 3u);
    assert(rin_icu_bytes_response_shape(text_payload, sizeof(text_payload)));
    assert(!rin_icu_bytes_response_shape(text_payload, sizeof(text_payload) - 1u));
    assert(!rin_icu_response_payload_valid(RIN_ICU_CMD_LOCALE_CANONICALIZE_V1,
                                           RIN_ICU_STATUS_INVALID,
                                           text_payload, sizeof(text_payload)));

    memset(bulk_payload, 0, sizeof(bulk_payload));
    outer.data_len = (uint32_t)(sizeof(inner) + sizeof(uint32_t) + 2u);
    inner.item_count = 1u;
    inner.reserved0 = 0u;
    memcpy(bulk_payload, &outer, sizeof(outer));
    memcpy(bulk_payload + sizeof(outer), &inner, sizeof(inner));
    *(uint32_t*)(bulk_payload + sizeof(outer) + sizeof(inner)) = 2u;
    memcpy(bulk_payload + sizeof(outer) + sizeof(inner) + sizeof(uint32_t), "xy", 2u);
    assert(rin_icu_bulk_bytes_response_shape(bulk_payload, sizeof(bulk_payload)));
    inner.reserved0 = 1u;
    memcpy(bulk_payload + sizeof(outer), &inner, sizeof(inner));
    assert(!rin_icu_bulk_bytes_response_shape(bulk_payload, sizeof(bulk_payload)));

    assert(rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_CREATE_V1, 0u, 1u));
    assert(!rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_CREATE_V1, 0u, 0u));
    assert(rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_COMPARE_V1, 7u, 7u));
    assert(!rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_COMPARE_V1, 7u, 8u));

    compare.result = 0;
    compare.reserved0 = 1u;
    assert(!rin_icu_response_payload_valid(RIN_ICU_CMD_COLLATOR_COMPARE_V1,
                                           RIN_ICU_STATUS_OK,
                                           (unsigned char*)&compare,
                                           sizeof(compare)));
    rin_icu_client_close(&client);
    return 0;
}
