#ifndef TEMPORAL_RS_ParsedDate_HPP
#define TEMPORAL_RS_ParsedDate_HPP

#include "ParsedDate.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "TemporalError.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_ParsedDate_from_utf8_result {union {temporal_rs::capi::ParsedDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_from_utf8_result;
    temporal_rs_ParsedDate_from_utf8_result temporal_rs_ParsedDate_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_ParsedDate_from_utf16_result {union {temporal_rs::capi::ParsedDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_from_utf16_result;
    temporal_rs_ParsedDate_from_utf16_result temporal_rs_ParsedDate_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    typedef struct temporal_rs_ParsedDate_year_month_from_utf8_result {union {temporal_rs::capi::ParsedDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_year_month_from_utf8_result;
    temporal_rs_ParsedDate_year_month_from_utf8_result temporal_rs_ParsedDate_year_month_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_ParsedDate_year_month_from_utf16_result {union {temporal_rs::capi::ParsedDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_year_month_from_utf16_result;
    temporal_rs_ParsedDate_year_month_from_utf16_result temporal_rs_ParsedDate_year_month_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    typedef struct temporal_rs_ParsedDate_month_day_from_utf8_result {union {temporal_rs::capi::ParsedDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_month_day_from_utf8_result;
    temporal_rs_ParsedDate_month_day_from_utf8_result temporal_rs_ParsedDate_month_day_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_ParsedDate_month_day_from_utf16_result {union {temporal_rs::capi::ParsedDate* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_month_day_from_utf16_result;
    temporal_rs_ParsedDate_month_day_from_utf16_result temporal_rs_ParsedDate_month_day_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    void temporal_rs_ParsedDate_destroy(ParsedDate* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> temporal_rs::ParsedDate::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedDate_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDate>>(std::unique_ptr<temporal_rs::ParsedDate>(temporal_rs::ParsedDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> temporal_rs::ParsedDate::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedDate_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDate>>(std::unique_ptr<temporal_rs::ParsedDate>(temporal_rs::ParsedDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> temporal_rs::ParsedDate::year_month_from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedDate_year_month_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDate>>(std::unique_ptr<temporal_rs::ParsedDate>(temporal_rs::ParsedDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> temporal_rs::ParsedDate::year_month_from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedDate_year_month_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDate>>(std::unique_ptr<temporal_rs::ParsedDate>(temporal_rs::ParsedDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> temporal_rs::ParsedDate::month_day_from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedDate_month_day_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDate>>(std::unique_ptr<temporal_rs::ParsedDate>(temporal_rs::ParsedDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError> temporal_rs::ParsedDate::month_day_from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_ParsedDate_month_day_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ParsedDate>>(std::unique_ptr<temporal_rs::ParsedDate>(temporal_rs::ParsedDate::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ParsedDate>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline const temporal_rs::capi::ParsedDate* temporal_rs::ParsedDate::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::ParsedDate*>(this);
}

inline temporal_rs::capi::ParsedDate* temporal_rs::ParsedDate::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::ParsedDate*>(this);
}

inline const temporal_rs::ParsedDate* temporal_rs::ParsedDate::FromFFI(const temporal_rs::capi::ParsedDate* ptr) {
    return reinterpret_cast<const temporal_rs::ParsedDate*>(ptr);
}

inline temporal_rs::ParsedDate* temporal_rs::ParsedDate::FromFFI(temporal_rs::capi::ParsedDate* ptr) {
    return reinterpret_cast<temporal_rs::ParsedDate*>(ptr);
}

inline void temporal_rs::ParsedDate::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_ParsedDate_destroy(reinterpret_cast<temporal_rs::capi::ParsedDate*>(ptr));
}


#endif // TEMPORAL_RS_ParsedDate_HPP
