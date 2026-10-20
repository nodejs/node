#ifndef TEMPORAL_RS_ParsedZonedDateTime_HPP
#define TEMPORAL_RS_ParsedZonedDateTime_HPP

#include "ParsedZonedDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "Provider.hpp"
#include "TemporalError.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_ParsedZonedDateTime_from_utf8_result {union {temporal_rs::capi::ParsedZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedZonedDateTime_from_utf8_result;
    temporal_rs_ParsedZonedDateTime_from_utf8_result temporal_rs_ParsedZonedDateTime_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_ParsedZonedDateTime_from_utf8_with_provider_result {union {temporal_rs::capi::ParsedZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedZonedDateTime_from_utf8_with_provider_result;
    temporal_rs_ParsedZonedDateTime_from_utf8_with_provider_result temporal_rs_ParsedZonedDateTime_from_utf8_with_provider(temporal_rs::diplomat::capi::DiplomatStringView s, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ParsedZonedDateTime_from_utf16_result {union {temporal_rs::capi::ParsedZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedZonedDateTime_from_utf16_result;
    temporal_rs_ParsedZonedDateTime_from_utf16_result temporal_rs_ParsedZonedDateTime_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    typedef struct temporal_rs_ParsedZonedDateTime_from_utf16_with_provider_result {union {temporal_rs::capi::ParsedZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedZonedDateTime_from_utf16_with_provider_result;
    temporal_rs_ParsedZonedDateTime_from_utf16_with_provider_result temporal_rs_ParsedZonedDateTime_from_utf16_with_provider(temporal_rs::diplomat::capi::DiplomatString16View s, const temporal_rs::capi::Provider* p);

    void temporal_rs_ParsedZonedDateTime_destroy(ParsedZonedDateTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ParsedZonedDateTime::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedZonedDateTime_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedZonedDateTime>>(std::unique_ptr<temporal_rs::ParsedZonedDateTime>(temporal_rs::ParsedZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ParsedZonedDateTime::from_utf8_with_provider(std::string_view s, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ParsedZonedDateTime_from_utf8_with_provider({s.data(), s.size()},
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedZonedDateTime>>(std::unique_ptr<temporal_rs::ParsedZonedDateTime>(temporal_rs::ParsedZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ParsedZonedDateTime::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedZonedDateTime_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedZonedDateTime>>(std::unique_ptr<temporal_rs::ParsedZonedDateTime>(temporal_rs::ParsedZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ParsedZonedDateTime::from_utf16_with_provider(std::u16string_view s, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ParsedZonedDateTime_from_utf16_with_provider({s.data(), s.size()},
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedZonedDateTime>>(std::unique_ptr<temporal_rs::ParsedZonedDateTime>(temporal_rs::ParsedZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::ParsedZonedDateTime* temporal_rs::ParsedZonedDateTime::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::ParsedZonedDateTime*>(this);
}

inline temporal_rs::capi::ParsedZonedDateTime* temporal_rs::ParsedZonedDateTime::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::ParsedZonedDateTime*>(this);
}

inline const temporal_rs::ParsedZonedDateTime* temporal_rs::ParsedZonedDateTime::FromFFI(const temporal_rs::capi::ParsedZonedDateTime* ptr) {
    return reinterpret_cast<const temporal_rs::ParsedZonedDateTime*>(ptr);
}

inline temporal_rs::ParsedZonedDateTime* temporal_rs::ParsedZonedDateTime::FromFFI(temporal_rs::capi::ParsedZonedDateTime* ptr) {
    return reinterpret_cast<temporal_rs::ParsedZonedDateTime*>(ptr);
}

inline void temporal_rs::ParsedZonedDateTime::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_ParsedZonedDateTime_destroy(reinterpret_cast<temporal_rs::capi::ParsedZonedDateTime*>(ptr));
}


#endif // TEMPORAL_RS_ParsedZonedDateTime_HPP
