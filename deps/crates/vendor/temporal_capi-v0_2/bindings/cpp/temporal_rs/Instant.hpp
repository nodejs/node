#ifndef TEMPORAL_RS_Instant_HPP
#define TEMPORAL_RS_Instant_HPP

#include "Instant.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "DifferenceSettings.hpp"
#include "Duration.hpp"
#include "I128Nanoseconds.hpp"
#include "Provider.hpp"
#include "RoundingOptions.hpp"
#include "TemporalError.hpp"
#include "TimeZone.hpp"
#include "ToStringRoundingOptions.hpp"
#include "ZonedDateTime.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_Instant_try_new_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_try_new_result;
    temporal_rs_Instant_try_new_result temporal_rs_Instant_try_new(temporal_rs::capi::I128Nanoseconds ns);

    typedef struct temporal_rs_Instant_from_epoch_milliseconds_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_epoch_milliseconds_result;
    temporal_rs_Instant_from_epoch_milliseconds_result temporal_rs_Instant_from_epoch_milliseconds(int64_t epoch_milliseconds);

    typedef struct temporal_rs_Instant_from_utf8_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_utf8_result;
    temporal_rs_Instant_from_utf8_result temporal_rs_Instant_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_Instant_from_utf16_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_utf16_result;
    temporal_rs_Instant_from_utf16_result temporal_rs_Instant_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    typedef struct temporal_rs_Instant_add_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_add_result;
    temporal_rs_Instant_add_result temporal_rs_Instant_add(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Duration* duration);

    typedef struct temporal_rs_Instant_subtract_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_subtract_result;
    temporal_rs_Instant_subtract_result temporal_rs_Instant_subtract(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Duration* duration);

    typedef struct temporal_rs_Instant_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_since_result;
    temporal_rs_Instant_since_result temporal_rs_Instant_since(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Instant* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_Instant_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_until_result;
    temporal_rs_Instant_until_result temporal_rs_Instant_until(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Instant* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_Instant_round_result {union {temporal_rs::capi::Instant* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_round_result;
    temporal_rs_Instant_round_result temporal_rs_Instant_round(const temporal_rs::capi::Instant* self, temporal_rs::capi::RoundingOptions options);

    int8_t temporal_rs_Instant_compare(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Instant* other);

    bool temporal_rs_Instant_equals(const temporal_rs::capi::Instant* self, const temporal_rs::capi::Instant* other);

    int64_t temporal_rs_Instant_epoch_milliseconds(const temporal_rs::capi::Instant* self);

    temporal_rs::capi::I128Nanoseconds temporal_rs_Instant_epoch_nanoseconds(const temporal_rs::capi::Instant* self);

    typedef struct temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result;
    temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result temporal_rs_Instant_to_ixdtf_string_with_compiled_data(const temporal_rs::capi::Instant* self, temporal_rs::capi::TimeZone_option zone, temporal_rs::capi::ToStringRoundingOptions options, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_Instant_to_ixdtf_string_with_provider_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_ixdtf_string_with_provider_result;
    temporal_rs_Instant_to_ixdtf_string_with_provider_result temporal_rs_Instant_to_ixdtf_string_with_provider(const temporal_rs::capi::Instant* self, temporal_rs::capi::TimeZone_option zone, temporal_rs::capi::ToStringRoundingOptions options, const temporal_rs::capi::Provider* p, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_Instant_to_zoned_date_time_iso_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_zoned_date_time_iso_result;
    temporal_rs_Instant_to_zoned_date_time_iso_result temporal_rs_Instant_to_zoned_date_time_iso(const temporal_rs::capi::Instant* self, temporal_rs::capi::TimeZone zone);

    typedef struct temporal_rs_Instant_to_zoned_date_time_iso_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_zoned_date_time_iso_with_provider_result;
    temporal_rs_Instant_to_zoned_date_time_iso_with_provider_result temporal_rs_Instant_to_zoned_date_time_iso_with_provider(const temporal_rs::capi::Instant* self, temporal_rs::capi::TimeZone zone, const temporal_rs::capi::Provider* p);

    temporal_rs::capi::Instant* temporal_rs_Instant_clone(const temporal_rs::capi::Instant* self);

    void temporal_rs_Instant_destroy(Instant* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::try_new(temporal_rs::I128Nanoseconds ns) {
    auto result = temporal_rs::capi::temporal_rs_Instant_try_new(ns.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::from_epoch_milliseconds(int64_t epoch_milliseconds) {
    auto result = temporal_rs::capi::temporal_rs_Instant_from_epoch_milliseconds(epoch_milliseconds);
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_Instant_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_Instant_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::add(const temporal_rs::Duration& duration) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_add(this->AsFFI(),
        duration.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::subtract(const temporal_rs::Duration& duration) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_subtract(this->AsFFI(),
        duration.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Instant::since(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_since(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Instant::until(const temporal_rs::Instant& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_until(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError> temporal_rs::Instant::round(temporal_rs::RoundingOptions options) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_round(this->AsFFI(),
        options.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Instant>>(std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Instant>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int8_t temporal_rs::Instant::compare(const temporal_rs::Instant& other) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_compare(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline bool temporal_rs::Instant::equals(const temporal_rs::Instant& other) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_equals(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline int64_t temporal_rs::Instant::epoch_milliseconds() const {
    auto result = temporal_rs::capi::temporal_rs_Instant_epoch_milliseconds(this->AsFFI());
    return result;
}

inline temporal_rs::I128Nanoseconds temporal_rs::Instant::epoch_nanoseconds() const {
    auto result = temporal_rs::capi::temporal_rs_Instant_epoch_nanoseconds(this->AsFFI());
    return temporal_rs::I128Nanoseconds::FromFFI(result);
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::Instant::to_ixdtf_string_with_compiled_data(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_Instant_to_ixdtf_string_with_compiled_data(this->AsFFI(),
        zone.has_value() ? (temporal_rs::capi::TimeZone_option{ { zone.value().AsFFI() }, true }) : (temporal_rs::capi::TimeZone_option{ {}, false }),
        options.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::Instant::to_ixdtf_string_with_compiled_data_write(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_Instant_to_ixdtf_string_with_compiled_data(this->AsFFI(),
        zone.has_value() ? (temporal_rs::capi::TimeZone_option{ { zone.value().AsFFI() }, true }) : (temporal_rs::capi::TimeZone_option{ {}, false }),
        options.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::Instant::to_ixdtf_string_with_provider(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options, const temporal_rs::Provider& p) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_Instant_to_ixdtf_string_with_provider(this->AsFFI(),
        zone.has_value() ? (temporal_rs::capi::TimeZone_option{ { zone.value().AsFFI() }, true }) : (temporal_rs::capi::TimeZone_option{ {}, false }),
        options.AsFFI(),
        p.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::Instant::to_ixdtf_string_with_provider_write(std::optional<temporal_rs::TimeZone> zone, temporal_rs::ToStringRoundingOptions options, const temporal_rs::Provider& p, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_Instant_to_ixdtf_string_with_provider(this->AsFFI(),
        zone.has_value() ? (temporal_rs::capi::TimeZone_option{ { zone.value().AsFFI() }, true }) : (temporal_rs::capi::TimeZone_option{ {}, false }),
        options.AsFFI(),
        p.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::Instant::to_zoned_date_time_iso(temporal_rs::TimeZone zone) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_to_zoned_date_time_iso(this->AsFFI(),
        zone.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::Instant::to_zoned_date_time_iso_with_provider(temporal_rs::TimeZone zone, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_Instant_to_zoned_date_time_iso_with_provider(this->AsFFI(),
        zone.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::Instant> temporal_rs::Instant::clone() const {
    auto result = temporal_rs::capi::temporal_rs_Instant_clone(this->AsFFI());
    return std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result));
}

inline const temporal_rs::capi::Instant* temporal_rs::Instant::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::Instant*>(this);
}

inline temporal_rs::capi::Instant* temporal_rs::Instant::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::Instant*>(this);
}

inline const temporal_rs::Instant* temporal_rs::Instant::FromFFI(const temporal_rs::capi::Instant* ptr) {
    return reinterpret_cast<const temporal_rs::Instant*>(ptr);
}

inline temporal_rs::Instant* temporal_rs::Instant::FromFFI(temporal_rs::capi::Instant* ptr) {
    return reinterpret_cast<temporal_rs::Instant*>(ptr);
}

inline void temporal_rs::Instant::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_Instant_destroy(reinterpret_cast<temporal_rs::capi::Instant*>(ptr));
}


#endif // TEMPORAL_RS_Instant_HPP
