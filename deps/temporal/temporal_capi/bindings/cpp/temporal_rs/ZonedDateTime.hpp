#ifndef TEMPORAL_RS_ZonedDateTime_HPP
#define TEMPORAL_RS_ZonedDateTime_HPP

#include "ZonedDateTime.d.hpp"

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
#include "DisplayOffset.hpp"
#include "DisplayTimeZone.hpp"
#include "Duration.hpp"
#include "I128Nanoseconds.hpp"
#include "Instant.hpp"
#include "OffsetDisambiguation.hpp"
#include "ParsedZonedDateTime.hpp"
#include "PartialZonedDateTime.hpp"
#include "PlainDate.hpp"
#include "PlainDateTime.hpp"
#include "PlainTime.hpp"
#include "Provider.hpp"
#include "RoundingOptions.hpp"
#include "TemporalError.hpp"
#include "TimeZone.hpp"
#include "ToStringRoundingOptions.hpp"
#include "TransitionDirection.hpp"
#include "diplomat_runtime.hpp"


namespace temporal_rs {
namespace capi {
    extern "C" {

    typedef struct temporal_rs_ZonedDateTime_try_new_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_try_new_result;
    temporal_rs_ZonedDateTime_try_new_result temporal_rs_ZonedDateTime_try_new(temporal_rs::capi::I128Nanoseconds nanosecond, temporal_rs::capi::AnyCalendarKind calendar, temporal_rs::capi::TimeZone time_zone);

    typedef struct temporal_rs_ZonedDateTime_try_new_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_try_new_with_provider_result;
    temporal_rs_ZonedDateTime_try_new_with_provider_result temporal_rs_ZonedDateTime_try_new_with_provider(temporal_rs::capi::I128Nanoseconds nanosecond, temporal_rs::capi::AnyCalendarKind calendar, temporal_rs::capi::TimeZone time_zone, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_from_partial_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_partial_result;
    temporal_rs_ZonedDateTime_from_partial_result temporal_rs_ZonedDateTime_from_partial(temporal_rs::capi::PartialZonedDateTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option);

    typedef struct temporal_rs_ZonedDateTime_from_partial_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_partial_with_provider_result;
    temporal_rs_ZonedDateTime_from_partial_with_provider_result temporal_rs_ZonedDateTime_from_partial_with_provider(temporal_rs::capi::PartialZonedDateTime partial, temporal_rs::capi::ArithmeticOverflow_option overflow, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_from_parsed_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_parsed_result;
    temporal_rs_ZonedDateTime_from_parsed_result temporal_rs_ZonedDateTime_from_parsed(const temporal_rs::capi::ParsedZonedDateTime* parsed, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_option);

    typedef struct temporal_rs_ZonedDateTime_from_parsed_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_parsed_with_provider_result;
    temporal_rs_ZonedDateTime_from_parsed_with_provider_result temporal_rs_ZonedDateTime_from_parsed_with_provider(const temporal_rs::capi::ParsedZonedDateTime* parsed, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_option, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_from_utf8_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf8_result;
    temporal_rs_ZonedDateTime_from_utf8_result temporal_rs_ZonedDateTime_from_utf8(temporal_rs::diplomat::capi::DiplomatStringView s, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_disambiguation);

    typedef struct temporal_rs_ZonedDateTime_from_utf8_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf8_with_provider_result;
    temporal_rs_ZonedDateTime_from_utf8_with_provider_result temporal_rs_ZonedDateTime_from_utf8_with_provider(temporal_rs::diplomat::capi::DiplomatStringView s, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_disambiguation, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_from_utf16_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf16_result;
    temporal_rs_ZonedDateTime_from_utf16_result temporal_rs_ZonedDateTime_from_utf16(temporal_rs::diplomat::capi::DiplomatString16View s, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_disambiguation);

    typedef struct temporal_rs_ZonedDateTime_from_utf16_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf16_with_provider_result;
    temporal_rs_ZonedDateTime_from_utf16_with_provider_result temporal_rs_ZonedDateTime_from_utf16_with_provider(temporal_rs::diplomat::capi::DiplomatString16View s, temporal_rs::capi::Disambiguation disambiguation, temporal_rs::capi::OffsetDisambiguation offset_disambiguation, const temporal_rs::capi::Provider* p);

    int64_t temporal_rs_ZonedDateTime_epoch_milliseconds(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_from_epoch_milliseconds_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_milliseconds_result;
    temporal_rs_ZonedDateTime_from_epoch_milliseconds_result temporal_rs_ZonedDateTime_from_epoch_milliseconds(int64_t ms, temporal_rs::capi::TimeZone tz);

    typedef struct temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider_result;
    temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider_result temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider(int64_t ms, temporal_rs::capi::TimeZone tz, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_from_epoch_nanoseconds_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_nanoseconds_result;
    temporal_rs_ZonedDateTime_from_epoch_nanoseconds_result temporal_rs_ZonedDateTime_from_epoch_nanoseconds(temporal_rs::capi::I128Nanoseconds ns, temporal_rs::capi::TimeZone tz);

    typedef struct temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider_result;
    temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider_result temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider(temporal_rs::capi::I128Nanoseconds ns, temporal_rs::capi::TimeZone tz, const temporal_rs::capi::Provider* p);

    temporal_rs::capi::I128Nanoseconds temporal_rs_ZonedDateTime_epoch_nanoseconds(const temporal_rs::capi::ZonedDateTime* self);

    int64_t temporal_rs_ZonedDateTime_offset_nanoseconds(const temporal_rs::capi::ZonedDateTime* self);

    temporal_rs::capi::Instant* temporal_rs_ZonedDateTime_to_instant(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_with_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_result;
    temporal_rs_ZonedDateTime_with_result temporal_rs_ZonedDateTime_with(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::PartialZonedDateTime partial, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_ZonedDateTime_with_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_with_provider_result;
    temporal_rs_ZonedDateTime_with_with_provider_result temporal_rs_ZonedDateTime_with_with_provider(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::PartialZonedDateTime partial, temporal_rs::capi::Disambiguation_option disambiguation, temporal_rs::capi::OffsetDisambiguation_option offset_option, temporal_rs::capi::ArithmeticOverflow_option overflow, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_with_timezone_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_timezone_result;
    temporal_rs_ZonedDateTime_with_timezone_result temporal_rs_ZonedDateTime_with_timezone(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::TimeZone zone);

    typedef struct temporal_rs_ZonedDateTime_with_timezone_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_timezone_with_provider_result;
    temporal_rs_ZonedDateTime_with_timezone_with_provider_result temporal_rs_ZonedDateTime_with_timezone_with_provider(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::TimeZone zone, const temporal_rs::capi::Provider* p);

    temporal_rs::capi::TimeZone temporal_rs_ZonedDateTime_timezone(const temporal_rs::capi::ZonedDateTime* self);

    int8_t temporal_rs_ZonedDateTime_compare_instant(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other);

    bool temporal_rs_ZonedDateTime_equals(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other);

    typedef struct temporal_rs_ZonedDateTime_equals_with_provider_result {union {bool ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_equals_with_provider_result;
    temporal_rs_ZonedDateTime_equals_with_provider_result temporal_rs_ZonedDateTime_equals_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_offset_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_offset_result;
    temporal_rs_ZonedDateTime_offset_result temporal_rs_ZonedDateTime_offset(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_ZonedDateTime_start_of_day_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_start_of_day_result;
    temporal_rs_ZonedDateTime_start_of_day_result temporal_rs_ZonedDateTime_start_of_day(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_start_of_day_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_start_of_day_with_provider_result;
    temporal_rs_ZonedDateTime_start_of_day_with_provider_result temporal_rs_ZonedDateTime_start_of_day_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_get_time_zone_transition_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_get_time_zone_transition_result;
    temporal_rs_ZonedDateTime_get_time_zone_transition_result temporal_rs_ZonedDateTime_get_time_zone_transition(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::TransitionDirection direction);

    typedef struct temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider_result;
    temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider_result temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::TransitionDirection direction, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_hours_in_day_result {union {double ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_hours_in_day_result;
    temporal_rs_ZonedDateTime_hours_in_day_result temporal_rs_ZonedDateTime_hours_in_day(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_hours_in_day_with_provider_result {union {double ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_hours_in_day_with_provider_result;
    temporal_rs_ZonedDateTime_hours_in_day_with_provider_result temporal_rs_ZonedDateTime_hours_in_day_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Provider* p);

    temporal_rs::capi::PlainDateTime* temporal_rs_ZonedDateTime_to_plain_datetime(const temporal_rs::capi::ZonedDateTime* self);

    temporal_rs::capi::PlainDate* temporal_rs_ZonedDateTime_to_plain_date(const temporal_rs::capi::ZonedDateTime* self);

    temporal_rs::capi::PlainTime* temporal_rs_ZonedDateTime_to_plain_time(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_to_ixdtf_string_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_ixdtf_string_result;
    temporal_rs_ZonedDateTime_to_ixdtf_string_result temporal_rs_ZonedDateTime_to_ixdtf_string(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::DisplayOffset display_offset, temporal_rs::capi::DisplayTimeZone display_timezone, temporal_rs::capi::DisplayCalendar display_calendar, temporal_rs::capi::ToStringRoundingOptions options, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider_result {union { temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider_result;
    temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider_result temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::DisplayOffset display_offset, temporal_rs::capi::DisplayTimeZone display_timezone, temporal_rs::capi::DisplayCalendar display_calendar, temporal_rs::capi::ToStringRoundingOptions options, const temporal_rs::capi::Provider* p, temporal_rs::diplomat::capi::DiplomatWrite* write);

    temporal_rs::capi::ZonedDateTime* temporal_rs_ZonedDateTime_with_calendar(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::AnyCalendarKind calendar);

    typedef struct temporal_rs_ZonedDateTime_with_plain_time_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_plain_time_result;
    temporal_rs_ZonedDateTime_with_plain_time_result temporal_rs_ZonedDateTime_with_plain_time(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::PlainTime* time);

    typedef struct temporal_rs_ZonedDateTime_with_plain_time_and_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_plain_time_and_provider_result;
    temporal_rs_ZonedDateTime_with_plain_time_and_provider_result temporal_rs_ZonedDateTime_with_plain_time_and_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::PlainTime* time, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_add_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_add_result;
    temporal_rs_ZonedDateTime_add_result temporal_rs_ZonedDateTime_add(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_ZonedDateTime_add_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_add_with_provider_result;
    temporal_rs_ZonedDateTime_add_with_provider_result temporal_rs_ZonedDateTime_add_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_subtract_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_subtract_result;
    temporal_rs_ZonedDateTime_subtract_result temporal_rs_ZonedDateTime_subtract(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow);

    typedef struct temporal_rs_ZonedDateTime_subtract_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_subtract_with_provider_result;
    temporal_rs_ZonedDateTime_subtract_with_provider_result temporal_rs_ZonedDateTime_subtract_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::Duration* duration, temporal_rs::capi::ArithmeticOverflow_option overflow, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_until_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_until_result;
    temporal_rs_ZonedDateTime_until_result temporal_rs_ZonedDateTime_until(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_ZonedDateTime_until_with_provider_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_until_with_provider_result;
    temporal_rs_ZonedDateTime_until_with_provider_result temporal_rs_ZonedDateTime_until_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, temporal_rs::capi::DifferenceSettings settings, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_since_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_since_result;
    temporal_rs_ZonedDateTime_since_result temporal_rs_ZonedDateTime_since(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, temporal_rs::capi::DifferenceSettings settings);

    typedef struct temporal_rs_ZonedDateTime_since_with_provider_result {union {temporal_rs::capi::Duration* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_since_with_provider_result;
    temporal_rs_ZonedDateTime_since_with_provider_result temporal_rs_ZonedDateTime_since_with_provider(const temporal_rs::capi::ZonedDateTime* self, const temporal_rs::capi::ZonedDateTime* other, temporal_rs::capi::DifferenceSettings settings, const temporal_rs::capi::Provider* p);

    typedef struct temporal_rs_ZonedDateTime_round_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_round_result;
    temporal_rs_ZonedDateTime_round_result temporal_rs_ZonedDateTime_round(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::RoundingOptions options);

    typedef struct temporal_rs_ZonedDateTime_round_with_provider_result {union {temporal_rs::capi::ZonedDateTime* ok; temporal_rs::capi::TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_round_with_provider_result;
    temporal_rs_ZonedDateTime_round_with_provider_result temporal_rs_ZonedDateTime_round_with_provider(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::capi::RoundingOptions options, const temporal_rs::capi::Provider* p);

    uint8_t temporal_rs_ZonedDateTime_hour(const temporal_rs::capi::ZonedDateTime* self);

    uint8_t temporal_rs_ZonedDateTime_minute(const temporal_rs::capi::ZonedDateTime* self);

    uint8_t temporal_rs_ZonedDateTime_second(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_millisecond(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_microsecond(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_nanosecond(const temporal_rs::capi::ZonedDateTime* self);

    const temporal_rs::capi::Calendar* temporal_rs_ZonedDateTime_calendar(const temporal_rs::capi::ZonedDateTime* self);

    int32_t temporal_rs_ZonedDateTime_year(const temporal_rs::capi::ZonedDateTime* self);

    uint8_t temporal_rs_ZonedDateTime_month(const temporal_rs::capi::ZonedDateTime* self);

    void temporal_rs_ZonedDateTime_month_code(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    uint8_t temporal_rs_ZonedDateTime_day(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_day_of_week(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_day_of_year(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_week_of_year_result;
    temporal_rs_ZonedDateTime_week_of_year_result temporal_rs_ZonedDateTime_week_of_year(const temporal_rs::capi::ZonedDateTime* self);

    typedef struct temporal_rs_ZonedDateTime_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_year_of_week_result;
    temporal_rs_ZonedDateTime_year_of_week_result temporal_rs_ZonedDateTime_year_of_week(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_days_in_week(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_days_in_month(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_days_in_year(const temporal_rs::capi::ZonedDateTime* self);

    uint16_t temporal_rs_ZonedDateTime_months_in_year(const temporal_rs::capi::ZonedDateTime* self);

    bool temporal_rs_ZonedDateTime_in_leap_year(const temporal_rs::capi::ZonedDateTime* self);

    void temporal_rs_ZonedDateTime_era(const temporal_rs::capi::ZonedDateTime* self, temporal_rs::diplomat::capi::DiplomatWrite* write);

    typedef struct temporal_rs_ZonedDateTime_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_era_year_result;
    temporal_rs_ZonedDateTime_era_year_result temporal_rs_ZonedDateTime_era_year(const temporal_rs::capi::ZonedDateTime* self);

    temporal_rs::capi::ZonedDateTime* temporal_rs_ZonedDateTime_clone(const temporal_rs::capi::ZonedDateTime* self);

    void temporal_rs_ZonedDateTime_destroy(ZonedDateTime* self);

    } // extern "C"
} // namespace capi
} // namespace

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::try_new(temporal_rs::I128Nanoseconds nanosecond, temporal_rs::AnyCalendarKind calendar, temporal_rs::TimeZone time_zone) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_try_new(nanosecond.AsFFI(),
        calendar.AsFFI(),
        time_zone.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::try_new_with_provider(temporal_rs::I128Nanoseconds nanosecond, temporal_rs::AnyCalendarKind calendar, temporal_rs::TimeZone time_zone, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_try_new_with_provider(nanosecond.AsFFI(),
        calendar.AsFFI(),
        time_zone.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_partial(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_partial(partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
        disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
        offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_partial_with_provider(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::ArithmeticOverflow> overflow, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_partial_with_provider(partial.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
        disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
        offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_parsed(const temporal_rs::ParsedZonedDateTime& parsed, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_option) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_parsed(parsed.AsFFI(),
        disambiguation.AsFFI(),
        offset_option.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_parsed_with_provider(const temporal_rs::ParsedZonedDateTime& parsed, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_option, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_parsed_with_provider(parsed.AsFFI(),
        disambiguation.AsFFI(),
        offset_option.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_utf8(std::string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_utf8({s.data(), s.size()},
        disambiguation.AsFFI(),
        offset_disambiguation.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_utf8_with_provider(std::string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_utf8_with_provider({s.data(), s.size()},
        disambiguation.AsFFI(),
        offset_disambiguation.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_utf16(std::u16string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_utf16({s.data(), s.size()},
        disambiguation.AsFFI(),
        offset_disambiguation.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_utf16_with_provider(std::u16string_view s, temporal_rs::Disambiguation disambiguation, temporal_rs::OffsetDisambiguation offset_disambiguation, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_utf16_with_provider({s.data(), s.size()},
        disambiguation.AsFFI(),
        offset_disambiguation.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline int64_t temporal_rs::ZonedDateTime::epoch_milliseconds() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_epoch_milliseconds(this->AsFFI());
    return result;
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_epoch_milliseconds(int64_t ms, temporal_rs::TimeZone tz) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_epoch_milliseconds(ms,
        tz.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_epoch_milliseconds_with_provider(int64_t ms, temporal_rs::TimeZone tz, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider(ms,
        tz.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_epoch_nanoseconds(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_epoch_nanoseconds(ns.AsFFI(),
        tz.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::from_epoch_nanoseconds_with_provider(temporal_rs::I128Nanoseconds ns, temporal_rs::TimeZone tz, const temporal_rs::Provider& p) {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider(ns.AsFFI(),
        tz.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::I128Nanoseconds temporal_rs::ZonedDateTime::epoch_nanoseconds() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_epoch_nanoseconds(this->AsFFI());
    return temporal_rs::I128Nanoseconds::FromFFI(result);
}

inline int64_t temporal_rs::ZonedDateTime::offset_nanoseconds() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_offset_nanoseconds(this->AsFFI());
    return result;
}

inline std::unique_ptr<temporal_rs::Instant> temporal_rs::ZonedDateTime::to_instant() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_instant(this->AsFFI());
    return std::unique_ptr<temporal_rs::Instant>(temporal_rs::Instant::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with(this->AsFFI(),
        partial.AsFFI(),
        disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
        offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_with_provider(temporal_rs::PartialZonedDateTime partial, std::optional<temporal_rs::Disambiguation> disambiguation, std::optional<temporal_rs::OffsetDisambiguation> offset_option, std::optional<temporal_rs::ArithmeticOverflow> overflow, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_with_provider(this->AsFFI(),
        partial.AsFFI(),
        disambiguation.has_value() ? (temporal_rs::capi::Disambiguation_option{ { disambiguation.value().AsFFI() }, true }) : (temporal_rs::capi::Disambiguation_option{ {}, false }),
        offset_option.has_value() ? (temporal_rs::capi::OffsetDisambiguation_option{ { offset_option.value().AsFFI() }, true }) : (temporal_rs::capi::OffsetDisambiguation_option{ {}, false }),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_timezone(temporal_rs::TimeZone zone) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_timezone(this->AsFFI(),
        zone.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_timezone_with_provider(temporal_rs::TimeZone zone, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_timezone_with_provider(this->AsFFI(),
        zone.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::TimeZone temporal_rs::ZonedDateTime::timezone() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_timezone(this->AsFFI());
    return temporal_rs::TimeZone::FromFFI(result);
}

inline int8_t temporal_rs::ZonedDateTime::compare_instant(const temporal_rs::ZonedDateTime& other) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_compare_instant(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline bool temporal_rs::ZonedDateTime::equals(const temporal_rs::ZonedDateTime& other) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_equals(this->AsFFI(),
        other.AsFFI());
    return result;
}

inline temporal_rs::diplomat::result<bool, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::equals_with_provider(const temporal_rs::ZonedDateTime& other, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_equals_with_provider(this->AsFFI(),
        other.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<bool, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<bool>(result.ok)) : temporal_rs::diplomat::result<bool, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::offset() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_offset(this->AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::offset_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_offset(this->AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::start_of_day() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_start_of_day(this->AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::start_of_day_with_provider(const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_start_of_day_with_provider(this->AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::get_time_zone_transition(temporal_rs::TransitionDirection direction) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_get_time_zone_transition(this->AsFFI(),
        direction.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::get_time_zone_transition_with_provider(temporal_rs::TransitionDirection direction, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider(this->AsFFI(),
        direction.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<double, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::hours_in_day() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_hours_in_day(this->AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<double>(result.ok)) : temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<double, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::hours_in_day_with_provider(const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_hours_in_day_with_provider(this->AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<double>(result.ok)) : temporal_rs::diplomat::result<double, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::PlainDateTime> temporal_rs::ZonedDateTime::to_plain_datetime() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_plain_datetime(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainDateTime>(temporal_rs::PlainDateTime::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::PlainDate> temporal_rs::ZonedDateTime::to_plain_date() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_plain_date(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainDate>(temporal_rs::PlainDate::FromFFI(result));
}

inline std::unique_ptr<temporal_rs::PlainTime> temporal_rs::ZonedDateTime::to_plain_time() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_plain_time(this->AsFFI());
    return std::unique_ptr<temporal_rs::PlainTime>(temporal_rs::PlainTime::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_ixdtf_string(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_ixdtf_string(this->AsFFI(),
        display_offset.AsFFI(),
        display_timezone.AsFFI(),
        display_calendar.AsFFI(),
        options.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_ixdtf_string_write(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_ixdtf_string(this->AsFFI(),
        display_offset.AsFFI(),
        display_timezone.AsFFI(),
        display_calendar.AsFFI(),
        options.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_ixdtf_string_with_provider(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options, const temporal_rs::Provider& p) const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider(this->AsFFI(),
        display_offset.AsFFI(),
        display_timezone.AsFFI(),
        display_calendar.AsFFI(),
        options.AsFFI(),
        p.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::string>(std::move(output))) : temporal_rs::diplomat::result<std::string, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}
template<typename W>
inline temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::to_ixdtf_string_with_provider_write(temporal_rs::DisplayOffset display_offset, temporal_rs::DisplayTimeZone display_timezone, temporal_rs::DisplayCalendar display_calendar, temporal_rs::ToStringRoundingOptions options, const temporal_rs::Provider& p, W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider(this->AsFFI(),
        display_offset.AsFFI(),
        display_timezone.AsFFI(),
        display_calendar.AsFFI(),
        options.AsFFI(),
        p.AsFFI(),
        &write);
    return result.is_ok ? temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::monostate>()) : temporal_rs::diplomat::result<std::monostate, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline std::unique_ptr<temporal_rs::ZonedDateTime> temporal_rs::ZonedDateTime::with_calendar(temporal_rs::AnyCalendarKind calendar) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_calendar(this->AsFFI(),
        calendar.AsFFI());
    return std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_plain_time(const temporal_rs::PlainTime* time) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_plain_time(this->AsFFI(),
        time ? time->AsFFI() : nullptr);
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::with_plain_time_and_provider(const temporal_rs::PlainTime* time, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_with_plain_time_and_provider(this->AsFFI(),
        time ? time->AsFFI() : nullptr,
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::add(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_add(this->AsFFI(),
        duration.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::add_with_provider(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_add_with_provider(this->AsFFI(),
        duration.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::subtract(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_subtract(this->AsFFI(),
        duration.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }));
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::subtract_with_provider(const temporal_rs::Duration& duration, std::optional<temporal_rs::ArithmeticOverflow> overflow, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_subtract_with_provider(this->AsFFI(),
        duration.AsFFI(),
        overflow.has_value() ? (temporal_rs::capi::ArithmeticOverflow_option{ { overflow.value().AsFFI() }, true }) : (temporal_rs::capi::ArithmeticOverflow_option{ {}, false }),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::until(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_until(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::until_with_provider(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_until_with_provider(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::since(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_since(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::since_with_provider(const temporal_rs::ZonedDateTime& other, temporal_rs::DifferenceSettings settings, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_since_with_provider(this->AsFFI(),
        other.AsFFI(),
        settings.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::Duration>>(std::unique_ptr<temporal_rs::Duration>(temporal_rs::Duration::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::Duration>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::round(temporal_rs::RoundingOptions options) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_round(this->AsFFI(),
        options.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError> temporal_rs::ZonedDateTime::round_with_provider(temporal_rs::RoundingOptions options, const temporal_rs::Provider& p) const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_round_with_provider(this->AsFFI(),
        options.AsFFI(),
        p.AsFFI());
    return result.is_ok ? temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Ok<std::unique_ptr<temporal_rs::ZonedDateTime>>(std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result.ok)))) : temporal_rs::diplomat::result<std::unique_ptr<temporal_rs::ZonedDateTime>, temporal_rs::TemporalError>(temporal_rs::diplomat::Err<temporal_rs::TemporalError>(temporal_rs::TemporalError::FromFFI(result.err)));
}

inline uint8_t temporal_rs::ZonedDateTime::hour() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_hour(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::ZonedDateTime::minute() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_minute(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::ZonedDateTime::second() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_second(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::millisecond() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_millisecond(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::microsecond() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_microsecond(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::nanosecond() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_nanosecond(this->AsFFI());
    return result;
}

inline const temporal_rs::Calendar& temporal_rs::ZonedDateTime::calendar() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_calendar(this->AsFFI());
    return *temporal_rs::Calendar::FromFFI(result);
}

inline int32_t temporal_rs::ZonedDateTime::year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_year(this->AsFFI());
    return result;
}

inline uint8_t temporal_rs::ZonedDateTime::month() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_month(this->AsFFI());
    return result;
}

inline std::string temporal_rs::ZonedDateTime::month_code() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_ZonedDateTime_month_code(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::ZonedDateTime::month_code_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_ZonedDateTime_month_code(this->AsFFI(),
        &write);
}

inline uint8_t temporal_rs::ZonedDateTime::day() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_day(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::day_of_week() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_day_of_week(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::day_of_year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_day_of_year(this->AsFFI());
    return result;
}

inline std::optional<uint8_t> temporal_rs::ZonedDateTime::week_of_year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_week_of_year(this->AsFFI());
    return result.is_ok ? std::optional<uint8_t>(result.ok) : std::nullopt;
}

inline std::optional<int32_t> temporal_rs::ZonedDateTime::year_of_week() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_year_of_week(this->AsFFI());
    return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline uint16_t temporal_rs::ZonedDateTime::days_in_week() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_days_in_week(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::days_in_month() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_days_in_month(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::days_in_year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_days_in_year(this->AsFFI());
    return result;
}

inline uint16_t temporal_rs::ZonedDateTime::months_in_year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_months_in_year(this->AsFFI());
    return result;
}

inline bool temporal_rs::ZonedDateTime::in_leap_year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_in_leap_year(this->AsFFI());
    return result;
}

inline std::string temporal_rs::ZonedDateTime::era() const {
    std::string output;
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteFromString(output);
    temporal_rs::capi::temporal_rs_ZonedDateTime_era(this->AsFFI(),
        &write);
    return output;
}
template<typename W>
inline void temporal_rs::ZonedDateTime::era_write(W& writeable) const {
    temporal_rs::diplomat::capi::DiplomatWrite write = temporal_rs::diplomat::WriteTrait<W>::Construct(writeable);
    temporal_rs::capi::temporal_rs_ZonedDateTime_era(this->AsFFI(),
        &write);
}

inline std::optional<int32_t> temporal_rs::ZonedDateTime::era_year() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_era_year(this->AsFFI());
    return result.is_ok ? std::optional<int32_t>(result.ok) : std::nullopt;
}

inline std::unique_ptr<temporal_rs::ZonedDateTime> temporal_rs::ZonedDateTime::clone() const {
    auto result = temporal_rs::capi::temporal_rs_ZonedDateTime_clone(this->AsFFI());
    return std::unique_ptr<temporal_rs::ZonedDateTime>(temporal_rs::ZonedDateTime::FromFFI(result));
}

inline const temporal_rs::capi::ZonedDateTime* temporal_rs::ZonedDateTime::AsFFI() const {
    return reinterpret_cast<const temporal_rs::capi::ZonedDateTime*>(this);
}

inline temporal_rs::capi::ZonedDateTime* temporal_rs::ZonedDateTime::AsFFI() {
    return reinterpret_cast<temporal_rs::capi::ZonedDateTime*>(this);
}

inline const temporal_rs::ZonedDateTime* temporal_rs::ZonedDateTime::FromFFI(const temporal_rs::capi::ZonedDateTime* ptr) {
    return reinterpret_cast<const temporal_rs::ZonedDateTime*>(ptr);
}

inline temporal_rs::ZonedDateTime* temporal_rs::ZonedDateTime::FromFFI(temporal_rs::capi::ZonedDateTime* ptr) {
    return reinterpret_cast<temporal_rs::ZonedDateTime*>(ptr);
}

inline void temporal_rs::ZonedDateTime::operator delete(void* ptr) {
    temporal_rs::capi::temporal_rs_ZonedDateTime_destroy(reinterpret_cast<temporal_rs::capi::ZonedDateTime*>(ptr));
}


#endif // TEMPORAL_RS_ZonedDateTime_HPP
