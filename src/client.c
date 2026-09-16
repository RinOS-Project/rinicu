/* SPDX-License-Identifier: MIT */
#include "client_internal.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#if RIN_ICU_ENABLE_AUTOSTART
#include <time.h>
#endif
#include <unistd.h>

#ifndef RIN_ICU_ENABLE_AUTOSTART
#define RIN_ICU_ENABLE_AUTOSTART 0
#endif

#if RIN_ICU_ENABLE_AUTOSTART
#include <rin/service.h>
#endif

#if RIN_ICU_ENABLE_AUTOSTART
static const unsigned kRinIcuConnectRetryMs = 25u;
static const unsigned kRinIcuConnectRetryBudgetMs = 250u;

static void rin_icu_sleep_ms(unsigned milliseconds)
{
#if defined(_RIN_UNISTD_SLEEP)
    _RIN_UNISTD_SLEEP(milliseconds);
#else
    struct timespec request;
    request.tv_sec = (time_t)(milliseconds / 1000u);
    request.tv_nsec = (long)((milliseconds % 1000u) * 1000000u);
    while (nanosleep(&request, &request) < 0 && errno == EINTR) {
    }
#endif
}
#endif

static void rin_icu_reset_client(rin_icu_client_t* client)
{
    rin_icu_client_internal_t* internal;
    if (!client) {
        return;
    }
    memset(client, 0, sizeof(*client));
    internal = rin_icu_client_internal(client);
    internal->state_magic = RIN_ICU_CLIENT_STATE_MAGIC;
    internal->fd = -1;
    internal->next_request_id = 1u;
    internal->reserved0 = 0u;
}

static size_t rin_icu_strlen_c(const char* s)
{
    size_t len = 0u;
    if (!s) {
        return 0u;
    }
    while (len < RIN_ICU_MAX_CSTRING_BYTES && s[len] != '\0') {
        ++len;
    }
    if (len == RIN_ICU_MAX_CSTRING_BYTES) return len + 1u;
    return len;
}

static int rin_icu_connect_once(void)
{
    struct sockaddr_un addr;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, RIN_ICU_SOCKET_PATH, sizeof(addr.sun_path) - 1u);

    if (connect(fd, (const struct sockaddr*)&addr, (socklen_t)sizeof(addr)) == 0) {
        return fd;
    }

    close(fd);
    return -1;
}

static int rin_icu_connect_with_autostart(void)
{
    int fd = rin_icu_connect_once();
    if (fd >= 0) {
        return fd;
    }

#if !RIN_ICU_ENABLE_AUTOSTART
    return -1;
#else
    unsigned waited_ms = 0u;
    if (rin_service_start(RIN_SERVICE_SCOPE_SYSTEM,
                          RIN_ICU_SERVICE_EXECUTABLE_ID) < 0) {
        return -1;
    }

    for (;;) {
        fd = rin_icu_connect_once();
        if (fd >= 0) {
            return fd;
        }
        if (waited_ms >= kRinIcuConnectRetryBudgetMs) {
            break;
        }
        rin_icu_sleep_ms(kRinIcuConnectRetryMs);
        waited_ms += kRinIcuConnectRetryMs;
    }

    return -1;
#endif
}

static int rin_icu_size_to_u32(size_t value, uint32_t* out)
{
    if (!out || value > (size_t)UINT32_MAX) return RIN_ICU_STATUS_TOO_LARGE;
    *out = (uint32_t)value;
    return RIN_ICU_STATUS_OK;
}

static int rin_icu_add_size(size_t left, size_t right, size_t* out)
{
    if (!out || right > SIZE_MAX - left) return RIN_ICU_STATUS_TOO_LARGE;
    *out = left + right;
    return RIN_ICU_STATUS_OK;
}

static int rin_icu_mul_size(size_t left, size_t right, size_t* out)
{
    if (!out || (left != 0u && right > SIZE_MAX / left)) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    *out = left * right;
    return RIN_ICU_STATUS_OK;
}

static int rin_icu_payload_size2(size_t first, size_t second, uint32_t* out)
{
    size_t total;
    int status = rin_icu_add_size(first, second, &total);
    if (status != RIN_ICU_STATUS_OK || total > RIN_ICU_MAX_INLINE_PAYLOAD) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    return rin_icu_size_to_u32(total, out);
}

static int rin_icu_payload_size3(size_t first, size_t second, size_t third,
                                 uint32_t* out)
{
    size_t partial;
    int status = rin_icu_add_size(first, second, &partial);
    if (status != RIN_ICU_STATUS_OK) return status;
    return rin_icu_payload_size2(partial, third, out);
}

static int rin_icu_status_valid(int32_t status)
{
    switch (status) {
    case RIN_ICU_STATUS_OK:
    case RIN_ICU_STATUS_INVALID:
    case RIN_ICU_STATUS_UNSUPPORTED:
    case RIN_ICU_STATUS_IO_ERROR:
    case RIN_ICU_STATUS_VERSION_MISMATCH:
    case RIN_ICU_STATUS_TOO_LARGE:
    case RIN_ICU_STATUS_BAD_HANDLE:
    case RIN_ICU_STATUS_NO_SPACE:
    case RIN_ICU_STATUS_DATA_ERROR:
        return 1;
    default:
        return 0;
    }
}

static int rin_icu_command_is_create(uint32_t command)
{
    return command == RIN_ICU_CMD_COLLATOR_CREATE_V1 ||
           command == RIN_ICU_CMD_SEGMENTER_CREATE_V1 ||
           command == RIN_ICU_CMD_NUMBER_FORMATTER_CREATE_V1 ||
           command == RIN_ICU_CMD_DATETIME_FORMATTER_CREATE_V1 ||
           command == RIN_ICU_CMD_PLURAL_RULES_CREATE_V1;
}

static uint32_t rin_icu_next_request_id(rin_icu_client_internal_t* internal)
{
    uint32_t request_id;
    if (!internal) return 0u;
    request_id = internal->next_request_id;
    if (request_id == 0u) request_id = 1u;
    internal->next_request_id = request_id + 1u;
    if (internal->next_request_id == 0u) internal->next_request_id = 1u;
    return request_id;
}

static int rin_icu_response_handle_valid(uint32_t command,
                                         uint32_t request_handle,
                                         uint32_t response_handle)
{
    if (rin_icu_command_is_create(command)) return response_handle != 0u;
    if (command == RIN_ICU_CMD_COLLATOR_COMPARE_V1 ||
        command == RIN_ICU_CMD_COLLATOR_SORT_KEY_V1 ||
        command == RIN_ICU_CMD_COLLATOR_SORT_KEYS_V1 ||
        command == RIN_ICU_CMD_SEGMENTER_RESET_V1 ||
        command == RIN_ICU_CMD_SEGMENTER_NEXT_V1 ||
        command == RIN_ICU_CMD_NUMBER_FORMAT_V1 ||
        command == RIN_ICU_CMD_DATETIME_FORMAT_EPOCH_MS_V1 ||
        command == RIN_ICU_CMD_PLURAL_RULES_SELECT_V1) {
        return response_handle == request_handle && request_handle != 0u;
    }
    return response_handle == 0u;
}

static int rin_icu_bytes_response_shape(const unsigned char* payload,
                                        uint32_t payload_len)
{
    const RinIcuBytesResponse* response;
    size_t total;
    if (!payload || payload_len < sizeof(RinIcuBytesResponse)) return 0;
    response = (const RinIcuBytesResponse*)payload;
    if (rin_icu_add_size(sizeof(*response), (size_t)response->data_len,
                         &total) != RIN_ICU_STATUS_OK) return 0;
    return total == payload_len;
}

static int rin_icu_bulk_bytes_response_shape(const unsigned char* payload,
                                             uint32_t payload_len)
{
    const RinIcuBytesResponse* outer;
    const RinIcuBulkBytesResponse* inner;
    size_t lengths_bytes;
    size_t minimum;
    if (!rin_icu_bytes_response_shape(payload, payload_len)) return 0;
    outer = (const RinIcuBytesResponse*)payload;
    if (outer->data_len < sizeof(RinIcuBulkBytesResponse)) return 0;
    inner = (const RinIcuBulkBytesResponse*)(payload + sizeof(*outer));
    if (inner->item_count > RIN_ICU_MAX_BULK_ITEMS || inner->reserved0 != 0u)
        return 0;
    if (rin_icu_mul_size((size_t)inner->item_count, sizeof(uint32_t),
                         &lengths_bytes) != RIN_ICU_STATUS_OK ||
        rin_icu_add_size(sizeof(*inner), lengths_bytes, &minimum) !=
            RIN_ICU_STATUS_OK ||
        minimum > outer->data_len)
        return 0;
    {
        const uint32_t* lengths =
            (const uint32_t*)(payload + sizeof(*outer) + sizeof(*inner));
        size_t bytes = minimum;
        size_t i;
        for (i = 0u; i < inner->item_count; ++i) {
            if (rin_icu_add_size(bytes, (size_t)lengths[i], &bytes) !=
                RIN_ICU_STATUS_OK || bytes > outer->data_len) return 0;
        }
        return bytes == outer->data_len;
    }
}

static int rin_icu_response_payload_valid(uint32_t command,
                                          int32_t status,
                                          const unsigned char* payload,
                                          uint32_t payload_len)
{
    if (status != RIN_ICU_STATUS_OK) return payload_len == 0u;
    switch (command) {
    case RIN_ICU_CMD_LOCALE_INFO_V1:
        return payload_len == sizeof(RinIcuDataLocaleRecord);
    case RIN_ICU_CMD_COLLATOR_COMPARE_V1:
        return payload_len == sizeof(RinIcuCompareResponse) && payload &&
               ((const RinIcuCompareResponse*)payload)->reserved0 == 0u;
    case RIN_ICU_CMD_COLLATOR_SORT_KEYS_V1:
        return rin_icu_bulk_bytes_response_shape(payload, payload_len);
    case RIN_ICU_CMD_SEGMENTER_NEXT_V1:
        return payload_len == sizeof(RinIcuSegmentNextResponse) && payload &&
               ((const RinIcuSegmentNextResponse*)payload)->has_value <= 1u &&
               ((const RinIcuSegmentNextResponse*)payload)->start <=
                   ((const RinIcuSegmentNextResponse*)payload)->end &&
               ((((const RinIcuSegmentNextResponse*)payload)->flags &
                 ~(uint32_t)(RIN_ICU_SEGMENT_FLAG_WORD_LIKE |
                             RIN_ICU_SEGMENT_FLAG_SOFT_BREAK |
                             RIN_ICU_SEGMENT_FLAG_HARD_BREAK)) == 0u);
    case RIN_ICU_CMD_TIME_ZONE_OFFSET_V1:
        return payload_len == sizeof(RinIcuTimeZoneOffsetResponse) && payload &&
               ((const RinIcuTimeZoneOffsetResponse*)payload)->in_dst <= 1u;
    case RIN_ICU_CMD_TIME_ZONE_TRANSITION_V1:
        return payload_len == sizeof(RinIcuTimeZoneTransitionResponse) && payload;
    case RIN_ICU_CMD_COLLATOR_CREATE_V1:
    case RIN_ICU_CMD_SEGMENTER_CREATE_V1:
    case RIN_ICU_CMD_NUMBER_FORMATTER_CREATE_V1:
    case RIN_ICU_CMD_DATETIME_FORMATTER_CREATE_V1:
    case RIN_ICU_CMD_PLURAL_RULES_CREATE_V1:
    case RIN_ICU_CMD_DESTROY_HANDLE_V1:
    case RIN_ICU_CMD_TIME_ZONE_RELOAD_V1:
        return payload_len == 0u;
    default:
        return rin_icu_bytes_response_shape(payload, payload_len);
    }
}

static int rin_icu_send_all(int fd, const void* data, size_t len)
{
    const unsigned char* ptr = (const unsigned char*)data;
    size_t sent = 0u;
    while (sent < len) {
        ssize_t rc = send(fd, ptr + sent, len - sent, 0);
        if (rc <= 0) {
            return -1;
        }
        sent += (size_t)rc;
    }
    return 0;
}

static int rin_icu_recv_all(int fd, void* data, size_t len)
{
    unsigned char* ptr = (unsigned char*)data;
    size_t received = 0u;
    while (received < len) {
        ssize_t rc = recv(fd, ptr + received, len - received, 0);
        if (rc <= 0) {
            return -1;
        }
        received += (size_t)rc;
    }
    return 0;
}

static uint64_t rin_icu_double_to_bits(double value)
{
    union {
        double d;
        uint64_t u;
    } conv;
    conv.d = value;
    return conv.u;
}

static int rin_icu_client_call(rin_icu_client_t* client,
                               uint32_t command,
                               uint32_t handle_id,
                               uint32_t flags,
                               const void* payload,
                               uint32_t payload_len,
                               RinIcuMsgHeader* out_header,
                               unsigned char** out_payload)
{
    rin_icu_client_internal_t* internal;
    RinIcuMsgHeader header;
    RinIcuMsgHeader response;
    unsigned char* response_payload = NULL;
    uint32_t request_id;

    internal = rin_icu_client_internal(client);
    if (!internal || internal->state_magic != RIN_ICU_CLIENT_STATE_MAGIC ||
        internal->fd < 0) {
        return RIN_ICU_STATUS_INVALID;
    }
    if (payload_len > RIN_ICU_MAX_INLINE_PAYLOAD) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    if (payload_len > 0u && !payload) {
        return RIN_ICU_STATUS_INVALID;
    }

    request_id = rin_icu_next_request_id(internal);

    memset(&header, 0, sizeof(header));
    header.magic = RIN_ICU_MAGIC;
    header.version = RIN_ICU_VERSION;
    header.command = command;
    header.request_id = request_id;
    header.handle_id = handle_id;
    header.payload_len = payload_len;
    header.flags = flags;

    if (rin_icu_send_all(internal->fd, &header, sizeof(header)) != 0 ||
        (payload_len > 0u && rin_icu_send_all(internal->fd, payload, payload_len) != 0)) {
        rin_icu_client_close(client);
        return RIN_ICU_STATUS_IO_ERROR;
    }

    if (rin_icu_recv_all(internal->fd, &response, sizeof(response)) != 0) {
        rin_icu_client_close(client);
        return RIN_ICU_STATUS_IO_ERROR;
    }

    if (response.magic != RIN_ICU_MAGIC ||
        response.version != RIN_ICU_VERSION) {
        rin_icu_client_close(client);
        return RIN_ICU_STATUS_VERSION_MISMATCH;
    }

    if (response.command != command ||
        response.request_id != request_id ||
        response.payload_len > RIN_ICU_MAX_INLINE_PAYLOAD ||
        response.flags & ~RIN_ICU_MSG_FLAGS_KNOWN ||
        !rin_icu_status_valid(response.status) ||
        (response.status == RIN_ICU_STATUS_OK
             ? !rin_icu_response_handle_valid(command, handle_id, response.handle_id)
             : response.handle_id != 0u)) {
        rin_icu_client_close(client);
        return RIN_ICU_STATUS_DATA_ERROR;
    }

    if (response.payload_len > 0u) {
        response_payload = (unsigned char*)malloc((size_t)response.payload_len);
        if (!response_payload) {
            rin_icu_client_close(client);
            return RIN_ICU_STATUS_IO_ERROR;
        }
        if (rin_icu_recv_all(internal->fd, response_payload, response.payload_len) != 0) {
            free(response_payload);
            rin_icu_client_close(client);
            return RIN_ICU_STATUS_IO_ERROR;
        }
    }

    if (!rin_icu_response_payload_valid(command,
                                        response.status,
                                        response_payload,
                                        response.payload_len)) {
        free(response_payload);
        rin_icu_client_close(client);
        return RIN_ICU_STATUS_DATA_ERROR;
    }

    if (out_header) {
        *out_header = response;
    }
    if (out_payload) {
        *out_payload = response_payload;
    } else if (response_payload) {
        free(response_payload);
    }
    return response.status;
}

static int rin_icu_copy_text_response(const unsigned char* payload,
                                      uint32_t payload_len,
                                      char* dest,
                                      size_t dest_cap,
                                      size_t* out_len)
{
    const RinIcuBytesResponse* response = (const RinIcuBytesResponse*)payload;
    size_t data_len;
    if (!rin_icu_bytes_response_shape(payload, payload_len)) {
        return RIN_ICU_STATUS_DATA_ERROR;
    }
    data_len = (size_t)response->data_len;
    if (out_len) {
        *out_len = data_len;
    }
    if (!dest) {
        return RIN_ICU_STATUS_OK;
    }
    if (dest_cap <= data_len) {
        return RIN_ICU_STATUS_NO_SPACE;
    }
    if (data_len > 0u) {
        memcpy(dest, payload + sizeof(RinIcuBytesResponse), data_len);
    }
    dest[data_len] = '\0';
    return RIN_ICU_STATUS_OK;
}

static int rin_icu_copy_bytes_response(const unsigned char* payload,
                                       uint32_t payload_len,
                                       uint8_t* dest,
                                       size_t dest_cap,
                                       size_t* out_len)
{
    const RinIcuBytesResponse* response = (const RinIcuBytesResponse*)payload;
    size_t data_len;
    if (!rin_icu_bytes_response_shape(payload, payload_len)) {
        return RIN_ICU_STATUS_DATA_ERROR;
    }
    data_len = (size_t)response->data_len;
    if (out_len) {
        *out_len = data_len;
    }
    if (!dest) {
        return RIN_ICU_STATUS_OK;
    }
    if (dest_cap < data_len) {
        return RIN_ICU_STATUS_NO_SPACE;
    }
    if (data_len > 0u) {
        memcpy(dest, payload + sizeof(RinIcuBytesResponse), data_len);
    }
    return RIN_ICU_STATUS_OK;
}

static int rin_icu_call_text_command(rin_icu_client_t* client,
                                     uint32_t command,
                                     uint32_t handle_id,
                                     const void* request,
                                     uint32_t request_len,
                                     char* dest,
                                     size_t dest_cap,
                                     size_t* out_len)
{
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    int status = rin_icu_client_call(client,
                                     command,
                                     handle_id,
                                     0u,
                                     request,
                                     request_len,
                                     &response,
                                     &payload);
    if (status == RIN_ICU_STATUS_OK) {
        status = rin_icu_copy_text_response(payload, response.payload_len, dest, dest_cap, out_len);
    }
    free(payload);
    return status;
}

static int rin_icu_call_text_no_payload(rin_icu_client_t* client,
                                        uint32_t command,
                                        char* dest,
                                        size_t dest_cap,
                                        size_t* out_len)
{
    return rin_icu_call_text_command(client, command, 0u, NULL, 0u, dest, dest_cap, out_len);
}

static int rin_icu_call_text_input(rin_icu_client_t* client,
                                   uint32_t handle_id,
                                   uint32_t command,
                                   const char* input,
                                   char* dest,
                                   size_t dest_cap,
                                   size_t* out_len)
{
    size_t input_len = rin_icu_strlen_c(input);
    uint32_t payload_len;
    unsigned char* payload;
    int status;
    if (rin_icu_payload_size2(sizeof(RinIcuTextRequest), input_len,
                              &payload_len) != RIN_ICU_STATUS_OK) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }

    ((RinIcuTextRequest*)payload)->text_len = (uint32_t)input_len;
    if (input_len > 0u) {
        memcpy(payload + sizeof(RinIcuTextRequest), input, input_len);
    }

    status = rin_icu_call_text_command(client,
                                       command,
                                       handle_id,
                                       payload,
                                       payload_len,
                                       dest,
                                       dest_cap,
                                       out_len);
    free(payload);
    return status;
}

static int rin_icu_call_bytes_input(rin_icu_client_t* client,
                                    uint32_t handle_id,
                                    uint32_t command,
                                    const char* input,
                                    uint8_t* dest,
                                    size_t dest_cap,
                                    size_t* out_len)
{
    size_t input_len = rin_icu_strlen_c(input);
    uint32_t payload_len;
    unsigned char* request_payload;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    int status;
    if (rin_icu_payload_size2(sizeof(RinIcuTextRequest), input_len,
                              &payload_len) != RIN_ICU_STATUS_OK) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }

    ((RinIcuTextRequest*)request_payload)->text_len = (uint32_t)input_len;
    if (input_len > 0u) {
        memcpy(request_payload + sizeof(RinIcuTextRequest), input, input_len);
    }

    status = rin_icu_client_call(client,
                                 command,
                                 handle_id,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    if (status == RIN_ICU_STATUS_OK) {
        status = rin_icu_copy_bytes_response(payload, response.payload_len, dest, dest_cap, out_len);
    }

    free(payload);
    free(request_payload);
    return status;
}

static int rin_icu_call_bytes_command(rin_icu_client_t* client,
                                      uint32_t command,
                                      uint32_t handle_id,
                                      const void* request,
                                      uint32_t request_len,
                                      uint8_t* dest,
                                      size_t dest_cap,
                                      size_t* out_len)
{
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    int status = rin_icu_client_call(client,
                                     command,
                                     handle_id,
                                     0u,
                                     request,
                                     request_len,
                                     &response,
                                     &payload);
    if (status == RIN_ICU_STATUS_OK) {
        status = rin_icu_copy_bytes_response(payload, response.payload_len, dest, dest_cap, out_len);
    }
    free(payload);
    return status;
}

static int rin_icu_destroy_handle(rin_icu_client_t* client, rin_icu_handle_t handle)
{
    RinIcuMsgHeader response;
    return rin_icu_client_call(client,
                               RIN_ICU_CMD_DESTROY_HANDLE_V1,
                               handle,
                               0u,
                               NULL,
                               0u,
                               &response,
                               NULL);
}

void rin_icu_client_init(rin_icu_client_t* client)
{
    if (client) rin_icu_reset_client(client);
}

int rin_icu_client_open(rin_icu_client_t* client)
{
    rin_icu_client_internal_t* internal;
    if (!client) return RIN_ICU_STATUS_INVALID;
    internal = rin_icu_client_internal(client);
    if (!internal || internal->state_magic != RIN_ICU_CLIENT_STATE_MAGIC) {
        rin_icu_client_init(client);
        internal = rin_icu_client_internal(client);
    }
    if (internal->fd >= 0) return RIN_ICU_STATUS_OK;
    internal->fd = rin_icu_connect_with_autostart();
    if (internal->fd < 0) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    return RIN_ICU_STATUS_OK;
}

void rin_icu_client_close(rin_icu_client_t* client)
{
    rin_icu_client_internal_t* internal;
    if (!client) {
        return;
    }
    internal = rin_icu_client_internal(client);
    if (!internal || internal->state_magic != RIN_ICU_CLIENT_STATE_MAGIC) {
        rin_icu_client_init(client);
        return;
    }
    if (internal->fd >= 0) close(internal->fd);
    rin_icu_reset_client(client);
}

int rin_icu_client_is_open(const rin_icu_client_t* client)
{
    const rin_icu_client_internal_t* internal = rin_icu_client_internal_const(client);
    return internal && internal->state_magic == RIN_ICU_CLIENT_STATE_MAGIC &&
           internal->fd >= 0;
}

int rin_icu_locale_canonicalize(rin_icu_client_t* client,
                                const char* input,
                                char* dest,
                                size_t dest_cap,
                                size_t* out_len)
{
    return rin_icu_call_text_input(client, 0u, RIN_ICU_CMD_LOCALE_CANONICALIZE_V1, input ? input : "", dest, dest_cap, out_len);
}

int rin_icu_locale_resolve(rin_icu_client_t* client,
                           const char* input,
                           char* dest,
                           size_t dest_cap,
                           size_t* out_len)
{
    return rin_icu_call_text_input(client, 0u, RIN_ICU_CMD_LOCALE_RESOLVE_V1, input ? input : "", dest, dest_cap, out_len);
}

int rin_icu_locale_maximize(rin_icu_client_t* client,
                            const char* input,
                            char* dest,
                            size_t dest_cap,
                            size_t* out_len)
{
    return rin_icu_call_text_input(client, 0u, RIN_ICU_CMD_LOCALE_MAXIMIZE_V1, input ? input : "", dest, dest_cap, out_len);
}

int rin_icu_locale_minimize(rin_icu_client_t* client,
                            const char* input,
                            char* dest,
                            size_t dest_cap,
                            size_t* out_len)
{
    return rin_icu_call_text_input(client, 0u, RIN_ICU_CMD_LOCALE_MINIMIZE_V1, input ? input : "", dest, dest_cap, out_len);
}

int rin_icu_locale_available(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len)
{
    return rin_icu_call_text_no_payload(client, RIN_ICU_CMD_LOCALE_AVAILABLE_V1, dest, dest_cap, out_len);
}

int rin_icu_locale_preferred(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len)
{
    return rin_icu_call_text_no_payload(client, RIN_ICU_CMD_LOCALE_PREFERRED_V1, dest, dest_cap, out_len);
}

int rin_icu_locale_info(rin_icu_client_t* client,
                        const char* input,
                        RinIcuDataLocaleRecord* out_record)
{
    RinIcuTextRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t input_len = rin_icu_strlen_c(input);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;

    if (!out_record || rin_icu_payload_size2(sizeof(request), input_len,
                                             &payload_len) != RIN_ICU_STATUS_OK) {
        return RIN_ICU_STATUS_INVALID;
    }

    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    request.text_len = (uint32_t)input_len;
    memcpy(request_payload, &request, sizeof(request));
    if (input_len > 0u) {
        memcpy(request_payload + sizeof(request), input, input_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_LOCALE_INFO_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    if (status == RIN_ICU_STATUS_OK) {
        if (response.payload_len != sizeof(RinIcuDataLocaleRecord) || !payload) {
            status = RIN_ICU_STATUS_DATA_ERROR;
        } else {
            memcpy(out_record, payload, sizeof(*out_record));
        }
    }
    free(payload);
    return status;
}

int rin_icu_normalize(rin_icu_client_t* client,
                      int form,
                      const char* input,
                      char* dest,
                      size_t dest_cap,
                      size_t* out_len)
{
    size_t input_len = rin_icu_strlen_c(input);
    uint32_t payload_len;
    unsigned char* payload;
    int status;
    if (rin_icu_payload_size2(sizeof(RinIcuNormalizeRequest), input_len,
                              &payload_len) != RIN_ICU_STATUS_OK) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    ((RinIcuNormalizeRequest*)payload)->form = (uint32_t)form;
    ((RinIcuNormalizeRequest*)payload)->text_len = (uint32_t)input_len;
    if (input_len > 0u) {
        memcpy(payload + sizeof(RinIcuNormalizeRequest), input ? input : "", input_len);
    }
    status = rin_icu_call_text_command(client,
                                       RIN_ICU_CMD_NORMALIZE_V1,
                                       0u,
                                       payload,
                                       payload_len,
                                       dest,
                                       dest_cap,
                                       out_len);
    free(payload);
    return status;
}

int rin_icu_collator_create(rin_icu_client_t* client,
                            const char* locale,
                            const rin_icu_collator_options_t* options,
                            rin_icu_handle_t* out_handle)
{
    RinIcuCollatorCreateRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t locale_len = rin_icu_strlen_c(locale);
    uint32_t payload_len;
    int status;
    unsigned char* request_payload;
    if (rin_icu_payload_size2(sizeof(request), locale_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload || !out_handle) {
        free(request_payload);
        return RIN_ICU_STATUS_INVALID;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    if (options) {
        request.options = *options;
    } else {
        request.options.strength = RIN_ICU_COLLATION_STRENGTH_TERTIARY;
    }
    memcpy(request_payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(request_payload + sizeof(request), locale, locale_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_COLLATOR_CREATE_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    free(payload);
    if (status == RIN_ICU_STATUS_OK) {
        *out_handle = response.handle_id;
    }
    return status;
}

int rin_icu_collator_compare(rin_icu_client_t* client,
                             rin_icu_handle_t handle,
                             const char* lhs,
                             const char* rhs,
                             int* out_result)
{
    RinIcuCompareRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t lhs_len = rin_icu_strlen_c(lhs);
    size_t rhs_len = rin_icu_strlen_c(rhs);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size3(sizeof(request), lhs_len, rhs_len,
                              &payload_len) != RIN_ICU_STATUS_OK) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload || !out_result) {
        free(request_payload);
        return RIN_ICU_STATUS_INVALID;
    }
    request.lhs_len = (uint32_t)lhs_len;
    request.rhs_len = (uint32_t)rhs_len;
    memcpy(request_payload, &request, sizeof(request));
    if (lhs_len > 0u) {
        memcpy(request_payload + sizeof(request), lhs ? lhs : "", lhs_len);
    }
    if (rhs_len > 0u) {
        memcpy(request_payload + sizeof(request) + lhs_len, rhs ? rhs : "", rhs_len);
    }

    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_COLLATOR_COMPARE_V1,
                                 handle,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    if (status == RIN_ICU_STATUS_OK) {
        if (!payload || response.payload_len < sizeof(RinIcuCompareResponse)) {
            status = RIN_ICU_STATUS_DATA_ERROR;
        } else {
            *out_result = ((const RinIcuCompareResponse*)payload)->result;
        }
    }
    free(payload);
    return status;
}

int rin_icu_collator_sort_key(rin_icu_client_t* client,
                              rin_icu_handle_t handle,
                              const char* input,
                              uint8_t* dest,
                              size_t dest_cap,
                              size_t* out_len)
{
    return rin_icu_call_bytes_input(client,
                                    handle,
                                    RIN_ICU_CMD_COLLATOR_SORT_KEY_V1,
                                    input ? input : "",
                                    dest,
                                    dest_cap,
                                    out_len);
}

int rin_icu_collator_sort_keys_bulk(rin_icu_client_t* client,
                                    rin_icu_handle_t handle,
                                    const char* const* inputs,
                                    size_t input_count,
                                    uint8_t* dest,
                                    size_t dest_cap,
                                    size_t* out_len)
{
    RinIcuBulkTextRequest request;
    size_t lengths_bytes;
    size_t text_bytes = 0u;
    size_t i;
    uint32_t payload_len;
    unsigned char* request_payload;

    if (input_count > RIN_ICU_MAX_BULK_ITEMS ||
        (input_count > 0u && !inputs)) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }

    if (rin_icu_mul_size(input_count, sizeof(uint32_t), &lengths_bytes) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;

    for (i = 0u; i < input_count; ++i) {
        size_t item_len = rin_icu_strlen_c(inputs[i]);
        if (rin_icu_add_size(text_bytes, item_len, &text_bytes) !=
            RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    }

    if (rin_icu_payload_size3(sizeof(request), lengths_bytes, text_bytes,
                              &payload_len) != RIN_ICU_STATUS_OK)
        return RIN_ICU_STATUS_TOO_LARGE;

    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }

    memset(&request, 0, sizeof(request));
    request.item_count = (uint32_t)input_count;
    memcpy(request_payload, &request, sizeof(request));

    {
        uint32_t* lengths = (uint32_t*)(request_payload + sizeof(request));
        unsigned char* text_ptr = request_payload + sizeof(request) + lengths_bytes;
        for (i = 0u; i < input_count; ++i) {
            size_t item_len = rin_icu_strlen_c(inputs[i]);
            lengths[i] = (uint32_t)item_len;
            if (item_len > 0u) {
                memcpy(text_ptr, inputs[i], item_len);
                text_ptr += item_len;
            }
        }
    }

    {
        int status = rin_icu_call_bytes_command(client,
                                                RIN_ICU_CMD_COLLATOR_SORT_KEYS_V1,
                                                handle,
                                                request_payload,
                                                payload_len,
                                                dest,
                                                dest_cap,
                                                out_len);
        free(request_payload);
        return status;
    }
}

int rin_icu_collator_destroy(rin_icu_client_t* client, rin_icu_handle_t handle)
{
    return rin_icu_destroy_handle(client, handle);
}

int rin_icu_segmenter_create(rin_icu_client_t* client,
                             const char* locale,
                             const rin_icu_segmenter_options_t* options,
                             rin_icu_handle_t* out_handle)
{
    RinIcuSegmenterCreateRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t locale_len = rin_icu_strlen_c(locale);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), locale_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload || !out_handle) {
        free(request_payload);
        return RIN_ICU_STATUS_INVALID;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    if (options) {
        request.options = *options;
    } else {
        request.options.kind = RIN_ICU_SEGMENTATION_GRAPHEME;
    }
    memcpy(request_payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(request_payload + sizeof(request), locale, locale_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_SEGMENTER_CREATE_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    free(payload);
    if (status == RIN_ICU_STATUS_OK) {
        *out_handle = response.handle_id;
    }
    return status;
}

int rin_icu_segmenter_reset(rin_icu_client_t* client, rin_icu_handle_t handle, const char* input)
{
    size_t input_len = rin_icu_strlen_c(input);
    uint32_t payload_len;
    unsigned char* payload;
    RinIcuSegmentResetRequest request;
    RinIcuMsgHeader response;
    int status;
    if (rin_icu_payload_size2(sizeof(RinIcuSegmentResetRequest), input_len,
                              &payload_len) != RIN_ICU_STATUS_OK) {
        return RIN_ICU_STATUS_TOO_LARGE;
    }
    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    request.text_len = (uint32_t)input_len;
    memcpy(payload, &request, sizeof(request));
    if (input_len > 0u) {
        memcpy(payload + sizeof(request), input ? input : "", input_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_SEGMENTER_RESET_V1,
                                 handle,
                                 0u,
                                 payload,
                                 payload_len,
                                 &response,
                                 NULL);
    free(payload);
    return status;
}

int rin_icu_segmenter_next(rin_icu_client_t* client,
                           rin_icu_handle_t handle,
                           rin_icu_segment_t* out_segment,
                           int* out_has_value)
{
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    int status = rin_icu_client_call(client,
                                     RIN_ICU_CMD_SEGMENTER_NEXT_V1,
                                     handle,
                                     0u,
                                     NULL,
                                     0u,
                                     &response,
                                     &payload);
    if (status == RIN_ICU_STATUS_OK) {
        if (!payload || response.payload_len < sizeof(RinIcuSegmentNextResponse)) {
            status = RIN_ICU_STATUS_DATA_ERROR;
        } else {
            if (out_segment) {
                *out_segment = *(const RinIcuSegmentNextResponse*)payload;
            }
            if (out_has_value) {
                *out_has_value = ((const RinIcuSegmentNextResponse*)payload)->has_value ? 1 : 0;
            }
        }
    }
    free(payload);
    return status;
}

int rin_icu_segmenter_destroy(rin_icu_client_t* client, rin_icu_handle_t handle)
{
    return rin_icu_destroy_handle(client, handle);
}

int rin_icu_number_formatter_create(rin_icu_client_t* client,
                                    const char* locale,
                                    const rin_icu_number_formatter_options_t* options,
                                    rin_icu_handle_t* out_handle)
{
    RinIcuNumberFormatterCreateRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t locale_len = rin_icu_strlen_c(locale);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), locale_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload || !out_handle) {
        free(request_payload);
        return RIN_ICU_STATUS_INVALID;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    if (options) {
        request.options = *options;
    } else {
        request.options.style = RIN_ICU_NUMBER_STYLE_DECIMAL;
        request.options.use_grouping = 1u;
        request.options.min_fraction_digits = -1;
        request.options.max_fraction_digits = -1;
    }
    memcpy(request_payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(request_payload + sizeof(request), locale, locale_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_NUMBER_FORMATTER_CREATE_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    free(payload);
    if (status == RIN_ICU_STATUS_OK) {
        *out_handle = response.handle_id;
    }
    return status;
}

int rin_icu_number_formatter_format(rin_icu_client_t* client,
                                    rin_icu_handle_t handle,
                                    double value,
                                    char* dest,
                                    size_t dest_cap,
                                    size_t* out_len)
{
    RinIcuNumberFormatRequest request;
    request.value_bits = rin_icu_double_to_bits(value);
    return rin_icu_call_text_command(client,
                                     RIN_ICU_CMD_NUMBER_FORMAT_V1,
                                     handle,
                                     &request,
                                     (uint32_t)sizeof(request),
                                     dest,
                                     dest_cap,
                                     out_len);
}

int rin_icu_number_formatter_destroy(rin_icu_client_t* client, rin_icu_handle_t handle)
{
    return rin_icu_destroy_handle(client, handle);
}

int rin_icu_datetime_formatter_create(rin_icu_client_t* client,
                                      const char* locale,
                                      const rin_icu_datetime_formatter_options_t* options,
                                      rin_icu_handle_t* out_handle)
{
    RinIcuDateTimeFormatterCreateRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t locale_len = rin_icu_strlen_c(locale);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), locale_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload || !out_handle) {
        free(request_payload);
        return RIN_ICU_STATUS_INVALID;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    if (options) {
        request.options = *options;
    } else {
        request.options.style = RIN_ICU_DATETIME_STYLE_DATETIME;
        request.options.hour_cycle = RIN_ICU_HOUR_CYCLE_DEFAULT;
    }
    memcpy(request_payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(request_payload + sizeof(request), locale, locale_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_DATETIME_FORMATTER_CREATE_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    free(payload);
    if (status == RIN_ICU_STATUS_OK) {
        *out_handle = response.handle_id;
    }
    return status;
}

int rin_icu_datetime_formatter_format_epoch_ms(rin_icu_client_t* client,
                                               rin_icu_handle_t handle,
                                               int64_t epoch_ms,
                                               char* dest,
                                               size_t dest_cap,
                                               size_t* out_len)
{
    RinIcuDateTimeFormatRequest request;
    request.epoch_ms = epoch_ms;
    return rin_icu_call_text_command(client,
                                     RIN_ICU_CMD_DATETIME_FORMAT_EPOCH_MS_V1,
                                     handle,
                                     &request,
                                     (uint32_t)sizeof(request),
                                     dest,
                                     dest_cap,
                                     out_len);
}

int rin_icu_datetime_formatter_destroy(rin_icu_client_t* client, rin_icu_handle_t handle)
{
    return rin_icu_destroy_handle(client, handle);
}

int rin_icu_plural_rules_create(rin_icu_client_t* client,
                                const char* locale,
                                const rin_icu_plural_rules_options_t* options,
                                rin_icu_handle_t* out_handle)
{
    RinIcuPluralRulesCreateRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t locale_len = rin_icu_strlen_c(locale);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), locale_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload || !out_handle) {
        free(request_payload);
        return RIN_ICU_STATUS_INVALID;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    if (options) {
        request.options = *options;
    } else {
        request.options.kind = RIN_ICU_PLURAL_KIND_CARDINAL;
    }
    memcpy(request_payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(request_payload + sizeof(request), locale, locale_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_PLURAL_RULES_CREATE_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    free(payload);
    if (status == RIN_ICU_STATUS_OK) {
        *out_handle = response.handle_id;
    }
    return status;
}

int rin_icu_plural_rules_select(rin_icu_client_t* client,
                                rin_icu_handle_t handle,
                                double value,
                                char* dest,
                                size_t dest_cap,
                                size_t* out_len)
{
    RinIcuPluralSelectRequest request;
    request.value_bits = rin_icu_double_to_bits(value);
    return rin_icu_call_text_command(client,
                                     RIN_ICU_CMD_PLURAL_RULES_SELECT_V1,
                                     handle,
                                     &request,
                                     (uint32_t)sizeof(request),
                                     dest,
                                     dest_cap,
                                     out_len);
}

int rin_icu_plural_rules_destroy(rin_icu_client_t* client, rin_icu_handle_t handle)
{
    return rin_icu_destroy_handle(client, handle);
}

int rin_icu_display_name(rin_icu_client_t* client,
                         const char* locale,
                         const char* code,
                         uint32_t type,
                         uint32_t style,
                         uint32_t language_display,
                         char* dest,
                         size_t dest_cap,
                         size_t* out_len)
{
    RinIcuDisplayNameRequest request;
    size_t locale_len = rin_icu_strlen_c(locale);
    size_t code_len = rin_icu_strlen_c(code);
    uint32_t payload_len;
    unsigned char* payload;
    int status;
    if (rin_icu_payload_size3(sizeof(request), locale_len, code_len,
                              &payload_len) != RIN_ICU_STATUS_OK)
        return RIN_ICU_STATUS_TOO_LARGE;
    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    request.code_len = (uint32_t)code_len;
    request.type = type;
    request.style = style;
    request.language_display = language_display;
    memcpy(payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(payload + sizeof(request), locale ? locale : "", locale_len);
    }
    if (code_len > 0u) {
        memcpy(payload + sizeof(request) + locale_len, code ? code : "", code_len);
    }
    status = rin_icu_call_text_command(client, RIN_ICU_CMD_DISPLAY_NAME_V1, 0u, payload, payload_len, dest, dest_cap, out_len);
    free(payload);
    return status;
}

int rin_icu_list_format(rin_icu_client_t* client,
                        const char* locale,
                        uint32_t type,
                        uint32_t style,
                        const char* const* items,
                        size_t item_count,
                        char* dest,
                        size_t dest_cap,
                        size_t* out_len)
{
    RinIcuListFormatRequest request;
    size_t locale_len = rin_icu_strlen_c(locale);
    size_t lengths_bytes;
    size_t text_bytes = 0u;
    size_t i;
    uint32_t payload_len;
    unsigned char* payload;
    int status;

    if (item_count > RIN_ICU_MAX_BULK_ITEMS ||
        (item_count > 0u && !items)) return RIN_ICU_STATUS_INVALID;
    if (rin_icu_mul_size(item_count, sizeof(uint32_t), &lengths_bytes) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    for (i = 0u; i < item_count; ++i) {
        size_t item_len = rin_icu_strlen_c(items[i]);
        if (rin_icu_add_size(text_bytes, item_len, &text_bytes) !=
            RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    }

    if (rin_icu_payload_size3(sizeof(request), locale_len, lengths_bytes,
                              &payload_len) != RIN_ICU_STATUS_OK)
        return RIN_ICU_STATUS_TOO_LARGE;
    if (rin_icu_payload_size2((size_t)payload_len, text_bytes,
                              &payload_len) != RIN_ICU_STATUS_OK)
        return RIN_ICU_STATUS_TOO_LARGE;

    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }

    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    request.item_count = (uint32_t)item_count;
    request.type = type;
    request.style = style;
    memcpy(payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(payload + sizeof(request), locale ? locale : "", locale_len);
    }
    {
        uint32_t* lengths = (uint32_t*)(payload + sizeof(request) + locale_len);
        unsigned char* text_ptr = payload + sizeof(request) + locale_len + lengths_bytes;
        for (i = 0u; i < item_count; ++i) {
            size_t item_len = rin_icu_strlen_c(items[i]);
            lengths[i] = (uint32_t)item_len;
            if (item_len > 0u) {
                memcpy(text_ptr, items[i], item_len);
                text_ptr += item_len;
            }
        }
    }

    status = rin_icu_call_text_command(client, RIN_ICU_CMD_LIST_FORMAT_V1, 0u, payload, payload_len, dest, dest_cap, out_len);
    free(payload);
    return status;
}

int rin_icu_relative_time_format(rin_icu_client_t* client,
                                 const char* locale,
                                 uint32_t style,
                                 uint32_t numeric_display,
                                 uint32_t unit,
                                 double value,
                                 char* dest,
                                 size_t dest_cap,
                                 size_t* out_len)
{
    RinIcuRelativeTimeRequest request;
    size_t locale_len = rin_icu_strlen_c(locale);
    uint32_t payload_len;
    unsigned char* payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), locale_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    memset(&request, 0, sizeof(request));
    request.locale_len = (uint32_t)locale_len;
    request.style = style;
    request.numeric_display = numeric_display;
    request.unit = unit;
    request.value_bits = rin_icu_double_to_bits(value);
    memcpy(payload, &request, sizeof(request));
    if (locale_len > 0u) {
        memcpy(payload + sizeof(request), locale ? locale : "", locale_len);
    }
    status = rin_icu_call_text_command(client, RIN_ICU_CMD_RELATIVE_TIME_FORMAT_V1, 0u, payload, payload_len, dest, dest_cap, out_len);
    free(payload);
    return status;
}

int rin_icu_time_zone_current(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len)
{
    return rin_icu_call_text_no_payload(client, RIN_ICU_CMD_TIME_ZONE_CURRENT_V1, dest, dest_cap, out_len);
}

int rin_icu_time_zone_canonicalize(rin_icu_client_t* client,
                                   const char* time_zone,
                                   char* dest,
                                   size_t dest_cap,
                                   size_t* out_len)
{
    return rin_icu_call_text_input(client, 0u, RIN_ICU_CMD_TIME_ZONE_CANONICALIZE_V1, time_zone ? time_zone : "", dest, dest_cap, out_len);
}

int rin_icu_time_zone_available(rin_icu_client_t* client, char* dest, size_t dest_cap, size_t* out_len)
{
    return rin_icu_call_text_no_payload(client, RIN_ICU_CMD_TIME_ZONE_AVAILABLE_V1, dest, dest_cap, out_len);
}

int rin_icu_time_zone_available_in_region(rin_icu_client_t* client,
                                          const char* region,
                                          char* dest,
                                          size_t dest_cap,
                                          size_t* out_len)
{
    return rin_icu_call_text_input(client, 0u,
                                   RIN_ICU_CMD_TIME_ZONE_AVAILABLE_REGION_V1,
                                   region ? region : "", dest, dest_cap, out_len);
}

int rin_icu_time_zone_offset(rin_icu_client_t* client,
                             const char* time_zone,
                             int64_t epoch_ms,
                             int* out_offset_minutes,
                             int* out_in_dst)
{
    RinIcuTimeZoneOffsetRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t time_zone_len = rin_icu_strlen_c(time_zone);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), time_zone_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload) {
        return RIN_ICU_STATUS_IO_ERROR;
    }
    memset(&request, 0, sizeof(request));
    request.time_zone_len = (uint32_t)time_zone_len;
    request.epoch_ms = epoch_ms;
    memcpy(request_payload, &request, sizeof(request));
    if (time_zone_len > 0u) {
        memcpy(request_payload + sizeof(request), time_zone ? time_zone : "", time_zone_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_TIME_ZONE_OFFSET_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    if (status == RIN_ICU_STATUS_OK) {
        RinIcuTimeZoneOffsetResponse const* offset_response;
        if (!payload || response.payload_len < sizeof(RinIcuTimeZoneOffsetResponse)) {
            status = RIN_ICU_STATUS_DATA_ERROR;
        } else {
            offset_response = (RinIcuTimeZoneOffsetResponse const*)payload;
            if (out_offset_minutes) {
                *out_offset_minutes = offset_response->offset_minutes;
            }
            if (out_in_dst) {
                *out_in_dst = offset_response->in_dst ? 1 : 0;
            }
        }
    }
    free(payload);
    return status;
}

int rin_icu_time_zone_transition(rin_icu_client_t* client,
                                 const char* time_zone,
                                 int64_t epoch_ms,
                                 uint32_t direction,
                                 uint32_t include_given_time,
                                 uint32_t transition_rule,
                                 int64_t* out_transition_epoch_ms)
{
    RinIcuTimeZoneTransitionRequest request;
    RinIcuMsgHeader response;
    unsigned char* payload = NULL;
    size_t time_zone_len = rin_icu_strlen_c(time_zone);
    uint32_t payload_len;
    unsigned char* request_payload;
    int status;
    if (rin_icu_payload_size2(sizeof(request), time_zone_len, &payload_len) !=
        RIN_ICU_STATUS_OK) return RIN_ICU_STATUS_TOO_LARGE;
    request_payload = (unsigned char*)malloc((size_t)payload_len);
    if (!request_payload) return RIN_ICU_STATUS_IO_ERROR;
    memset(&request, 0, sizeof(request));
    request.time_zone_len = (uint32_t)time_zone_len;
    request.epoch_ms = epoch_ms;
    request.direction = direction;
    request.include_given_time = include_given_time;
    request.transition_rule = transition_rule;
    memcpy(request_payload, &request, sizeof(request));
    if (time_zone_len > 0u) {
        memcpy(request_payload + sizeof(request), time_zone ? time_zone : "", time_zone_len);
    }
    status = rin_icu_client_call(client,
                                 RIN_ICU_CMD_TIME_ZONE_TRANSITION_V1,
                                 0u,
                                 0u,
                                 request_payload,
                                 payload_len,
                                 &response,
                                 &payload);
    free(request_payload);
    if (status == RIN_ICU_STATUS_OK) {
        RinIcuTimeZoneTransitionResponse const* transition_response;
        if (!payload || response.payload_len != sizeof(RinIcuTimeZoneTransitionResponse)) {
            status = RIN_ICU_STATUS_DATA_ERROR;
        } else {
            transition_response = (RinIcuTimeZoneTransitionResponse const*)payload;
            if (out_transition_epoch_ms) {
                *out_transition_epoch_ms = transition_response->transition_epoch_ms;
            }
        }
    }
    free(payload);
    return status;
}

int rin_icu_case_map(rin_icu_client_t* client,
                     const char* locale,
                     const char* input,
                     int to_upper,
                     char* dest,
                     size_t dest_cap,
                     size_t* out_len)
{
    size_t locale_len = rin_icu_strlen_c(locale);
    size_t input_len = rin_icu_strlen_c(input);
    uint32_t payload_len;
    unsigned char* payload;
    int status;
    if (rin_icu_payload_size3(sizeof(RinIcuCaseMapRequest), locale_len,
                              input_len, &payload_len) != RIN_ICU_STATUS_OK)
        return RIN_ICU_STATUS_TOO_LARGE;
    payload = (unsigned char*)malloc((size_t)payload_len);
    if (!payload) return RIN_ICU_STATUS_IO_ERROR;
    ((RinIcuCaseMapRequest*)payload)->locale_len = (uint32_t)locale_len;
    ((RinIcuCaseMapRequest*)payload)->text_len = (uint32_t)input_len;
    ((RinIcuCaseMapRequest*)payload)->to_upper = to_upper ? 1u : 0u;
    ((RinIcuCaseMapRequest*)payload)->reserved0 = 0u;
    if (locale_len > 0u) memcpy(payload + sizeof(RinIcuCaseMapRequest), locale, locale_len);
    if (input_len > 0u) memcpy(payload + sizeof(RinIcuCaseMapRequest) + locale_len, input, input_len);
    status = rin_icu_call_text_command(client,
                                       RIN_ICU_CMD_CASE_MAP_V1,
                                       0u,
                                       payload,
                                       payload_len,
                                       dest,
                                       dest_cap,
                                       out_len);
    free(payload);
    return status;
}
