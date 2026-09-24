/* SPDX-License-Identifier: MIT */
#ifndef RINICU_RINICU_H
#define RINICU_RINICU_H

#include <stddef.h>
#include <stdint.h>

#include <rin/icu/service_abi.h>
#include <rinicu/data_blob.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RIN_ICU_CLIENT_STORAGE_WORDS 4u
#define RIN_ICU_CLIENT_STORAGE_SIZE \
    (sizeof(uintptr_t) * RIN_ICU_CLIENT_STORAGE_WORDS)

/* Maximum bytes inspected by APIs that accept a NUL-terminated caller
 * string.  The transport still applies the smaller per-request payload
 * limit after the scan; this bound prevents an unterminated pointer from
 * becoming an unbounded memory walk before that validation. */
#define RIN_ICU_MAX_CSTRING_BYTES (64u * 1024u)

/* Caller-owned storage for the connection-scoped client.  The contents are
 * private and may change between library releases; applications must use the
 * lifecycle functions below and must not inspect this storage. */
typedef struct rin_icu_client {
    uintptr_t opaque[RIN_ICU_CLIENT_STORAGE_WORDS];
} rin_icu_client_t;

typedef uint32_t rin_icu_handle_t;
typedef RinIcuCollatorOptions rin_icu_collator_options_t;
typedef RinIcuSegmenterOptions rin_icu_segmenter_options_t;
typedef RinIcuNumberFormatterOptions rin_icu_number_formatter_options_t;
typedef RinIcuNumberFormatterOptionsV2 rin_icu_number_formatter_options_v2_t;
typedef RinIcuDateTimeFormatterOptions rin_icu_datetime_formatter_options_t;
typedef RinIcuPluralRulesOptions rin_icu_plural_rules_options_t;
typedef RinIcuSegmentNextResponse rin_icu_segment_t;

void rin_icu_client_init(rin_icu_client_t* client);
int rin_icu_client_open(rin_icu_client_t* client);
void rin_icu_client_close(rin_icu_client_t* client);
int rin_icu_client_is_open(const rin_icu_client_t* client);

int rin_icu_locale_canonicalize(rin_icu_client_t* client, const char* input, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_locale_resolve(rin_icu_client_t* client, const char* input, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_normalize(rin_icu_client_t* client, int form, const char* input, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_case_map(rin_icu_client_t* client, const char* locale, const char* input, int to_upper, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_locale_maximize(rin_icu_client_t* client, const char* input, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_locale_minimize(rin_icu_client_t* client, const char* input, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_locale_available(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_locale_preferred(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_locale_info(rin_icu_client_t* client, const char* input, RinIcuDataLocaleRecord* out_record);

int rin_icu_collator_create(rin_icu_client_t* client, const char* locale, const rin_icu_collator_options_t* options, rin_icu_handle_t* out_handle);
int rin_icu_collator_compare(rin_icu_client_t* client, rin_icu_handle_t handle, const char* lhs, const char* rhs, int* out_result);
int rin_icu_collator_sort_key(rin_icu_client_t* client, rin_icu_handle_t handle, const char* input, uint8_t* dest, size_t dest_cap, size_t* out_len);
int rin_icu_collator_sort_keys_bulk(rin_icu_client_t* client, rin_icu_handle_t handle, const char* const* inputs, size_t input_count, uint8_t* dest, size_t dest_cap, size_t* out_len);
int rin_icu_collator_destroy(rin_icu_client_t* client, rin_icu_handle_t handle);

int rin_icu_segmenter_create(rin_icu_client_t* client, const char* locale, const rin_icu_segmenter_options_t* options, rin_icu_handle_t* out_handle);
int rin_icu_segmenter_reset(rin_icu_client_t* client, rin_icu_handle_t handle, const char* input);
int rin_icu_segmenter_next(rin_icu_client_t* client, rin_icu_handle_t handle, rin_icu_segment_t* out_segment, int* out_has_value);
int rin_icu_segmenter_destroy(rin_icu_client_t* client, rin_icu_handle_t handle);

int rin_icu_number_formatter_create(rin_icu_client_t* client, const char* locale, const rin_icu_number_formatter_options_t* options, rin_icu_handle_t* out_handle);
int rin_icu_number_formatter_create_v2(rin_icu_client_t* client, const char* locale, const rin_icu_number_formatter_options_v2_t* options, rin_icu_handle_t* out_handle);
int rin_icu_number_formatter_format(rin_icu_client_t* client, rin_icu_handle_t handle, double value, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_number_formatter_destroy(rin_icu_client_t* client, rin_icu_handle_t handle);

int rin_icu_datetime_formatter_create(rin_icu_client_t* client, const char* locale, const rin_icu_datetime_formatter_options_t* options, rin_icu_handle_t* out_handle);
int rin_icu_datetime_formatter_format_epoch_ms(rin_icu_client_t* client, rin_icu_handle_t handle, int64_t epoch_ms, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_datetime_formatter_destroy(rin_icu_client_t* client, rin_icu_handle_t handle);

int rin_icu_plural_rules_create(rin_icu_client_t* client, const char* locale, const rin_icu_plural_rules_options_t* options, rin_icu_handle_t* out_handle);
int rin_icu_plural_rules_select(rin_icu_client_t* client, rin_icu_handle_t handle, double value, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_plural_rules_destroy(rin_icu_client_t* client, rin_icu_handle_t handle);

int rin_icu_display_name(rin_icu_client_t* client, const char* locale, const char* code, uint32_t type, uint32_t style, uint32_t language_display, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_list_format(rin_icu_client_t* client, const char* locale, uint32_t type, uint32_t style, const char* const* items, size_t item_count, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_relative_time_format(rin_icu_client_t* client, const char* locale, uint32_t style, uint32_t numeric_display, uint32_t unit, double value, char* dest, size_t dest_cap, size_t* out_len);

int rin_icu_time_zone_current(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_time_zone_canonicalize(rin_icu_client_t* client, const char* time_zone, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_time_zone_available(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_time_zone_available_in_region(rin_icu_client_t* client, const char* region, char* dest, size_t dest_cap, size_t* out_len);
int rin_icu_time_zone_offset(rin_icu_client_t* client, const char* time_zone, int64_t epoch_ms, int* out_offset_minutes, int* out_in_dst);
int rin_icu_time_zone_transition(rin_icu_client_t* client, const char* time_zone, int64_t epoch_ms, uint32_t direction, uint32_t include_given_time, uint32_t transition_rule, int64_t* out_transition_epoch_ms);
#ifdef __cplusplus
}
#endif

#endif
