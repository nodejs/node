#ifndef ZonedDateTime_H
#define ZonedDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "AnyCalendarKind.d.h"
#include "ArithmeticOverflow.d.h"
#include "Calendar.d.h"
#include "DifferenceSettings.d.h"
#include "Disambiguation.d.h"
#include "DisplayCalendar.d.h"
#include "DisplayOffset.d.h"
#include "DisplayTimeZone.d.h"
#include "Duration.d.h"
#include "I128Nanoseconds.d.h"
#include "Instant.d.h"
#include "OffsetDisambiguation.d.h"
#include "ParsedZonedDateTime.d.h"
#include "PartialZonedDateTime.d.h"
#include "PlainDate.d.h"
#include "PlainDateTime.d.h"
#include "PlainTime.d.h"
#include "Provider.d.h"
#include "RoundingOptions.d.h"
#include "TemporalError.d.h"
#include "TimeZone.d.h"
#include "ToStringRoundingOptions.d.h"
#include "TransitionDirection.d.h"

#include "ZonedDateTime.d.h"






typedef struct temporal_rs_ZonedDateTime_try_new_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_try_new_result;
temporal_rs_ZonedDateTime_try_new_result temporal_rs_ZonedDateTime_try_new(I128Nanoseconds nanosecond, AnyCalendarKind calendar, TimeZone time_zone);

typedef struct temporal_rs_ZonedDateTime_try_new_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_try_new_with_provider_result;
temporal_rs_ZonedDateTime_try_new_with_provider_result temporal_rs_ZonedDateTime_try_new_with_provider(I128Nanoseconds nanosecond, AnyCalendarKind calendar, TimeZone time_zone, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_from_partial_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_partial_result;
temporal_rs_ZonedDateTime_from_partial_result temporal_rs_ZonedDateTime_from_partial(PartialZonedDateTime partial, ArithmeticOverflow_option overflow, Disambiguation_option disambiguation, OffsetDisambiguation_option offset_option);

typedef struct temporal_rs_ZonedDateTime_from_partial_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_partial_with_provider_result;
temporal_rs_ZonedDateTime_from_partial_with_provider_result temporal_rs_ZonedDateTime_from_partial_with_provider(PartialZonedDateTime partial, ArithmeticOverflow_option overflow, Disambiguation_option disambiguation, OffsetDisambiguation_option offset_option, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_from_parsed_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_parsed_result;
temporal_rs_ZonedDateTime_from_parsed_result temporal_rs_ZonedDateTime_from_parsed(const ParsedZonedDateTime* parsed, Disambiguation disambiguation, OffsetDisambiguation offset_option);

typedef struct temporal_rs_ZonedDateTime_from_parsed_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_parsed_with_provider_result;
temporal_rs_ZonedDateTime_from_parsed_with_provider_result temporal_rs_ZonedDateTime_from_parsed_with_provider(const ParsedZonedDateTime* parsed, Disambiguation disambiguation, OffsetDisambiguation offset_option, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_from_utf8_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf8_result;
temporal_rs_ZonedDateTime_from_utf8_result temporal_rs_ZonedDateTime_from_utf8(DiplomatStringView s, Disambiguation disambiguation, OffsetDisambiguation offset_disambiguation);

typedef struct temporal_rs_ZonedDateTime_from_utf8_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf8_with_provider_result;
temporal_rs_ZonedDateTime_from_utf8_with_provider_result temporal_rs_ZonedDateTime_from_utf8_with_provider(DiplomatStringView s, Disambiguation disambiguation, OffsetDisambiguation offset_disambiguation, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_from_utf16_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf16_result;
temporal_rs_ZonedDateTime_from_utf16_result temporal_rs_ZonedDateTime_from_utf16(DiplomatString16View s, Disambiguation disambiguation, OffsetDisambiguation offset_disambiguation);

typedef struct temporal_rs_ZonedDateTime_from_utf16_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_utf16_with_provider_result;
temporal_rs_ZonedDateTime_from_utf16_with_provider_result temporal_rs_ZonedDateTime_from_utf16_with_provider(DiplomatString16View s, Disambiguation disambiguation, OffsetDisambiguation offset_disambiguation, const Provider* p);

int64_t temporal_rs_ZonedDateTime_epoch_milliseconds(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_from_epoch_milliseconds_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_milliseconds_result;
temporal_rs_ZonedDateTime_from_epoch_milliseconds_result temporal_rs_ZonedDateTime_from_epoch_milliseconds(int64_t ms, TimeZone tz);

typedef struct temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider_result;
temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider_result temporal_rs_ZonedDateTime_from_epoch_milliseconds_with_provider(int64_t ms, TimeZone tz, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_from_epoch_nanoseconds_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_nanoseconds_result;
temporal_rs_ZonedDateTime_from_epoch_nanoseconds_result temporal_rs_ZonedDateTime_from_epoch_nanoseconds(I128Nanoseconds ns, TimeZone tz);

typedef struct temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider_result;
temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider_result temporal_rs_ZonedDateTime_from_epoch_nanoseconds_with_provider(I128Nanoseconds ns, TimeZone tz, const Provider* p);

I128Nanoseconds temporal_rs_ZonedDateTime_epoch_nanoseconds(const ZonedDateTime* self);

int64_t temporal_rs_ZonedDateTime_offset_nanoseconds(const ZonedDateTime* self);

Instant* temporal_rs_ZonedDateTime_to_instant(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_with_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_result;
temporal_rs_ZonedDateTime_with_result temporal_rs_ZonedDateTime_with(const ZonedDateTime* self, PartialZonedDateTime partial, Disambiguation_option disambiguation, OffsetDisambiguation_option offset_option, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_ZonedDateTime_with_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_with_provider_result;
temporal_rs_ZonedDateTime_with_with_provider_result temporal_rs_ZonedDateTime_with_with_provider(const ZonedDateTime* self, PartialZonedDateTime partial, Disambiguation_option disambiguation, OffsetDisambiguation_option offset_option, ArithmeticOverflow_option overflow, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_with_timezone_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_timezone_result;
temporal_rs_ZonedDateTime_with_timezone_result temporal_rs_ZonedDateTime_with_timezone(const ZonedDateTime* self, TimeZone zone);

typedef struct temporal_rs_ZonedDateTime_with_timezone_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_timezone_with_provider_result;
temporal_rs_ZonedDateTime_with_timezone_with_provider_result temporal_rs_ZonedDateTime_with_timezone_with_provider(const ZonedDateTime* self, TimeZone zone, const Provider* p);

TimeZone temporal_rs_ZonedDateTime_timezone(const ZonedDateTime* self);

int8_t temporal_rs_ZonedDateTime_compare_instant(const ZonedDateTime* self, const ZonedDateTime* other);

bool temporal_rs_ZonedDateTime_equals(const ZonedDateTime* self, const ZonedDateTime* other);

typedef struct temporal_rs_ZonedDateTime_equals_with_provider_result {union {bool ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_equals_with_provider_result;
temporal_rs_ZonedDateTime_equals_with_provider_result temporal_rs_ZonedDateTime_equals_with_provider(const ZonedDateTime* self, const ZonedDateTime* other, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_offset_result {union { TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_offset_result;
temporal_rs_ZonedDateTime_offset_result temporal_rs_ZonedDateTime_offset(const ZonedDateTime* self, DiplomatWrite* write);

typedef struct temporal_rs_ZonedDateTime_start_of_day_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_start_of_day_result;
temporal_rs_ZonedDateTime_start_of_day_result temporal_rs_ZonedDateTime_start_of_day(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_start_of_day_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_start_of_day_with_provider_result;
temporal_rs_ZonedDateTime_start_of_day_with_provider_result temporal_rs_ZonedDateTime_start_of_day_with_provider(const ZonedDateTime* self, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_get_time_zone_transition_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_get_time_zone_transition_result;
temporal_rs_ZonedDateTime_get_time_zone_transition_result temporal_rs_ZonedDateTime_get_time_zone_transition(const ZonedDateTime* self, TransitionDirection direction);

typedef struct temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider_result;
temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider_result temporal_rs_ZonedDateTime_get_time_zone_transition_with_provider(const ZonedDateTime* self, TransitionDirection direction, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_hours_in_day_result {union {double ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_hours_in_day_result;
temporal_rs_ZonedDateTime_hours_in_day_result temporal_rs_ZonedDateTime_hours_in_day(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_hours_in_day_with_provider_result {union {double ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_hours_in_day_with_provider_result;
temporal_rs_ZonedDateTime_hours_in_day_with_provider_result temporal_rs_ZonedDateTime_hours_in_day_with_provider(const ZonedDateTime* self, const Provider* p);

PlainDateTime* temporal_rs_ZonedDateTime_to_plain_datetime(const ZonedDateTime* self);

PlainDate* temporal_rs_ZonedDateTime_to_plain_date(const ZonedDateTime* self);

PlainTime* temporal_rs_ZonedDateTime_to_plain_time(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_to_ixdtf_string_result {union { TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_ixdtf_string_result;
temporal_rs_ZonedDateTime_to_ixdtf_string_result temporal_rs_ZonedDateTime_to_ixdtf_string(const ZonedDateTime* self, DisplayOffset display_offset, DisplayTimeZone display_timezone, DisplayCalendar display_calendar, ToStringRoundingOptions options, DiplomatWrite* write);

typedef struct temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider_result {union { TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider_result;
temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider_result temporal_rs_ZonedDateTime_to_ixdtf_string_with_provider(const ZonedDateTime* self, DisplayOffset display_offset, DisplayTimeZone display_timezone, DisplayCalendar display_calendar, ToStringRoundingOptions options, const Provider* p, DiplomatWrite* write);

ZonedDateTime* temporal_rs_ZonedDateTime_with_calendar(const ZonedDateTime* self, AnyCalendarKind calendar);

typedef struct temporal_rs_ZonedDateTime_with_plain_time_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_plain_time_result;
temporal_rs_ZonedDateTime_with_plain_time_result temporal_rs_ZonedDateTime_with_plain_time(const ZonedDateTime* self, const PlainTime* time);

typedef struct temporal_rs_ZonedDateTime_with_plain_time_and_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_with_plain_time_and_provider_result;
temporal_rs_ZonedDateTime_with_plain_time_and_provider_result temporal_rs_ZonedDateTime_with_plain_time_and_provider(const ZonedDateTime* self, const PlainTime* time, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_add_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_add_result;
temporal_rs_ZonedDateTime_add_result temporal_rs_ZonedDateTime_add(const ZonedDateTime* self, const Duration* duration, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_ZonedDateTime_add_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_add_with_provider_result;
temporal_rs_ZonedDateTime_add_with_provider_result temporal_rs_ZonedDateTime_add_with_provider(const ZonedDateTime* self, const Duration* duration, ArithmeticOverflow_option overflow, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_subtract_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_subtract_result;
temporal_rs_ZonedDateTime_subtract_result temporal_rs_ZonedDateTime_subtract(const ZonedDateTime* self, const Duration* duration, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_ZonedDateTime_subtract_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_subtract_with_provider_result;
temporal_rs_ZonedDateTime_subtract_with_provider_result temporal_rs_ZonedDateTime_subtract_with_provider(const ZonedDateTime* self, const Duration* duration, ArithmeticOverflow_option overflow, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_until_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_until_result;
temporal_rs_ZonedDateTime_until_result temporal_rs_ZonedDateTime_until(const ZonedDateTime* self, const ZonedDateTime* other, DifferenceSettings settings);

typedef struct temporal_rs_ZonedDateTime_until_with_provider_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_until_with_provider_result;
temporal_rs_ZonedDateTime_until_with_provider_result temporal_rs_ZonedDateTime_until_with_provider(const ZonedDateTime* self, const ZonedDateTime* other, DifferenceSettings settings, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_since_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_since_result;
temporal_rs_ZonedDateTime_since_result temporal_rs_ZonedDateTime_since(const ZonedDateTime* self, const ZonedDateTime* other, DifferenceSettings settings);

typedef struct temporal_rs_ZonedDateTime_since_with_provider_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_since_with_provider_result;
temporal_rs_ZonedDateTime_since_with_provider_result temporal_rs_ZonedDateTime_since_with_provider(const ZonedDateTime* self, const ZonedDateTime* other, DifferenceSettings settings, const Provider* p);

typedef struct temporal_rs_ZonedDateTime_round_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_round_result;
temporal_rs_ZonedDateTime_round_result temporal_rs_ZonedDateTime_round(const ZonedDateTime* self, RoundingOptions options);

typedef struct temporal_rs_ZonedDateTime_round_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ZonedDateTime_round_with_provider_result;
temporal_rs_ZonedDateTime_round_with_provider_result temporal_rs_ZonedDateTime_round_with_provider(const ZonedDateTime* self, RoundingOptions options, const Provider* p);

uint8_t temporal_rs_ZonedDateTime_hour(const ZonedDateTime* self);

uint8_t temporal_rs_ZonedDateTime_minute(const ZonedDateTime* self);

uint8_t temporal_rs_ZonedDateTime_second(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_millisecond(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_microsecond(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_nanosecond(const ZonedDateTime* self);

const Calendar* temporal_rs_ZonedDateTime_calendar(const ZonedDateTime* self);

int32_t temporal_rs_ZonedDateTime_year(const ZonedDateTime* self);

uint8_t temporal_rs_ZonedDateTime_month(const ZonedDateTime* self);

void temporal_rs_ZonedDateTime_month_code(const ZonedDateTime* self, DiplomatWrite* write);

uint8_t temporal_rs_ZonedDateTime_day(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_day_of_week(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_day_of_year(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_week_of_year_result;
temporal_rs_ZonedDateTime_week_of_year_result temporal_rs_ZonedDateTime_week_of_year(const ZonedDateTime* self);

typedef struct temporal_rs_ZonedDateTime_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_year_of_week_result;
temporal_rs_ZonedDateTime_year_of_week_result temporal_rs_ZonedDateTime_year_of_week(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_days_in_week(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_days_in_month(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_days_in_year(const ZonedDateTime* self);

uint16_t temporal_rs_ZonedDateTime_months_in_year(const ZonedDateTime* self);

bool temporal_rs_ZonedDateTime_in_leap_year(const ZonedDateTime* self);

void temporal_rs_ZonedDateTime_era(const ZonedDateTime* self, DiplomatWrite* write);

typedef struct temporal_rs_ZonedDateTime_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_ZonedDateTime_era_year_result;
temporal_rs_ZonedDateTime_era_year_result temporal_rs_ZonedDateTime_era_year(const ZonedDateTime* self);

ZonedDateTime* temporal_rs_ZonedDateTime_clone(const ZonedDateTime* self);

void temporal_rs_ZonedDateTime_destroy(ZonedDateTime* self);





#endif // ZonedDateTime_H
