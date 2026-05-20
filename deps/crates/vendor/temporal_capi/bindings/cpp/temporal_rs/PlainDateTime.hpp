#ifndef TEMPORAL_RS_PlainDateTime_HPP
#define TEMPORAL_RS_PlainDateTime_HPP

#include "PlainDateTime.d.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <memory>
#include <functional>
#include <optional>
#include <cstdlib>
#include "AnyCalendarKind.hpp"
#include "ArithmeticOverflow.hpp"
#include "Calendar.hpp"
#include "DifferenceSettings.hpp"
#include "Disambiguation.hpp"
#include "DisplayCalendar.hpp"
#include "Duration.hpp"
#include "I128Nanoseconds.hpp"
#include "ParsedDateTime.hpp"
#include "PartialDateTime.hpp"
#include "PlainDate.hpp"
#include "PlainTime.hpp"
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

    typedef struct temporal_rs_PlainDateTime_try_new_constrain_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_try_new_constrain_result;
    temporal_rs_PlainDateTime_try_new_constrain_result temporal_rs_PlainDateTime_try_new_constrain(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, temporal_rs::capi::AnyCalendarKind calendar);

    typedef struct temporal_rs_PlainDateTime_try_new_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_try_new_result;
    temporal_rs_PlainDateTime_try_new_result temporal_rs_PlainDateTime_try_new(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, temporal_rs::capi::AnyCalendarKind calendar);

    typedef struct temporal_rs_PlainDateTime_from_partial_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_partial_result;
    temporal_rs_PlainDateTime_from_partial_result temporal_rs_PlainDateTime_from_partial(temporal_rs::capi::PartialDateTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDateTime_from_parsed_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_parsed_result;
    temporal_rs_PlainDateTime_from_parsed_result temporal_rs_PlainDateTime_from_parsed(const temporal_rs::capi::ParsedDateTime* parsed);

    typedef struct temporal_rs_PlainDateTime_from_epoch_milliseconds_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_milliseconds_result;
    temporal_rs_PlainDateTime_from_epoch_milliseconds_result temporal_rs_PlainDateTime_from_epoch_milliseconds(int64_t ms, temporal_rs::capi::TimeZone tz);

    typedef struct temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider_result;
    temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider_result temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider(int64_t ms, temporal_rs::capi::TimeZone tz, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_PlainDateTime_from_epoch_nanoseconds_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_nanoseconds_result;
    temporal_rs_PlainDateTime_from_epoch_nanoseconds_result temporal_rs_PlainDateTime_from_epoch_nanoseconds(temporal_rs::capi::I128Nanoseconds ns, temporal_rs::capi::TimeZone tz);

    typedef struct temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider_result;
    temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider_result temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider(temporal_rs::capi::I128Nanoseconds ns, temporal_rs::capi::TimeZone tz, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_PlainDateTime_with_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_with_result;
    temporal_rs_PlainDateTime_with_result temporal_rs_PlainDateTime_with(const temporal_rs::capi::PlainDateTime* self, temporal_rs::capi::PartialDateTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDateTime_with_time_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_with_time_result;
    temporal_rs_PlainDateTime_with_time_result temporal_rs_PlainDateTime_with_time(const temporal_rs::capi::PlainDateTime* self, const temporal_rs::capi::PlainTime* time);

    temporal_rs::capi::PlainDateTime* temporal_rs_PlainDateTime_with_calendar(const temporal_rs::capi::PlainDateTime* self, temporal_rs::capi::AnyCalendarKind calendar);

    typedef struct temporal_rs_PlainDateTime_from_utf8_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_utf8_result;
    temporal_rs_PlainDateTime_from_utf8_result temporal_rs_PlainDateTime_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s);

    typedef struct temporal_rs_PlainDateTime_from_utf16_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_utf16_result;
    temporal_rs_PlainDateTime_from_utf16_result temporal_rs_PlainDateTime_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s);

    uint8_t temporal_rs_PlainDateTime_hour(const temporal_rs::capi::PlainDateTime* self);

    uint8_t temporal_rs_PlainDateTime_minute(const temporal_rs::capi::PlainDateTime* self);

    uint8_t temporal_rs_PlainDateTime_second(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_millisecond(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_microsecond(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_nanosecond(const temporal_rs::capi::PlainDateTime* self);

    const temporal_rs::capi::Calendar* temporal_rs_PlainDateTime_calendar(const temporal_rs::capi::PlainDateTime* self);

    int32_t temporal_rs_PlainDateTime_year(const temporal_rs::capi::PlainDateTime* self);

    uint8_t temporal_rs_PlainDateTime_month(const temporal_rs::capi::PlainDateTime* self);

    void temporal_rs_PlainDateTime_month_code(const temporal_rs::capi::PlainDateTime* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    uint8_t temporal_rs_PlainDateTime_day(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_day_of_week(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_day_of_year(const temporal_rs::capi::PlainDateTime* self);

    typedef struct temporal_rs_PlainDateTime_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_PlainDateTime_week_of_year_result;
    temporal_rs_PlainDateTime_week_of_year_result temporal_rs_PlainDateTime_week_of_year(const temporal_rs::capi::PlainDateTime* self);

    typedef struct temporal_rs_PlainDateTime_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDateTime_year_of_week_result;
    temporal_rs_PlainDateTime_year_of_week_result temporal_rs_PlainDateTime_year_of_week(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_days_in_week(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_days_in_month(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_days_in_year(const temporal_rs::capi::PlainDateTime* self);

    uint16_t temporal_rs_PlainDateTime_months_in_year(const temporal_rs::capi::PlainDateTime* self);

    bool temporal_rs_PlainDateTime_in_leap_year(const temporal_rs::capi::PlainDateTime* self);

    void temporal_rs_PlainDateTime_era(const temporal_rs::capi::PlainDateTime* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_PlainDateTime_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDateTime_era_year_result;
    temporal_rs_PlainDateTime_era_year_result temporal_rs_PlainDateTime_era_year(const temporal_rs::capi::PlainDateTime* self);

    typedef struct temporal_rs_PlainDateTime_add_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_add_result;
    temporal_rs_PlainDateTime_add_result temporal_rs_PlainDateTime_add(const temporal_rs::capi::PlainDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDateTime_subtract_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_subtract_result;
    temporal_rs_PlainDateTime_subtract_result temporal_rs_PlainDateTime_subtract(const temporal_rs::capi::PlainDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_PlainDateTime_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_until_result;
    temporal_rs_PlainDateTime_until_result temporal_rs_PlainDateTime_until(const temporal_rs::capi::PlainDateTime* self, const temporal_rs::capi::PlainDateTime* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_PlainDateTime_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_since_result;
    temporal_rs_PlainDateTime_since_result temporal_rs_PlainDateTime_since(const temporal_rs::capi::PlainDateTime* self, const temporal_rs::capi::PlainDateTime* other, temporal_rs::capi::DifferenceSettings settings);

    bool temporal_rs_PlainDateTime_equals(const temporal_rs::capi::PlainDateTime* self, const temporal_rs::capi::PlainDateTime* other);

    int8_t temporal_rs_PlainDateTime_compare(const temporal_rs::capi::PlainDateTime* one, const temporal_rs::capi::PlainDateTime* two);

    typedef struct temporal_rs_PlainDateTime_round_result {union {temporal_rs::capi::PlainDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_round_result;
    temporal_rs_PlainDateTime_round_result temporal_rs_PlainDateTime_round(const temporal_rs::capi::PlainDateTime* self, temporal_rs::capi::RoundingOptions options);

    temporal_rs::capi::PlainDate* temporal_rs_PlainDateTime_to_plain_date(const temporal_rs::capi::PlainDateTime* self);

    temporal_rs::capi::PlainTime* temporal_rs_PlainDateTime_to_plain_time(const temporal_rs::capi::PlainDateTime* self);

    typedef struct temporal_rs_PlainDateTime_to_zoned_date_time_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_to_zoned_date_time_result;
    temporal_rs_PlainDateTime_to_zoned_date_time_result temporal_rs_PlainDateTime_to_zoned_date_time(const temporal_rs::capi::PlainDateTime* self, temporal_rs::capi::TimeZone time_zone, temporal_rs::capi::Disambiguation disambiguation);

    typedef struct temporal_rs_PlainDateTime_to_zoned_date_time_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_to_zoned_date_time_with_provider_result;
    temporal_rs_PlainDateTime_to_zoned_date_time_with_provider_result temporal_rs_PlainDateTime_to_zoned_date_time_with_provider(const temporal_rs::capi::PlainDateTime* self, temporal_rs::capi::TimeZone time_zone, temporal_rs::capi::Disambiguation disambiguation, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_PlainDateTime_to_ixdtf_string_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_to_ixdtf_string_result;
    temporal_rs_PlainDateTime_to_ixdtf_string_result temporal_rs_PlainDateTime_to_ixdtf_string(const temporal_rs::capi::PlainDateTime* self, temporal_rs::capi::ToStringRoundingOptions options, temporal_rs::capi::DisplayCalendar display_calendar, temporal_rs::diplomat::capi::DiplomatWrite* write);

    temporal_rs::capi::PlainDateTime* temporal_rs_PlainDateTime_clone(const temporal_rs::capi::PlainDateTime* self);

    void temporal_rs_PlainDateTime_destroy(PlainDateTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::try_new_constrain(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, temporal_rs::AnyCalendarKind calendar) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_try_new_constrain(year,
        month,
        day,
        hour,
        minute,
        second,
        millisecond,
        microsecond,
        nanosecond,
        calendar.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::try_new(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, temporal_rs::AnyCalendarKind calendar) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_try_new(year,
        month,
        day,
        hour,
        minute,
        second,
        millisecond,
        microsecond,
        nanosecond,
        calendar.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_partial(temporal_rs::PartialDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_partial(partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_parsed(const temporal_rs::ParsedDateTime& parsed) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_parsed(parsed.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_epoch_milliseconds(int64_t ms, temporal_rs::TimeZone tz) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_epoch_milliseconds(ms,
        tz.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_epoch_milliseconds_with_provider(int64_t ms, temporal_rs::TimeZone tz, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider(ms,
        tz.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_epoch_nanoseconds(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_epoch_nanoseconds(ns.AsFFI(),
        tz.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_epoch_nanoseconds_with_provider(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider(ns.AsFFI(),
        tz.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::with(temporal_rs::PartialDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_with(this->AsFFI(),
        partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::with_time(const temporal_rs::PlainTime* time) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_with_time(this->AsFFI(),
        time ? time->AsFFI() : nullptr);
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::PlainDateTime> temporal_rs::PlainDateTime::with_calendar(temporal_rs::AnyCalendarKind calendar) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_with_calendar(this->AsFFI(),
        calendar.AsFFI());
    return std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_utf8(std::string_view s) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_utf8({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::from_utf16(std::u16string_view s) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_from_utf16({s.data(), s.size()});
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint8_t temporal_rs::PlainDateTime::hour() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_hour(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::PlainDateTime::minute() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_minute(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::PlainDateTime::second() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_second(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::millisecond() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_millisecond(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::microsecond() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_microsecond(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::nanosecond() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_nanosecond(this->AsFFI());
    return result;
}

inline const temporal_rs::Calendar& temporal_rs::PlainDateTime::calendar() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_calendar(this->AsFFI());
    return *temporal_rs::Calendar::FromFFI(result);
}

inline int32_t temporal_rs::PlainDateTime::year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_year(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::PlainDateTime::month() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_month(this->AsFFI());
    return result;
}

inline std::string temporal_rs::PlainDateTime::month_code() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainDateTime_month_code(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainDateTime::month_code_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainDateTime_month_code(this->AsFFI(),
        &write);
}

inline uint8_t temporal_rs::PlainDateTime::day() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_day(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::day_of_week() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_day_of_week(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::day_of_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_day_of_year(this->AsFFI());
    return result;
}

inline std::optional<uint8_t> temporal_rs::PlainDateTime::week_of_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_week_of_year(this->AsFFI());
    return result.is_ok ? std::optional<uint8_t>(result.ok) : std::nullopt;
}

inline std::optional<int32_t> temporal_rs::PlainDateTime::year_of_week() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_year_of_week(this->AsFFI());
    return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline uint16_t temporal_rs::PlainDateTime::days_in_week() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_days_in_week(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::days_in_month() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_days_in_month(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::days_in_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_days_in_year(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::PlainDateTime::months_in_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_months_in_year(this->AsFFI());
    return result;
}

inline bool temporal_rs::PlainDateTime::in_leap_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_in_leap_year(this->AsFFI());
    return result;
}

inline std::string temporal_rs::PlainDateTime::era() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_PlainDateTime_era(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::PlainDateTime::era_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_PlainDateTime_era(this->AsFFI(),
        &write);
}

inline std::optional<int32_t> temporal_rs::PlainDateTime::era_year() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_era_year(this->AsFFI());
    return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_add(this->AsFFI(),
        duration.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_subtract(this->AsFFI(),
        duration.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::until(const temporal_rs::PlainDateTime& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_until(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::since(const temporal_rs::PlainDateTime& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_since(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline bool temporal_rs::PlainDateTime::equals(const temporal_rs::PlainDateTime& other) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_equals(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline int8_t temporal_rs::PlainDateTime::compare(const temporal_rs::PlainDateTime& one, const temporal_rs::PlainDateTime& two) {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_compare(one.AsFFI(),
        two.AsFFI());
    return result;
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::round(temporal_rs::RoundingOptions options) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_round(this->AsFFI(),
        options.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::PlainDateTime>>(std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::PlainDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::PlainDate> temporal_rs::PlainDateTime::to_plain_date() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_to_plain_date(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::PlainTime> temporal_rs::PlainDateTime::to_plain_time() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_to_plain_time(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::to_zoned_date_time(temporal_rs::TimeZone time_zone, temporal_rs::Disambiguation disambiguation) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_to_zoned_date_time(this->AsFFI(),
        time_zone.AsFFI(),
        disambiguation.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::PlainDateTime::to_zoned_date_time_with_provider(temporal_rs::TimeZone time_zone, temporal_rs::Disambiguation disambiguation, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_to_zoned_date_time_with_provider(this->AsFFI(),
        time_zone.AsFFI(),
        disambiguation.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::PlainDateTime::to_ixdtf_string(temporal_rs::ToStringRoundingOptions options, temporal_rs::DisplayCalendar display_calendar) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_to_ixdtf_string(this->AsFFI(),
        options.AsFFI(),
        display_calendar.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::PlainDateTime::to_ixdtf_string_write(temporal_rs::ToStringRoundingOptions options, temporal_rs::DisplayCalendar display_calendar, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_to_ixdtf_string(this->AsFFI(),
        options.AsFFI(),
        display_calendar.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::PlainDateTime> temporal_rs::PlainDateTime::clone() const {
    auto result = temporal_rs::capi::temporal_rs_PlainDateTime_clone(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result));
}

inline const temporal_rs::capi::PlainDateTime* temporal_rs::PlainDateTime::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::PlainDateTime*>(this);
}

inline temporal_rs::capi::PlainDateTime* temporal_rs::PlainDateTime::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::PlainDateTime*>(this);
}

inline const temporal_rs::PlainDateTime* temporal_rs::PlainDateTime::FromFFI(const temporal_rs::capi::PlainDateTime* ptr) {
    return reinterpret_cast<const temporal_rs::PlainDateTime*>(ptr);
}

inline temporal_rs::PlainDateTime* temporal_rs::PlainDateTime::FromFFI(temporal_rs::capi::PlainDateTime* ptr) {
    return reinterpret_cast<temporal_rs::PlainDateTime*>(ptr);
}

inline void temporal_rs::PlainDateTime::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_PlainDateTime_destroy(reinterpret_cast<temporal_rs::capi::PlainDateTime*>(ptr));
}


#endif // TEMPORAL_RS_PlainDateTime_HPP
