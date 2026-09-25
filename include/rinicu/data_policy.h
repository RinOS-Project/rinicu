/* SPDX-License-Identifier: MIT */
#ifndef RINICU_DATA_POLICY_H
#define RINICU_DATA_POLICY_H

#include <stdint.h>
#include <stddef.h>
#include <rinicu/data_blob.h>
#include <rinicu/tzdb_blob.h>

#define RIN_ICU_DATA_MAX_LOCALES 512u
#define RIN_ICU_TZDB_MAX_ZONES 1024u
#define RIN_ICU_TZDB_MAX_TRANSITIONS 262144u

static inline int rin_icu_data_u64_add(uint64_t left, uint64_t right,
                                       uint64_t* out)
{
    if (!out || right > UINT64_MAX - left) return 0;
    *out = left + right;
    return 1;
}

static inline int rin_icu_data_u64_mul(uint64_t left, uint64_t right,
                                       uint64_t* out)
{
    if (!out || (left != 0u && right > UINT64_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

static inline int rin_icu_data_bytes_equal(const char* left, const char* right,
                                           size_t size)
{
    size_t index;
    unsigned char difference = 0u;
    if (!left || !right) return 0;
    for (index = 0u; index < size; ++index)
        difference |= (unsigned char)left[index] ^ (unsigned char)right[index];
    return difference == 0u;
}

static inline int rin_icu_data_bytes_terminated(const char* value,
                                                size_t capacity,
                                                int require_nonempty)
{
    size_t index;
    if (!value || capacity == 0u) return 0;
    for (index = 0u; index < capacity; ++index) {
        unsigned char byte = (unsigned char)value[index];
        if (byte == 0u) return !require_nonempty || index != 0u;
        if (byte < 0x20u || byte == 0x7fu) return 0;
    }
    return 0;
}

static inline int rin_icu_data_header_valid(const RinIcuDataHeader* header,
                                            uint64_t file_size)
{
    uint64_t expected;
    if (!header || sizeof(RinIcuDataHeader) > file_size ||
        !rin_icu_data_bytes_equal(header->magic, RIN_ICU_DATA_MAGIC,
                                  sizeof(header->magic)) ||
        header->version != RIN_ICU_DATA_VERSION ||
        header->record_size != sizeof(RinIcuDataLocaleRecord) ||
        header->reserved0 != 0u || header->locale_count == 0u ||
        header->locale_count > RIN_ICU_DATA_MAX_LOCALES)
        return 0;
    if (!rin_icu_data_u64_mul(header->locale_count, header->record_size,
                              &expected) ||
        !rin_icu_data_u64_add(sizeof(RinIcuDataHeader), expected, &expected))
        return 0;
    return expected == file_size;
}

static inline int rin_icu_data_locale_valid(
    const RinIcuDataLocaleRecord* record)
{
    if (!record || !rin_icu_data_bytes_terminated(record->locale_id,
                                                  sizeof(record->locale_id), 1) ||
        !rin_icu_data_bytes_terminated(record->language,
                                       sizeof(record->language), 0) ||
        !rin_icu_data_bytes_terminated(record->region,
                                       sizeof(record->region), 0) ||
        !rin_icu_data_bytes_terminated(record->script,
                                       sizeof(record->script), 0) ||
        !rin_icu_data_bytes_terminated(record->currency_code,
                                       sizeof(record->currency_code), 0) ||
        !rin_icu_data_bytes_terminated(record->currency_symbol,
                                       sizeof(record->currency_symbol), 0) ||
        !rin_icu_data_bytes_terminated(record->decimal_sep,
                                       sizeof(record->decimal_sep), 0) ||
        !rin_icu_data_bytes_terminated(record->group_sep,
                                       sizeof(record->group_sep), 0) ||
        !rin_icu_data_bytes_terminated(record->plus_sign,
                                       sizeof(record->plus_sign), 0) ||
        !rin_icu_data_bytes_terminated(record->minus_sign,
                                       sizeof(record->minus_sign), 0) ||
        !rin_icu_data_bytes_terminated(record->percent_sign,
                                       sizeof(record->percent_sign), 0) ||
        !rin_icu_data_bytes_terminated(record->am, sizeof(record->am), 0) ||
        !rin_icu_data_bytes_terminated(record->pm, sizeof(record->pm), 0) ||
        !rin_icu_data_bytes_terminated(record->decimal_pattern,
                                       sizeof(record->decimal_pattern), 0) ||
        !rin_icu_data_bytes_terminated(record->percent_pattern,
                                       sizeof(record->percent_pattern), 0) ||
        !rin_icu_data_bytes_terminated(record->currency_pattern,
                                       sizeof(record->currency_pattern), 0) ||
        !rin_icu_data_bytes_terminated(record->date_pattern,
                                       sizeof(record->date_pattern), 0) ||
        !rin_icu_data_bytes_terminated(record->long_date_pattern,
                                       sizeof(record->long_date_pattern), 0) ||
        !rin_icu_data_bytes_terminated(record->time_pattern,
                                       sizeof(record->time_pattern), 0) ||
        !rin_icu_data_bytes_terminated(record->datetime_pattern,
                                       sizeof(record->datetime_pattern), 0) ||
        record->cardinal_rule > RIN_ICU_PLURAL_RULE_ENGLISH_ORDINAL ||
        record->ordinal_rule > RIN_ICU_PLURAL_RULE_ENGLISH_ORDINAL ||
        record->currency_digits > 3u ||
        (record->flags & ~RIN_ICU_DATA_FLAG_DEFAULT_H12) != 0u ||
        record->reserved0 != 0u)
        return 0;
    return 1;
}

static inline int rin_icu_tzdb_header_valid(const RinIcuTzdbHeader* header,
                                            uint64_t file_size)
{
    uint64_t expected;
    if (!header || sizeof(RinIcuTzdbHeader) > file_size ||
        !rin_icu_data_bytes_equal(header->magic, RIN_ICU_TZDB_MAGIC,
                                  sizeof(header->magic)) ||
        header->version != RIN_ICU_TZDB_VERSION ||
        header->record_size != sizeof(RinIcuTzdbZoneRecord) ||
        header->reserved0 != 0u || header->zone_count == 0u ||
        header->zone_count > RIN_ICU_TZDB_MAX_ZONES)
        return 0;
    if (!rin_icu_data_u64_mul(header->zone_count, header->record_size,
                              &expected) ||
        !rin_icu_data_u64_add(sizeof(RinIcuTzdbHeader), expected, &expected))
        return 0;
    return expected == file_size;
}

static inline int rin_icu_tzdb_v2_header_valid(
    const RinIcuTzdbHeader* header)
{
    return header != NULL &&
           rin_icu_data_bytes_equal(header->magic, RIN_ICU_TZDB_MAGIC,
                                    sizeof(header->magic)) &&
           header->version == RIN_ICU_TZDB_V2_VERSION &&
           header->record_size == sizeof(RinIcuTzdbZoneRecord) &&
           header->reserved0 == 0u && header->zone_count != 0u &&
           header->zone_count <= RIN_ICU_TZDB_MAX_ZONES;
}

static inline int rin_icu_tzdb_v2_footer_valid(
    const RinIcuTzdbV2Footer* footer, uint32_t zone_count,
    uint64_t file_size)
{
    uint64_t base;
    uint64_t expected;
    if (!footer || !rin_icu_data_bytes_equal(
            footer->magic, RIN_ICU_TZDB_V2_MAGIC, sizeof(footer->magic)) ||
        footer->version != RIN_ICU_TZDB_V2_VERSION ||
        footer->meta_size != sizeof(RinIcuTzdbZoneMetaV1) ||
        footer->transition_size != sizeof(RinIcuTzdbTransitionV1) ||
        footer->meta_count != zone_count ||
        footer->transition_count > RIN_ICU_TZDB_MAX_TRANSITIONS ||
        footer->reserved0 != 0u)
        return 0;
    if (!rin_icu_data_u64_mul(zone_count, sizeof(RinIcuTzdbZoneRecord), &base) ||
        !rin_icu_data_u64_add(sizeof(RinIcuTzdbHeader), base, &base) ||
        !rin_icu_data_u64_add(base, sizeof(*footer), &expected) ||
        !rin_icu_data_u64_mul(footer->meta_count, footer->meta_size, &base) ||
        !rin_icu_data_u64_add(expected, base, &expected) ||
        !rin_icu_data_u64_mul(footer->transition_count,
                              footer->transition_size, &base) ||
        !rin_icu_data_u64_add(expected, base, &expected))
        return 0;
    return expected == file_size;
}

static inline int rin_icu_tzdb_transition_valid(
    const RinIcuTzdbTransitionV1* transition)
{
    size_t index;
    if (!transition || transition->is_dst > 1u ||
        transition->offset_seconds < -86400 ||
        transition->offset_seconds > 86400)
        return 0;
    for (index = 0u; index < sizeof(transition->reserved); ++index)
        if (transition->reserved[index] != 0u) return 0;
    return rin_icu_data_bytes_terminated(
        transition->abbreviation, sizeof(transition->abbreviation), 0);
}

static inline int rin_icu_tzdb_meta_valid(
    const RinIcuTzdbZoneMetaV1* meta, uint32_t transition_count)
{
    size_t index;
    if (!meta || meta->transition_offset > transition_count ||
        meta->transition_count > transition_count - meta->transition_offset ||
        meta->initial_offset_seconds < -86400 ||
        meta->initial_offset_seconds > 86400 ||
        meta->initial_is_dst > 1u)
        return 0;
    for (index = 0u; index < sizeof(meta->reserved); ++index)
        if (meta->reserved[index] != 0u) return 0;
    return 1;
}

static inline int rin_icu_tzdb_zone_valid(
    const RinIcuTzdbZoneRecord* record)
{
    uint32_t rule;
    if (!record || !rin_icu_data_bytes_terminated(record->zone_id,
                                                  sizeof(record->zone_id), 1) ||
        !rin_icu_data_bytes_terminated(record->canonical_id,
                                       sizeof(record->canonical_id), 1) ||
        record->offset_minutes < -1440 || record->offset_minutes > 1440 ||
        (record->flags & ~((uint32_t)RIN_ICU_TZDB_FLAG_DST |
                           (uint32_t)RIN_ICU_TZDB_DST_RULE_MASK)) !=
            0u)
        return 0;
    rule = RIN_ICU_TZDB_DST_RULE_FROM_FLAGS(record->flags);
    return rule <= RIN_ICU_TZDB_DST_RULE_NEW_ZEALAND;
}

#endif
