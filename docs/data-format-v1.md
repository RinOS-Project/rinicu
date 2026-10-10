# RICUDB1 locale data

`RICUDB1` is the generated locale catalog format used by `rinicud`, not part of
RinOS-SDK. The header is 24 bytes, followed by `locale_count` fixed 380-byte
`RinIcuDataLocaleRecord` values. Integers are little-endian. `record_size` must
equal 380, `reserved0` must be zero, and the file length must equal the exact
header plus record calculation. Locale count is bounded by
`RIN_ICU_DATA_MAX_LOCALES`.

Every fixed string field must be terminated within its field and contain no
control byte. Plural rules, currency digits, flags, and all reserved values are
validated before the service installs a catalog. Generated data and the API
ABI have independent version numbers.

When `flags & RIN_ICU_DATA_FLAG_GROUPING_PRESENT` is set, bits 8-14 carry
the rightmost CLDR decimal group size and bits 16-22 carry the repeated
secondary group size. A zero/zero pair means that the locale's standard
decimal pattern is ungrouped. Records without the presence bit retain the
legacy reader fallback and are accepted for backward-compatible loading.
