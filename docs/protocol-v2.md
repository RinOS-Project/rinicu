# RinICU wire protocol v2

The wire protocol is a fixed-layout, little-endian protocol. All integer
fields use the exact-width types in `rin/icu/service_abi.h`; the records are
packed and have compile-time size/offset assertions. `value_bits` fields carry
IEEE-754 binary64 representations. A big-endian implementation must add an
encode/decode layer before it can speak v2.

Each message starts with a 32-byte `RinIcuMsgHeader`. `magic`, `version`,
command, request ID, handle ID, payload length, status, and flags are checked by
the client. Version is the wire protocol version, not a library version.
`RIN_ICU_MAGIC`, command numbers, status values, and enum values are frozen;
removed command numbers are never reused. Header flags and every reserved field
are sent as zero and rejected when received non-zero.

Inline payloads are limited to 64 KiB (`RIN_ICU_MAX_INLINE_PAYLOAD`), and bulk
operations are limited to `RIN_ICU_MAX_BULK_ITEMS`. Length arithmetic is checked
before allocation or pointer arithmetic. Successful responses have an exact
command-specific payload shape; failure responses have no payload.

Formatter handles are connection-scoped and handle ID zero is invalid. A close
invalidates every handle. The public client does not expose socket descriptors
or socket paths. The current path is an internal transport policy and can be
replaced without changing this ABI.
