/* SPDX-License-Identifier: MIT */
#include <assert.h>

#include <rin/icu_service_abi.h>

int main(void)
{
    RinIcuMsgHeader header = {};
    header.magic = RIN_ICU_MAGIC;
    header.version = RIN_ICU_VERSION;
    header.command = RIN_ICU_CMD_LOCALE_AVAILABLE_V1;
    header.request_id = 7u;

    assert(rin_icu_header_valid_v2(&header));

    header.magic ^= 1u; /* wrong magic */
    assert(!rin_icu_header_valid_v2(&header));
    header.magic = RIN_ICU_MAGIC;
    header.version = RIN_ICU_VERSION + 1u; /* wrong version */
    assert(!rin_icu_header_valid_v2(&header));
    header.version = RIN_ICU_VERSION;
    header.command = 0xFFFFFFFFu; /* unknown command */
    assert(!rin_icu_header_valid_v2(&header));
    header.command = RIN_ICU_CMD_LOCALE_AVAILABLE_V1;
    header.request_id = 0u; /* stale/invalid request ID */
    assert(!rin_icu_header_valid_v2(&header));
    header.request_id = 7u;
    header.flags = 1u; /* reserved/unknown flag */
    assert(!rin_icu_header_valid_v2(&header));
    header.flags = 0u;
    header.payload_len = RIN_ICU_MAX_INLINE_PAYLOAD + 1u;
    assert(!rin_icu_header_valid_v2(&header));
    assert(!rin_icu_status_known_v2(-999));
    assert(rin_icu_status_known_v2(RIN_ICU_STATUS_DATA_ERROR));
    assert(!rin_icu_command_known_v2(0u));
    return 0;
}
