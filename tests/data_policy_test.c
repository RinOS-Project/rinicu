/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>

#include <rinicu/data_policy.h>

static void make_locale(RinIcuDataLocaleRecord* record)
{
    memset(record, 0, sizeof(*record));
    strcpy(record->locale_id, "en-US");
    strcpy(record->language, "en");
    strcpy(record->region, "US");
    strcpy(record->currency_code, "USD");
    strcpy(record->currency_symbol, "$" );
    strcpy(record->decimal_sep, ".");
    strcpy(record->group_sep, ",");
    strcpy(record->plus_sign, "+");
    strcpy(record->minus_sign, "-");
    strcpy(record->percent_sign, "%");
    strcpy(record->am, "AM");
    strcpy(record->pm, "PM");
    strcpy(record->decimal_pattern, "{value}");
    strcpy(record->percent_pattern, "{value}{percent}");
    strcpy(record->currency_pattern, "{symbol}{value}");
    strcpy(record->date_pattern, "MM/DD/YYYY");
    strcpy(record->long_date_pattern, "MMMM DD, YYYY");
    strcpy(record->time_pattern, "hh:mm A");
    strcpy(record->datetime_pattern, "{date} {time}");
    record->cardinal_rule = RIN_ICU_PLURAL_RULE_ONE;
    record->ordinal_rule = RIN_ICU_PLURAL_RULE_ENGLISH_ORDINAL;
    record->currency_digits = 2u;
}

int main(void)
{
    RinIcuDataHeader data_header = {};
    RinIcuDataLocaleRecord locale;
    RinIcuTzdbHeader tz_header = {};
    RinIcuTzdbZoneRecord zone = {};
    RinIcuTzdbTransitionV1 transition = {};
    uint64_t data_size = sizeof(data_header) + sizeof(locale);
    uint64_t tz_size = sizeof(tz_header) + sizeof(zone);

    make_locale(&locale);
    memcpy(data_header.magic, RIN_ICU_DATA_MAGIC, sizeof(data_header.magic));
    data_header.version = RIN_ICU_DATA_VERSION;
    data_header.locale_count = 1u;
    data_header.record_size = sizeof(locale);
    assert(rin_icu_data_header_valid(&data_header, data_size));
    assert(rin_icu_data_locale_valid(&locale));

    data_header.locale_count = RIN_ICU_DATA_MAX_LOCALES + 1u;
    assert(!rin_icu_data_header_valid(&data_header, data_size));
    make_locale(&locale);
    memset(locale.locale_id, 'x', sizeof(locale.locale_id));
    assert(!rin_icu_data_locale_valid(&locale));
    make_locale(&locale);
    locale.reserved0 = 1u;
    assert(!rin_icu_data_locale_valid(&locale));
    make_locale(&locale);
    locale.cardinal_rule = RIN_ICU_PLURAL_RULE_ENGLISH_ORDINAL + 1u;
    assert(!rin_icu_data_locale_valid(&locale));
    make_locale(&locale);
    locale.currency_digits = 4u;
    assert(!rin_icu_data_locale_valid(&locale));
    assert(!rin_icu_data_u64_add(UINT64_MAX, 1u, &data_size));
    assert(!rin_icu_data_u64_mul(UINT64_MAX, 2u, &data_size));

    memcpy(tz_header.magic, RIN_ICU_TZDB_MAGIC, sizeof(tz_header.magic));
    tz_header.version = RIN_ICU_TZDB_VERSION;
    tz_header.zone_count = 1u;
    tz_header.record_size = sizeof(zone);
    strcpy(zone.zone_id, "UTC");
    strcpy(zone.canonical_id, "UTC");
    assert(rin_icu_tzdb_header_valid(&tz_header, tz_size));
    assert(rin_icu_tzdb_zone_valid(&zone));
    zone.offset_minutes = 1441;
    assert(!rin_icu_tzdb_zone_valid(&zone));

    transition.offset_seconds = 0;
    transition.is_dst = 0u;
    strcpy(transition.abbreviation, "UTC");
    assert(rin_icu_tzdb_transition_valid(&transition));
    transition.reserved[0] = 1u;
    assert(!rin_icu_tzdb_transition_valid(&transition));
    return 0;
}
