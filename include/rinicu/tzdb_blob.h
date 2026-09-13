/* SPDX-License-Identifier: MIT */
#ifndef RINICU_TZDB_BLOB_H
#define RINICU_TZDB_BLOB_H

#include <stdint.h>

#define RIN_ICU_TZDB_MAGIC "RITZDB1"
#define RIN_ICU_TZDB_VERSION 1u
#define RIN_ICU_TZDB_V2_VERSION 2u
#define RIN_ICU_TZDB_V2_MAGIC "RITZDB2"

enum {
    RIN_ICU_TZDB_FLAG_DST = 1u << 0
};

enum {
    RIN_ICU_TZDB_DST_RULE_NONE = 0u,
    RIN_ICU_TZDB_DST_RULE_EU = 1u,
    RIN_ICU_TZDB_DST_RULE_NORTH_AMERICA = 2u,
    RIN_ICU_TZDB_DST_RULE_AUSTRALIA_EASTERN = 3u,
    RIN_ICU_TZDB_DST_RULE_NEW_ZEALAND = 4u
};

enum {
    RIN_ICU_TZDB_DST_RULE_SHIFT = 8u,
    RIN_ICU_TZDB_DST_RULE_MASK = 0xFFu << RIN_ICU_TZDB_DST_RULE_SHIFT
};

#define RIN_ICU_TZDB_MAKE_FLAGS(has_dst, rule) \
    (((has_dst) ? RIN_ICU_TZDB_FLAG_DST : 0u) | ((uint32_t)(rule) << RIN_ICU_TZDB_DST_RULE_SHIFT))

#define RIN_ICU_TZDB_DST_RULE_FROM_FLAGS(flags) \
    (((uint32_t)(flags) & RIN_ICU_TZDB_DST_RULE_MASK) >> RIN_ICU_TZDB_DST_RULE_SHIFT)

#if defined(_MSC_VER)
#define RIN_ICU_TZDB_PACKED
#pragma pack(push, 1)
#else
#define RIN_ICU_TZDB_PACKED __attribute__((packed))
#endif

typedef struct RIN_ICU_TZDB_PACKED RinIcuTzdbHeader {
    char magic[8];
    uint32_t version;
    uint32_t zone_count;
    uint32_t record_size;
    uint32_t reserved0;
} RinIcuTzdbHeader;

typedef struct RIN_ICU_TZDB_PACKED RinIcuTzdbZoneRecord {
    char zone_id[48];
    char canonical_id[48];
    int32_t offset_minutes;
    uint32_t flags;
} RinIcuTzdbZoneRecord;

/* A v2 transition is an instant at which the zone changes to the supplied
 * offset/type.  The zone metadata supplies the initial type for earlier
 * instants. */
typedef struct RIN_ICU_TZDB_PACKED RinIcuTzdbTransitionV1 {
    int64_t at_epoch_seconds;
    int32_t offset_seconds;
    uint8_t is_dst;
    uint8_t reserved[3];
    char abbreviation[16];
} RinIcuTzdbTransitionV1;

typedef struct RIN_ICU_TZDB_PACKED RinIcuTzdbZoneMetaV1 {
    uint32_t transition_offset;
    uint32_t transition_count;
    int32_t initial_offset_seconds;
    uint8_t initial_is_dst;
    uint8_t reserved[3];
} RinIcuTzdbZoneMetaV1;

typedef struct RIN_ICU_TZDB_PACKED RinIcuTzdbV2Footer {
    char magic[8];
    uint32_t version;
    uint32_t meta_count;
    uint32_t transition_count;
    uint32_t meta_size;
    uint32_t transition_size;
    uint32_t reserved0;
} RinIcuTzdbV2Footer;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef RIN_ICU_TZDB_PACKED

#if defined(__cplusplus)
static_assert(sizeof(RinIcuTzdbHeader) == 24u, "RinICU TZDB header ABI drift");
static_assert(sizeof(RinIcuTzdbZoneRecord) == 104u,
              "RinICU TZDB zone record ABI drift");
static_assert(sizeof(RinIcuTzdbTransitionV1) == 32u,
              "RinICU TZDB transition ABI drift");
static_assert(sizeof(RinIcuTzdbZoneMetaV1) == 16u,
              "RinICU TZDB metadata ABI drift");
static_assert(sizeof(RinIcuTzdbV2Footer) == 32u,
              "RinICU TZDB footer ABI drift");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(RinIcuTzdbHeader) == 24u, "RinICU TZDB header ABI drift");
_Static_assert(sizeof(RinIcuTzdbZoneRecord) == 104u,
               "RinICU TZDB zone record ABI drift");
_Static_assert(sizeof(RinIcuTzdbTransitionV1) == 32u,
               "RinICU TZDB transition ABI drift");
_Static_assert(sizeof(RinIcuTzdbZoneMetaV1) == 16u,
               "RinICU TZDB metadata ABI drift");
_Static_assert(sizeof(RinIcuTzdbV2Footer) == 32u,
               "RinICU TZDB footer ABI drift");
#endif

#endif
