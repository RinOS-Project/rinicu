/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>

#include <rin/icu/service_abi.h>
#include <rinicu/data_blob.h>
#include <rinicu/tzdb_blob.h>
#include <rinicu/rinicu.h>

int main(void)
{
    rin_icu_client_t client = {};
    RinIcuMsgHeader header = {};

    assert(sizeof(RinIcuMsgHeader) == 32u);
    assert(sizeof(RinIcuCollatorOptions) == 16u);
    assert(sizeof(RinIcuSegmenterOptions) == 16u);
    assert(sizeof(RinIcuNumberFormatterOptions) == 24u);
    assert(sizeof(RinIcuDateTimeFormatterOptions) == 16u);
    assert(sizeof(RinIcuPluralRulesOptions) == 8u);
    assert(sizeof(RinIcuDataHeader) == 24u);
    assert(sizeof(RinIcuDataLocaleRecord) == 348u);
    assert(sizeof(RinIcuTzdbHeader) == 24u);
    assert(sizeof(RinIcuTzdbZoneRecord) == 104u);
    assert(sizeof(RinIcuTzdbTransitionV1) == 32u);
    assert(sizeof(RinIcuTzdbZoneMetaV1) == 16u);
    assert(sizeof(RinIcuTzdbV2Footer) == 32u);
    assert(sizeof(client) == RIN_ICU_CLIENT_STORAGE_SIZE);
    assert(RIN_ICU_MAX_CSTRING_BYTES == 65536u);

    assert(RIN_ICU_MAGIC == UINT32_C(0x52495631));
    assert(RIN_ICU_VERSION == 2u);
    assert(RIN_ICU_MAX_INLINE_PAYLOAD == 65536u);
    assert(RIN_ICU_MAX_BULK_ITEMS == 4096u);
    assert(offsetof(RinIcuMsgHeader, request_id) == 16u);
    assert(offsetof(RinIcuMsgHeader, payload_len) == 24u);

    header.magic = RIN_ICU_MAGIC;
    header.version = RIN_ICU_VERSION;
    header.command = RIN_ICU_CMD_LOCALE_AVAILABLE_V1;
    header.request_id = 1u;
    assert(rin_icu_header_valid_v2(&header));
    return 0;
}
