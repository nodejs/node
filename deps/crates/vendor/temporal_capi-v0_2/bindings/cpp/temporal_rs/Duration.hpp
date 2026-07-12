#ifndef TEMPORAL_RS_Duration_HPP
#define TEMPORAL_RS_Duration_HPP

#include "Duration.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "PartialDuration.hpp"
#include "Provider.hpp"
#include "RelativeTo.hpp"
#include "RoundingOptions.hpp"
#include "Sign.hpp"
#include "TemporalError.hpp"
#include "ToStringRoundingOptions.hpp"
#include "Unit.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_Duration_create_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_create_result;
    temporal_rs_Duration_create_result temporal_rs_Duration_create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

    typedef struct temporal_rs_Duration_try_new_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_try_new_result;
    temporal_rs_Duration_try_new_result temporal_rs_Duration_try_new(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

    typedef struct temporal_rs_Duration_from_partial_duration_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_partial_duration_result;
    temporal_rs_Duration_from_partial_duration_result temporal_rs_Duration_from_partial_duration(temporal_rs::capi::PartialDuration partial);

    typedef struct temporal_rs_Duration_from_utf8_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_utf8_result;
    temporal_rs_Duration_from_utf8_result temporal_rs_Duration_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_Duration_from_utf16_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_utf16_result;
    temporal_rs_Duration_from_utf16_result temporal_rs_Duration_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    bool temporal_rs_Duration_is_time_within_range(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_years(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_months(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_weeks(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_days(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_hours(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_minutes(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_seconds(const temporal_rs::capi::Duration* self);

    int64_t temporal_rs_Duration_milliseconds(const temporal_rs::capi::Duration* self);

    double temporal_rs_Duration_microseconds(const temporal_rs::capi::Duration* self);

    double temporal_rs_Duration_nanoseconds(const temporal_rs::capi::Duration* self);

    temporal_rs::capi::Sign temporal_rs_Duration_sign(const temporal_rs::capi::Duration* self);

    bool temporal_rs_Duration_is_zero(const temporal_rs::capi::Duration* self);

    temporal_rs::capi::Duration* temporal_rs_Duration_abs(const temporal_rs::capi::Duration* self);

    temporal_rs::capi::Duration* temporal_rs_Duration_negated(const temporal_rs::capi::Duration* self);

    typedef struct temporal_rs_Duration_add_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_add_result;
    temporal_rs_Duration_add_result temporal_rs_Duration_add(const temporal_rs::capi::Duration* self, const temporal_rs::capi::Duration* other);

    typedef struct temporal_rs_Duration_subtract_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_subtract_result;
    temporal_rs_Duration_subtract_result temporal_rs_Duration_subtract(const temporal_rs::capi::Duration* self, const temporal_rs::capi::Duration* other);

    typedef struct temporal_rs_Duration_to_string_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_to_string_result;
    temporal_rs_Duration_to_string_result temporal_rs_Duration_to_string(const temporal_rs::capi::Duration* self, temporal_rs::capi::ToStringRoundingOptions options, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_Duration_round_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_round_result;
    temporal_rs_Duration_round_result temporal_rs_Duration_round(const temporal_rs::capi::Duration* self, temporal_rs::capi::RoundingOptions options, temporal_rs::capi::RelativeTo relative_to);

    typedef struct temporal_rs_Duration_round_with_provider_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_round_with_provider_result;
    temporal_rs_Duration_round_with_provider_result temporal_rs_Duration_round_with_provider(const temporal_rs::capi::Duration* self, temporal_rs::capi::RoundingOptions options, temporal_rs::capi::RelativeTo relative_to, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_Duration_compare_result {union {int8_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_compare_result;
    temporal_rs_Duration_compare_result temporal_rs_Duration_compare(const temporal_rs::capi::Duration* self, const temporal_rs::capi::Duration* other, temporal_rs::capi::RelativeTo relative_to);

    typedef struct temporal_rs_Duration_compare_with_provider_result {union {int8_t ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_compare_with_provider_result;
    temporal_rs_Duration_compare_with_provider_result temporal_rs_Duration_compare_with_provider(const temporal_rs::capi::Duration* self, const temporal_rs::capi::Duration* other, temporal_rs::capi::RelativeTo relative_to, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_Duration_total_result {union {double ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_total_result;
    temporal_rs_Duration_total_result temporal_rs_Duration_total(const temporal_rs::capi::Duration* self, temporal_rs::capi::Unit unit, temporal_rs::capi::RelativeTo relative_to);

    typedef struct temporal_rs_Duration_total_with_provider_result {union {double ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_Duration_total_with_provider_result;
    temporal_rs_Duration_total_with_provider_result temporal_rs_Duration_total_with_provider(const temporal_rs::capi::Duration* self, temporal_rs::capi::Unit unit, temporal_rs::capi::RelativeTo relative_to, const temporal_rs::capi::Provider* p);

    temporal_rs::capi::Duration* temporal_rs_Duration_clone(const temporal_rs::capi::Duration* self);

    void temporal_rs_Duration_destroy(Duration* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds) {
    auto result = temporal_rs::capi::temporal_rs_Duration_create(years,
        months,
        weeks,
        days,
        hours,
        minutes,
        seconds,
        milliseconds,
        microseconds,
        nanoseconds);
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::try_new(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds) {
    auto result = temporal_rs::capi::temporal_rs_Duration_try_new(years,
        months,
        weeks,
        days,
        hours,
        minutes,
        seconds,
        milliseconds,
        microseconds,
        nanoseconds);
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_partial_duration(temporal_rs::PartialDuration partial) {
    auto result = temporal_rs::capi::temporal_rs_Duration_from_partial_duration(partial.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_Duration_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_Duration_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::Duration::is_time_within_range() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_is_time_within_range(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::years() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_years(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::months() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_months(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::weeks() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_weeks(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::days() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_days(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::hours() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_hours(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::minutes() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_minutes(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::seconds() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_seconds(this->AsFFI());
    return result;
}

inline int64_t temporal_rs::Duration::milliseconds() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_milliseconds(this->AsFFI());
    return result;
}

inline double temporal_rs::Duration::microseconds() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_microseconds(this->AsFFI());
    return result;
}

inline double temporal_rs::Duration::nanoseconds() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_nanoseconds(this->AsFFI());
    return result;
}

inline temporal_rs::Sign temporal_rs::Duration::sign() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_sign(this->AsFFI());
    return temporal_rs::Sign::FromFFI(result);
}

inline bool temporal_rs::Duration::is_zero() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_is_zero(this->AsFFI());
    return result;
}

inline std::unique_ptr<temporal_rs::Duration> temporal_rs::Duration::abs() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_abs(this->AsFFI());
    return std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::Duration> temporal_rs::Duration::negated() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_negated(this->AsFFI());
    return std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::add(const temporal_rs::Duration& other) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_add(this->AsFFI(),
        other.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::subtract(const temporal_rs::Duration& other) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_subtract(this->AsFFI(),
        other.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::Duration::to_string(temporal_rs::ToStringRoundingOptions options) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_Duration_to_string(this->AsFFI(),
        options.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::Duration::to_string_write(temporal_rs::ToStringRoundingOptions options, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_Duration_to_string(this->AsFFI(),
        options.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::round(temporal_rs::RoundingOptions options, temporal_rs::RelativeTo relative_to) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_round(this->AsFFI(),
        options.AsFFI(),
        relative_to.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::Duration::round_with_provider(temporal_rs::RoundingOptions options, temporal_rs::RelativeTo relative_to, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_round_with_provider(this->AsFFI(),
        options.AsFFI(),
        relative_to.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError> temporal_rs::Duration::compare(const temporal_rs::Duration& other, temporal_rs::RelativeTo relative_to) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_compare(this->AsFFI(),
        other.AsFFI(),
        relative_to.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<int8_t>(result.ok)) : temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError> temporal_rs::Duration::compare_with_provider(const temporal_rs::Duration& other, temporal_rs::RelativeTo relative_to, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_compare_with_provider(this->AsFFI(),
        other.AsFFI(),
        relative_to.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<int8_t>(result.ok)) : temporal_rs::diplomat::result<int8_t, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<double, temporal_rs::TemporalError> temporal_rs::Duration::total(temporal_rs::Unit unit, temporal_rs::RelativeTo relative_to) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_total(this->AsFFI(),
        unit.AsFFI(),
        relative_to.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<double>(result.ok)) : temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<double, temporal_rs::TemporalError> temporal_rs::Duration::total_with_provider(temporal_rs::Unit unit, temporal_rs::RelativeTo relative_to, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_Duration_total_with_provider(this->AsFFI(),
        unit.AsFFI(),
        relative_to.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<double>(result.ok)) : temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::Duration> temporal_rs::Duration::clone() const {
    auto result = temporal_rs::capi::temporal_rs_Duration_clone(this->AsFFI());
    return std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result));
}

inline const temporal_rs::capi::Duration* temporal_rs::Duration::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::Duration*>(this);
}

inline temporal_rs::capi::Duration* temporal_rs::Duration::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::Duration*>(this);
}

inline const temporal_rs::Duration* temporal_rs::Duration::FromFFI(const temporal_rs::capi::Duration* ptr) {
    return reinterpret_cast<const temporal_rs::Duration*>(ptr);
}

inline temporal_rs::Duration* temporal_rs::Duration::FromFFI(temporal_rs::capi::Duration* ptr) {
    return reinterpret_cast<temporal_rs::Duration*>(ptr);
}

inline void temporal_rs::Duration::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_Duration_destroy(reinterpret_cast<temporal_rs::capi::Duration*>(ptr));
}


#endif // TEMPORAL_RS_Duration_HPP
