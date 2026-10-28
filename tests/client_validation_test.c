/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

/* Include the implementation so the host-only test can exercise private
 * wire-shape validators without adding them to the public API. */
#include "../src/client.c"

int main(void)
{
    rin_icu_client_t client;
    rin_icu_client_internal_t* internal;
    static char oversized[RIN_ICU_MAX_CSTRING_BYTES + 1u];
    unsigned char text_payload[sizeof(RinIcuBytesResponse) + 3u];
    unsigned char bulk_payload[sizeof(RinIcuBytesResponse) +
                               sizeof(RinIcuBulkBytesResponse) +
                               sizeof(uint32_t) + 2u];
    RinIcuBytesResponse outer;
    RinIcuBulkBytesResponse inner;
    RinIcuCompareResponse compare;
    int timeout_pair[2];
    int reinit_pair[2];
    struct timeval receive_timeout;
    struct timeval send_timeout;
    char text_output[8];
    uint8_t byte_output[8];
    size_t output_length = SIZE_MAX;

    memset(&client, 0, sizeof(client));
    rin_icu_client_init(&client);
    memset(oversized, 'x', sizeof(oversized));
    assert(rin_icu_strlen_c(NULL) == 0u);
    assert(rin_icu_strlen_c("") == 0u);
    assert(rin_icu_strlen_c("abc") == 3u);
    assert(rin_icu_strlen_c(oversized) == RIN_ICU_MAX_CSTRING_BYTES + 1u);
    internal = rin_icu_client_internal(&client);
    internal->next_request_id = UINT32_MAX;
    assert(rin_icu_next_request_id(internal) == UINT32_MAX);
    assert(internal->next_request_id == 1u);
    assert(rin_icu_next_request_id(internal) == 1u);
    assert(!rin_icu_status_valid(-999));

    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, reinit_pair) == 0);
    internal->fd = reinit_pair[0];
    rin_icu_client_init(&client);
    errno = 0;
    assert(fcntl(reinit_pair[0], F_GETFD) == -1 && errno == EBADF);
    close(reinit_pair[1]);
    internal = rin_icu_client_internal(&client);

    memset(text_payload, 0, sizeof(text_payload));
    ((RinIcuBytesResponse*)text_payload)->data_len = 3u;
    memcpy(text_payload + sizeof(RinIcuBytesResponse), "abc", 3u);
    assert(rin_icu_bytes_response_shape(text_payload, sizeof(text_payload)));
    assert(!rin_icu_bytes_response_shape(text_payload, sizeof(text_payload) - 1u));
    assert(!rin_icu_response_payload_valid(RIN_ICU_CMD_LOCALE_CANONICALIZE_V1,
                                           RIN_ICU_STATUS_INVALID,
                                           text_payload, sizeof(text_payload)));
    memset(text_output, 0xa5, sizeof(text_output));
    assert(rin_icu_copy_text_response(text_payload, sizeof(text_payload),
                                      text_output, 2u,
                                      &output_length) ==
           RIN_ICU_STATUS_NO_SPACE);
    assert(output_length == 3u);
    for (size_t index = 0u; index < 2u; ++index)
        assert(text_output[index] == 0u);
    for (size_t index = 2u; index < sizeof(text_output); ++index)
        assert((unsigned char)text_output[index] == 0xa5u);
    memset(text_output, 0xa5, sizeof(text_output));
    assert(rin_icu_copy_text_response(text_payload, sizeof(text_payload) - 1u,
                                      text_output, sizeof(text_output),
                                      &output_length) ==
           RIN_ICU_STATUS_DATA_ERROR);
    assert(output_length == 0u);
    for (size_t index = 0u; index < sizeof(text_output); ++index)
        assert(text_output[index] == 0u);
    memset(byte_output, 0xa5, sizeof(byte_output));
    assert(rin_icu_copy_bytes_response(text_payload, sizeof(text_payload),
                                       byte_output, 2u,
                                       &output_length) ==
           RIN_ICU_STATUS_NO_SPACE);
    assert(output_length == 3u);
    for (size_t index = 0u; index < 2u; ++index)
        assert(byte_output[index] == 0u);
    for (size_t index = 2u; index < sizeof(byte_output); ++index)
        assert(byte_output[index] == 0xa5u);

    memset(bulk_payload, 0, sizeof(bulk_payload));
    outer.data_len = (uint32_t)(sizeof(inner) + sizeof(uint32_t) + 2u);
    inner.item_count = 1u;
    inner.reserved0 = 0u;
    memcpy(bulk_payload, &outer, sizeof(outer));
    memcpy(bulk_payload + sizeof(outer), &inner, sizeof(inner));
    *(uint32_t*)(bulk_payload + sizeof(outer) + sizeof(inner)) = 2u;
    memcpy(bulk_payload + sizeof(outer) + sizeof(inner) + sizeof(uint32_t), "xy", 2u);
    assert(rin_icu_bulk_bytes_response_shape(bulk_payload, sizeof(bulk_payload)));
    inner.reserved0 = 1u;
    memcpy(bulk_payload + sizeof(outer), &inner, sizeof(inner));
    assert(!rin_icu_bulk_bytes_response_shape(bulk_payload, sizeof(bulk_payload)));

    assert(rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_CREATE_V1, 0u, 1u));
    assert(!rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_CREATE_V1, 0u, 0u));
    assert(rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_COMPARE_V1, 7u, 7u));
    assert(!rin_icu_response_handle_valid(RIN_ICU_CMD_COLLATOR_COMPARE_V1, 7u, 8u));

    assert(rin_icu_apply_io_timeout(-1) == -1);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, timeout_pair) == 0);
    assert(rin_icu_apply_io_timeout(timeout_pair[0]) == 0);
    socklen_t timeout_size = (socklen_t)sizeof(receive_timeout);
    assert(getsockopt(timeout_pair[0], SOL_SOCKET, SO_RCVTIMEO,
                      &receive_timeout, &timeout_size) == 0);
    assert(timeout_size == sizeof(receive_timeout));
    assert(receive_timeout.tv_sec == 30 && receive_timeout.tv_usec == 0);
    timeout_size = (socklen_t)sizeof(send_timeout);
    assert(getsockopt(timeout_pair[0], SOL_SOCKET, SO_SNDTIMEO,
                      &send_timeout, &timeout_size) == 0);
    assert(timeout_size == sizeof(send_timeout));
    assert(send_timeout.tv_sec == 30 && send_timeout.tv_usec == 0);
    close(timeout_pair[0]);
    close(timeout_pair[1]);

    compare.result = 0;
    compare.reserved0 = 1u;
    assert(!rin_icu_response_payload_valid(RIN_ICU_CMD_COLLATOR_COMPARE_V1,
                                           RIN_ICU_STATUS_OK,
                                           (unsigned char*)&compare,
                                           sizeof(compare)));
    rin_icu_client_close(&client);
    return 0;
}
