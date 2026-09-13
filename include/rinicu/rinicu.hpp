/* SPDX-License-Identifier: MIT */
#ifndef RINICU_RINICU_HPP
#define RINICU_RINICU_HPP

#include <rinicu/rinicu.h>
#include <memory>
#include <string>

namespace RinICU {

template<typename T>
struct Result {
    int status = RIN_ICU_STATUS_INVALID;
    T value = {};

    bool ok() const { return status == RIN_ICU_STATUS_OK; }
    explicit operator bool() const { return ok(); }
};

using StringResult = Result<std::string>;

struct ClientLease {
    rin_icu_client_t* client;
};

class Client {
public:
    Client() : m_client {}, m_lease(new ClientLease { &m_client })
    {
        rin_icu_client_init(&m_client);
    }
    ~Client()
    {
        rin_icu_client_close(&m_client);
        m_lease->client = nullptr;
    }

    Client(Client const&) = delete;
    Client& operator=(Client const&) = delete;

    int open() { return rin_icu_client_open(&m_client); }
    void close() { rin_icu_client_close(&m_client); }
    bool is_open() const { return rin_icu_client_is_open(&m_client) != 0; }
    rin_icu_client_t* raw() { return m_lease->client; }
    std::shared_ptr<ClientLease> lease() { return m_lease; }

    StringResult canonicalize(std::string const& locale) { return call_text(&rin_icu_locale_canonicalize, locale); }
    StringResult resolve(std::string const& locale) { return call_text(&rin_icu_locale_resolve, locale); }
    StringResult maximize(std::string const& locale) { return call_text(&rin_icu_locale_maximize, locale); }
    StringResult minimize(std::string const& locale) { return call_text(&rin_icu_locale_minimize, locale); }
    StringResult available_locales() { return call_text_no_payload(&rin_icu_locale_available); }
    StringResult preferred_locale() { return call_text_no_payload(&rin_icu_locale_preferred); }
    StringResult normalize(int form, std::string const& text) {
        return call_text_form(&rin_icu_normalize, form, text);
    }
    StringResult current_time_zone() { return call_text_no_payload(&rin_icu_time_zone_current); }
    StringResult canonicalize_time_zone(std::string const& time_zone) { return call_text(&rin_icu_time_zone_canonicalize, time_zone); }
    StringResult available_time_zones() { return call_text_no_payload(&rin_icu_time_zone_available); }

private:
    typedef int (*TextFn)(rin_icu_client_t*, const char*, char*, size_t, size_t*);
    typedef int (*TextFormFn)(rin_icu_client_t*, int, const char*, char*, size_t, size_t*);
    typedef int (*NoPayloadTextFn)(rin_icu_client_t*, char*, size_t, size_t*);

    StringResult call_text(TextFn fn, std::string const& input)
    {
        size_t len = 0;
        char buffer[256];
        if (!fn) return { RIN_ICU_STATUS_INVALID, {} };
        int status = fn(&m_client, input.c_str(), buffer, sizeof(buffer), &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len < sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
        }
        std::string out;
        out.resize(len + 1u);
        status = fn(&m_client, input.c_str(), out.data(), out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

    StringResult call_text_no_payload(NoPayloadTextFn fn)
    {
        size_t len = 0;
        char buffer[256];
        if (!fn) return { RIN_ICU_STATUS_INVALID, {} };
        int status = fn(&m_client, buffer, sizeof(buffer), &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len < sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
        }
        std::string out;
        out.resize(len + 1u);
        status = fn(&m_client, out.data(), out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

    StringResult call_text_form(TextFormFn fn, int form, std::string const& input)
    {
        size_t len = 0;
        char buffer[256];
        if (!fn) return { RIN_ICU_STATUS_INVALID, {} };
        int status = fn(&m_client, form, input.c_str(), buffer, sizeof(buffer),
                        &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len < sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
        }
        std::string out;
        out.resize(len + 1u);
        status = fn(&m_client, form, input.c_str(), out.data(), out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

    rin_icu_client_t m_client;
    std::shared_ptr<ClientLease> m_lease;
};

class Collator {
public:
    Collator() = default;
    ~Collator() { reset(); }
    Collator(Collator const&) = delete;
    Collator& operator=(Collator const&) = delete;
    Collator(Collator&&) = delete;
    Collator& operator=(Collator&&) = delete;

    int create(Client& client, std::string const& locale, rin_icu_collator_options_t const& options)
    {
        reset();
        m_lease = client.lease();
        m_client = client.raw();
        return rin_icu_collator_create(m_client, locale.c_str(), &options, &m_handle);
    }

    void reset()
    {
        if (m_lease && m_lease->client && m_handle) {
            (void)rin_icu_collator_destroy(m_lease->client, m_handle);
        }
        m_client = nullptr;
        m_lease.reset();
        m_handle = 0;
    }

    bool valid() const { return m_lease && m_lease->client && m_handle != 0; }

    Result<int> compare(std::string const& lhs, std::string const& rhs) const
    {
        int result = 0;
        if (!valid()) return { RIN_ICU_STATUS_INVALID, 0 };
        int status = rin_icu_collator_compare(m_client, m_handle, lhs.c_str(),
                                              rhs.c_str(), &result);
        if (status != RIN_ICU_STATUS_OK) return { status, 0 };
        return { RIN_ICU_STATUS_OK, result };
    }

    StringResult sort_key(std::string const& input) const
    {
        size_t len = 0;
        uint8_t buffer[256];
        if (!valid()) return { RIN_ICU_STATUS_INVALID, {} };
        int status = rin_icu_collator_sort_key(m_client, m_handle, input.c_str(),
                                               buffer, sizeof(buffer), &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len <= sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK,
                     std::string(reinterpret_cast<char const*>(buffer), len) };
        }
        std::string out;
        out.resize(len);
        status = rin_icu_collator_sort_key(m_client, m_handle, input.c_str(),
                                           reinterpret_cast<uint8_t*>(out.data()),
                                           out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

    StringResult sort_keys_bulk_raw(char const* const* inputs, size_t count) const
    {
        size_t len = 0;
        uint8_t buffer[256];
        if (!valid()) return { RIN_ICU_STATUS_INVALID, {} };
        int status = rin_icu_collator_sort_keys_bulk(m_client, m_handle, inputs,
                                                     count, buffer, sizeof(buffer),
                                                     &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len <= sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK,
                     std::string(reinterpret_cast<char const*>(buffer), len) };
        }
        std::string out;
        out.resize(len);
        status = rin_icu_collator_sort_keys_bulk(m_client, m_handle, inputs, count,
                                                 reinterpret_cast<uint8_t*>(out.data()),
                                                 out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

private:
    rin_icu_client_t* m_client { nullptr };
    std::shared_ptr<ClientLease> m_lease;
    rin_icu_handle_t m_handle { 0 };
};

class Segmenter {
public:
    Segmenter() = default;
    ~Segmenter() { reset(); }
    Segmenter(Segmenter const&) = delete;
    Segmenter& operator=(Segmenter const&) = delete;
    Segmenter(Segmenter&&) = delete;
    Segmenter& operator=(Segmenter&&) = delete;

    int create(Client& client, std::string const& locale, rin_icu_segmenter_options_t const& options)
    {
        reset();
        m_lease = client.lease();
        m_client = client.raw();
        return rin_icu_segmenter_create(m_client, locale.c_str(), &options, &m_handle);
    }

    void reset()
    {
        if (m_lease && m_lease->client && m_handle) {
            (void)rin_icu_segmenter_destroy(m_lease->client, m_handle);
        }
        m_client = nullptr;
        m_lease.reset();
        m_handle = 0;
    }

    bool valid() const { return m_lease && m_lease->client && m_handle != 0; }

    int set_text(std::string const& text) const
    {
        if (!valid()) {
            return RIN_ICU_STATUS_INVALID;
        }
        return rin_icu_segmenter_reset(m_client, m_handle, text.c_str());
    }

    int next(rin_icu_segment_t& out_segment, bool& has_value) const
    {
        int present = 0;
        if (!valid()) {
            has_value = false;
            return RIN_ICU_STATUS_INVALID;
        }
        int status = rin_icu_segmenter_next(m_client, m_handle, &out_segment, &present);
        if (status != RIN_ICU_STATUS_OK) {
            has_value = false;
            return status;
        }
        has_value = present != 0;
        return RIN_ICU_STATUS_OK;
    }

private:
    rin_icu_client_t* m_client { nullptr };
    std::shared_ptr<ClientLease> m_lease;
    rin_icu_handle_t m_handle { 0 };
};

class NumberFormatter {
public:
    NumberFormatter() = default;
    ~NumberFormatter() { reset(); }
    NumberFormatter(NumberFormatter const&) = delete;
    NumberFormatter& operator=(NumberFormatter const&) = delete;
    NumberFormatter(NumberFormatter&&) = delete;
    NumberFormatter& operator=(NumberFormatter&&) = delete;

    int create(Client& client, std::string const& locale, rin_icu_number_formatter_options_t const& options)
    {
        reset();
        m_lease = client.lease();
        m_client = client.raw();
        return rin_icu_number_formatter_create(m_client, locale.c_str(), &options, &m_handle);
    }

    void reset()
    {
        if (m_lease && m_lease->client && m_handle) {
            (void)rin_icu_number_formatter_destroy(m_lease->client, m_handle);
        }
        m_client = nullptr;
        m_lease.reset();
        m_handle = 0;
    }

    bool valid() const { return m_lease && m_lease->client && m_handle != 0; }

    StringResult format(double value) const
    {
        size_t len = 0;
        char buffer[256];
        if (!valid()) return { RIN_ICU_STATUS_INVALID, {} };
        int status = rin_icu_number_formatter_format(m_client, m_handle, value,
                                                     buffer, sizeof(buffer), &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len < sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
        }
        std::string out;
        out.resize(len + 1u);
        status = rin_icu_number_formatter_format(m_client, m_handle, value,
                                                  out.data(), out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

private:
    rin_icu_client_t* m_client { nullptr };
    std::shared_ptr<ClientLease> m_lease;
    rin_icu_handle_t m_handle { 0 };
};

class DateTimeFormatter {
public:
    DateTimeFormatter() = default;
    ~DateTimeFormatter() { reset(); }
    DateTimeFormatter(DateTimeFormatter const&) = delete;
    DateTimeFormatter& operator=(DateTimeFormatter const&) = delete;
    DateTimeFormatter(DateTimeFormatter&&) = delete;
    DateTimeFormatter& operator=(DateTimeFormatter&&) = delete;

    int create(Client& client, std::string const& locale, rin_icu_datetime_formatter_options_t const& options)
    {
        reset();
        m_lease = client.lease();
        m_client = client.raw();
        return rin_icu_datetime_formatter_create(m_client, locale.c_str(), &options, &m_handle);
    }

    void reset()
    {
        if (m_lease && m_lease->client && m_handle) {
            (void)rin_icu_datetime_formatter_destroy(m_lease->client, m_handle);
        }
        m_client = nullptr;
        m_lease.reset();
        m_handle = 0;
    }

    bool valid() const { return m_lease && m_lease->client && m_handle != 0; }

    StringResult format_epoch_ms(long long epoch_ms) const
    {
        size_t len = 0;
        char buffer[256];
        if (!valid()) return { RIN_ICU_STATUS_INVALID, {} };
        int status = rin_icu_datetime_formatter_format_epoch_ms(m_client, m_handle,
                                                                epoch_ms, buffer,
                                                                sizeof(buffer), &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len < sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
        }
        std::string out;
        out.resize(len + 1u);
        status = rin_icu_datetime_formatter_format_epoch_ms(m_client, m_handle,
                                                            epoch_ms, out.data(),
                                                            out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

private:
    rin_icu_client_t* m_client { nullptr };
    std::shared_ptr<ClientLease> m_lease;
    rin_icu_handle_t m_handle { 0 };
};

class PluralRules {
public:
    PluralRules() = default;
    ~PluralRules() { reset(); }
    PluralRules(PluralRules const&) = delete;
    PluralRules& operator=(PluralRules const&) = delete;
    PluralRules(PluralRules&&) = delete;
    PluralRules& operator=(PluralRules&&) = delete;

    int create(Client& client, std::string const& locale, rin_icu_plural_rules_options_t const& options)
    {
        reset();
        m_lease = client.lease();
        m_client = client.raw();
        return rin_icu_plural_rules_create(m_client, locale.c_str(), &options, &m_handle);
    }

    void reset()
    {
        if (m_lease && m_lease->client && m_handle) {
            (void)rin_icu_plural_rules_destroy(m_lease->client, m_handle);
        }
        m_client = nullptr;
        m_lease.reset();
        m_handle = 0;
    }

    bool valid() const { return m_lease && m_lease->client && m_handle != 0; }

    StringResult select(double value) const
    {
        size_t len = 0;
        char buffer[256];
        if (!valid()) return { RIN_ICU_STATUS_INVALID, {} };
        int status = rin_icu_plural_rules_select(m_client, m_handle, value,
                                                 buffer, sizeof(buffer), &len);
        if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
            return { status, {} };
        if (len < sizeof(buffer)) {
            return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
        }
        std::string out;
        out.resize(len + 1u);
        status = rin_icu_plural_rules_select(m_client, m_handle, value,
                                             out.data(), out.size(), &len);
        if (status != RIN_ICU_STATUS_OK) return { status, {} };
        out.resize(len);
        return { RIN_ICU_STATUS_OK, out };
    }

private:
    rin_icu_client_t* m_client { nullptr };
    std::shared_ptr<ClientLease> m_lease;
    rin_icu_handle_t m_handle { 0 };
};

inline StringResult display_name(Client& client, std::string const& locale, std::string const& code, uint32_t type, uint32_t style, uint32_t language_display = RIN_ICU_LANGUAGE_DISPLAY_STANDARD)
{
    size_t len = 0;
    char buffer[256];
    int status = rin_icu_display_name(client.raw(), locale.c_str(), code.c_str(),
                                      type, style, language_display, buffer,
                                      sizeof(buffer), &len);
    if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
        return { status, {} };
    if (len < sizeof(buffer)) {
        return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
    }
    std::string out;
    out.resize(len + 1u);
    status = rin_icu_display_name(client.raw(), locale.c_str(), code.c_str(),
                                  type, style, language_display, out.data(),
                                  out.size(), &len);
    if (status != RIN_ICU_STATUS_OK) return { status, {} };
    out.resize(len);
    return { RIN_ICU_STATUS_OK, out };
}

inline StringResult list_format(Client& client, std::string const& locale, uint32_t type, uint32_t style, char const* const* items, size_t item_count)
{
    size_t len = 0;
    char buffer[256];
    int status = rin_icu_list_format(client.raw(), locale.c_str(), type, style,
                                     items, item_count, buffer, sizeof(buffer),
                                     &len);
    if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
        return { status, {} };
    if (len < sizeof(buffer)) {
        return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
    }
    std::string out;
    out.resize(len + 1u);
    status = rin_icu_list_format(client.raw(), locale.c_str(), type, style, items,
                                 item_count, out.data(), out.size(), &len);
    if (status != RIN_ICU_STATUS_OK) return { status, {} };
    out.resize(len);
    return { RIN_ICU_STATUS_OK, out };
}

inline StringResult relative_time_format(Client& client, std::string const& locale, uint32_t style, uint32_t numeric_display, uint32_t unit, double value)
{
    size_t len = 0;
    char buffer[256];
    int status = rin_icu_relative_time_format(client.raw(), locale.c_str(), style,
                                              numeric_display, unit, value, buffer,
                                              sizeof(buffer), &len);
    if (status != RIN_ICU_STATUS_OK && status != RIN_ICU_STATUS_NO_SPACE)
        return { status, {} };
    if (len < sizeof(buffer)) {
        return { RIN_ICU_STATUS_OK, std::string(buffer, len) };
    }
    std::string out;
    out.resize(len + 1u);
    status = rin_icu_relative_time_format(client.raw(), locale.c_str(), style,
                                          numeric_display, unit, value, out.data(),
                                          out.size(), &len);
    if (status != RIN_ICU_STATUS_OK) return { status, {} };
    out.resize(len);
    return { RIN_ICU_STATUS_OK, out };
}

inline int time_zone_offset(Client& client, std::string const& time_zone, long long epoch_ms, int& offset_minutes, int& in_dst)
{
    return rin_icu_time_zone_offset(client.raw(), time_zone.c_str(), epoch_ms,
                                    &offset_minutes, &in_dst);
}

}

#endif
