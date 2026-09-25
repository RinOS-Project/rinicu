/* SPDX-License-Identifier: MIT */
#ifndef RINICU_DATA_BLOB_H
#define RINICU_DATA_BLOB_H

#include <stdint.h>

#define RIN_ICU_DATA_MAGIC "RICUDB1"
#define RIN_ICU_DATA_VERSION 1u

enum {
    RIN_ICU_DATA_FLAG_DEFAULT_H12 = 1u << 0
};

enum {
    RIN_ICU_PLURAL_RULE_NONE = 0,
    RIN_ICU_PLURAL_RULE_ONE = 1,
    RIN_ICU_PLURAL_RULE_FRENCH_ONE = 2,
    RIN_ICU_PLURAL_RULE_SLAVIC = 3,
    RIN_ICU_PLURAL_RULE_ENGLISH_ORDINAL = 4
};

#if defined(_MSC_VER)
#define RIN_ICU_BLOB_PACKED
#pragma pack(push, 1)
#else
#define RIN_ICU_BLOB_PACKED __attribute__((packed))
#endif

typedef struct RIN_ICU_BLOB_PACKED RinIcuDataHeader {
    char magic[8];
    uint32_t version;
    uint32_t locale_count;
    uint32_t record_size;
    uint32_t reserved0;
} RinIcuDataHeader;

typedef struct RIN_ICU_BLOB_PACKED RinIcuDataLocaleRecord {
    char locale_id[16];
    char language[8];
    char region[8];
    char script[8];
    char currency_code[8];
    char currency_symbol[16];
    char decimal_sep[8];
    char group_sep[8];
    char plus_sign[8];
    char minus_sign[8];
    char percent_sign[8];
    char am[8];
    char pm[8];
    char decimal_pattern[32];
    char percent_pattern[32];
    char currency_pattern[32];
    char date_pattern[32];
    char long_date_pattern[32];
    char time_pattern[32];
    char datetime_pattern[48];
    uint32_t cardinal_rule;
    uint32_t ordinal_rule;
    uint32_t currency_digits;
    uint32_t flags;
    uint32_t reserved0;
} RinIcuDataLocaleRecord;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef RIN_ICU_BLOB_PACKED

#if defined(__cplusplus)
static_assert(sizeof(RinIcuDataHeader) == 24u, "RinICU data header ABI drift");
static_assert(sizeof(RinIcuDataLocaleRecord) == 380u,
              "RinICU locale record ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RinIcuDataHeader) == 24u, "RinICU data header ABI drift");
_Static_assert(sizeof(RinIcuDataLocaleRecord) == 380u,
               "RinICU locale record ABI drift");
#endif

#endif
