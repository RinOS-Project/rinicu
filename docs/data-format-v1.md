# RICUDB1 locale data

`RICUDB1` is the generated locale catalog format used by `rinicud`, not part of
RinOS-SDK. The header is 24 bytes, followed by `locale_count` fixed 380-byte
`RinIcuDataLocaleRecord` values. Integers are little-endian. `record_size` must
equal 380, `reserved0` must be zero, and the file length must equal the exact
header plus record calculation. Locale count is bounded by
`RIN_ICU_DATA_MAX_LOCALES` (currently 2048, which bounds the pinned CLDR 48.2
common/main inventory of 1122 locale files while keeping malformed inputs
bounded). The committed product projection remains a separate, smaller
compatibility scope and is not thereby promoted to full CLDR parity.

Every fixed string field must be terminated within its field and contain no
control byte. Plural rules, currency digits, flags, and all reserved values are
validated before the service installs a catalog. Generated data and the API
ABI have independent version numbers.

When `flags & RIN_ICU_DATA_FLAG_GROUPING_PRESENT` is set, bits 8-14 carry
the rightmost CLDR decimal group size and bits 16-22 carry the repeated
secondary group size. A zero/zero pair means that the locale's standard
decimal pattern is ungrouped. Records without the presence bit retain the
legacy reader fallback and are accepted for backward-compatible loading.

When `flags & RIN_ICU_DATA_FLAG_DECIMAL_DIGITS_PRESENT` is set, bits 24-27
carry the maximum fraction-digit count of the locale's CLDR standard decimal
pattern. The product formatter bounds this value to six. Records without the
presence bit retain the legacy reader fallback of three fractional digits;
older RICUDB1 snapshots therefore remain loadable without claiming that their
decimal precision was sourced from CLDR.

The high part of `week_data` carries a bounded CLDR default-numbering-system
projection. `RIN_ICU_DATA_NUMBERING_SYSTEM_PRESENT` and bits 12-16 identify
one of the 19 numeric systems listed in `data_blob.h`. A missing presence bit
means that the reader must use its legacy script fallback. A CLDR system that
has no RinICU digit table is not silently mapped to another system; generation
leaves the bit absent and records the unsupported projection count.
