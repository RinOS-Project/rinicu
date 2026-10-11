/* SPDX-License-Identifier: MIT */
#ifndef RINICU_DATA_BLOB_H
#define RINICU_DATA_BLOB_H

#include <stdint.h>

#define RIN_ICU_DATA_MAGIC "RICUDB1"
#define RIN_ICU_DATA_VERSION 1u

enum {
    RIN_ICU_DATA_FLAG_DEFAULT_H12 = 1u << 0,
    /* Bit 1 and the two following bytes preserve CLDR decimal grouping in
     * the fixed-size v1 record without changing the 380-byte ABI. */
    RIN_ICU_DATA_FLAG_GROUPING_PRESENT = 1u << 1,
    /* Bits 24-27 carry the CLDR standard decimal maximum fraction digits
     * without changing the fixed-size v1 record. */
    RIN_ICU_DATA_FLAG_DECIMAL_DIGITS_PRESENT = 1u << 2
};

#define RIN_ICU_DATA_GROUPING_PRIMARY_SHIFT 8u
#define RIN_ICU_DATA_GROUPING_PRIMARY_MASK UINT32_C(0x00007f00)
#define RIN_ICU_DATA_GROUPING_SECONDARY_SHIFT 16u
#define RIN_ICU_DATA_GROUPING_SECONDARY_MASK UINT32_C(0x007f0000)
#define RIN_ICU_DATA_GROUPING_FLAGS_MASK \
    (RIN_ICU_DATA_FLAG_GROUPING_PRESENT | \
     RIN_ICU_DATA_GROUPING_PRIMARY_MASK | \
     RIN_ICU_DATA_GROUPING_SECONDARY_MASK)

#define RIN_ICU_DATA_DECIMAL_DIGITS_SHIFT 24u
#define RIN_ICU_DATA_DECIMAL_DIGITS_MASK UINT32_C(0x0f000000)
#define RIN_ICU_DATA_DECIMAL_FLAGS_MASK \
    (RIN_ICU_DATA_FLAG_DECIMAL_DIGITS_PRESENT | \
     RIN_ICU_DATA_DECIMAL_DIGITS_MASK)

/* The final word keeps the original RICUDB1 record size while carrying the
 * CLDR supplementalData/weekData projection. */
#define RIN_ICU_DATA_WEEK_FIRST_DAY_SHIFT 0u
#define RIN_ICU_DATA_WEEK_FIRST_DAY_MASK UINT32_C(0x00000007)
#define RIN_ICU_DATA_WEEK_MIN_DAYS_SHIFT 3u
#define RIN_ICU_DATA_WEEK_MIN_DAYS_MASK UINT32_C(0x00000038)
#define RIN_ICU_DATA_WEEKEND_START_SHIFT 6u
#define RIN_ICU_DATA_WEEKEND_START_MASK UINT32_C(0x000001c0)
#define RIN_ICU_DATA_WEEKEND_END_SHIFT 9u
#define RIN_ICU_DATA_WEEKEND_END_MASK UINT32_C(0x00000e00)
#define RIN_ICU_DATA_WEEK_DATA_MASK \
    (RIN_ICU_DATA_WEEK_FIRST_DAY_MASK | RIN_ICU_DATA_WEEK_MIN_DAYS_MASK | \
     RIN_ICU_DATA_WEEKEND_START_MASK | RIN_ICU_DATA_WEEKEND_END_MASK)

enum {
    RIN_ICU_DATA_WEEK_SUNDAY = 0,
    RIN_ICU_DATA_WEEK_MONDAY = 1,
    RIN_ICU_DATA_WEEK_TUESDAY = 2,
    RIN_ICU_DATA_WEEK_WEDNESDAY = 3,
    RIN_ICU_DATA_WEEK_THURSDAY = 4,
    RIN_ICU_DATA_WEEK_FRIDAY = 5,
    RIN_ICU_DATA_WEEK_SATURDAY = 6
};

enum {
    RIN_ICU_PLURAL_RULE_NONE = 0,
    RIN_ICU_PLURAL_RULE_ONE = 1,
    RIN_ICU_PLURAL_RULE_FRENCH_ONE = 2,
    RIN_ICU_PLURAL_RULE_SLAVIC = 3,
    RIN_ICU_PLURAL_RULE_ENGLISH_ORDINAL = 4,
    RIN_ICU_PLURAL_RULE_ARABIC = 5,
    RIN_ICU_PLURAL_RULE_ZERO_ONE = 6,
    RIN_ICU_PLURAL_RULE_ONE_FEW_MANY_V = 7,
    RIN_ICU_PLURAL_RULE_POLISH = 8,
    RIN_ICU_PLURAL_RULE_ROMANIAN = 9,
    RIN_ICU_PLURAL_RULE_HEBREW = 10,
    RIN_ICU_PLURAL_RULE_DANISH = 11,
    RIN_ICU_PLURAL_RULE_FINNISH = 12,
    RIN_ICU_PLURAL_RULE_ZERO_OR_ONE_INTEGER = 13,
    RIN_ICU_PLURAL_RULE_ORDINAL_ONE = 14,
    RIN_ICU_PLURAL_RULE_ONE_MANY_MILLION = 15,
    RIN_ICU_PLURAL_RULE_ZERO_ONE_MANY_MILLION = 16,
    RIN_ICU_PLURAL_RULE_ORDINAL_CATALAN = 17,
    RIN_ICU_PLURAL_RULE_ORDINAL_HINDI = 18,
    RIN_ICU_PLURAL_RULE_ORDINAL_HUNGARIAN = 19,
    RIN_ICU_PLURAL_RULE_ORDINAL_ITALIAN = 20,
    RIN_ICU_PLURAL_RULE_ORDINAL_SWEDISH = 21,
    RIN_ICU_PLURAL_RULE_ORDINAL_UKRAINIAN = 22
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
    uint32_t week_data;
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
