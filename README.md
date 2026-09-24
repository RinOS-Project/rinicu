# RinICU

RinICU is the public RinOS client library for the `rinicu` service. It uses
wire protocol v2 over the current local-socket transport and has no dependency
on OS-Core private headers. The service implementation remains in OS-Core.

Include the public API with:

```c
#include <rinicu/rinicu.h>
```

The public protocol ABI is supplied by RinOS-SDK:

```c
#include <rin/icu/service_abi.h>
```

Build with CMake or Meson. A standalone build needs a POSIX socket toolchain
and a checkout of `RinOS-SDK`; set `RINICU_SDK_INCLUDE_DIR` (CMake) or
`-Drinicu_sdk_include=...` (Meson) when the SDK is not at
`../../RinOS-SDK/include`.

`rin_icu_client_t` is caller-owned opaque storage. Call
`rin_icu_client_init()` before `rin_icu_client_open()` when the storage is not
zero-initialized. Handles are connection-scoped: closing a client invalidates
all handles, and a reconnect never revalidates an old handle. Destroying an
already destroyed handle returns `RIN_ICU_STATUS_BAD_HANDLE`.

Autostart is opt-in at build time (`RINICU_ENABLE_AUTOSTART`); when enabled it
uses the public SDK service-manager wrapper and the executable ID `rinicud`.
The public service discovery ID is `RIN_ICU_SERVICE_ID` (`rinicu`).

The ordinary application API does not expose timezone catalog reload. That is
an OS-Core administration operation; the wire command number remains reserved
and is never reused.

## Public API contract

| Requirement | Contract |
| --- | --- |
| Purpose | Public client for the local rinicu service using protocol v2 and public SDK service ABI. |
| Supported API | include/rinicu/rinicu.h/.hpp plus data_blob.h, tzdb_blob.h, and data_policy.h. |
| Unsupported API | Application clients cannot reload timezone catalogs; full ICU API parity and service administration are outside this client. |
| ownership | Caller owns client storage and initializes before open; closing invalidates all connection-scoped handles. |
| thread-safety | Use a client from one serialized owner at a time unless a specific API permits concurrent calls. |
| limits | Protocol records and data accesses are bounded by RinOS-SDK ABI and declared blob policies. |
| errors | RinIcuStatus reports invalid handles, missing data, transport/protocol errors, and service failures. |
| ABI stability | Wire protocol ABI v2 is defined by RinOS-SDK; records are versioned. |
| security | Uses local service boundary and exposes no private service implementation or admin reload authority. |
| build | Use repository CMake or Meson and supply RinOS-SDK include paths as described above. |
| test | Run tests through configured CMake/CTest or Meson CI. No tests/builds run for this README update. |
