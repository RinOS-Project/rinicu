# RinICU TZDB blobs

The v1 `RITZDB1` header is 24 bytes and is followed by fixed 104-byte zone
records. Offsets are bounded to ±1440 minutes. The v2 extension uses a
`RITZDB2` 32-byte footer after the v1 zone records, then fixed 16-byte zone
metadata and fixed 32-byte transitions. Transition offsets are bounded to
±86400 seconds, transition counts are bounded by
`RIN_ICU_TZDB_MAX_TRANSITIONS`, and all reserved bytes must be zero.

The validator checks exact file length, footer version/record sizes, metadata
ranges, transition ranges, DST bits, and terminated zone/abbreviation fields.
Malformed blobs are rejected before they become the active catalog.

The generator also emits `rinicu_tzdata_version.txt` and
`rinicu_tzdb_leaps.txt` beside the blob. The version must be a five-character
IANA release identifier. Each leap-sidecar row is an effective POSIX midnight
in seconds followed by `1` or `-1`; rows are strictly increasing and capped at
128 entries. `rinicud` loads the version and signed leap table with the TZDB
blob and returns them with zone aliases in one catalog snapshot. The build and
media image must package all three files from the same generated directory.
