#ifndef PlainDateTime_H
#define PlainDateTime_H

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
#include "Duration.d.h"
#include "I128Nanoseconds.d.h"
#include "ParsedDateTime.d.h"
#include "PartialDateTime.d.h"
#include "PlainDate.d.h"
#include "PlainTime.d.h"
#include "Provider.d.h"
#include "RoundingOptions.d.h"
#include "TemporalError.d.h"
#include "TimeZone.d.h"
#include "ToStringRoundingOptions.d.h"
#include "ZonedDateTime.d.h"

#include "PlainDateTime.d.h"






typedef struct temporal_rs_PlainDateTime_try_new_constrain_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_try_new_constrain_result;
temporal_rs_PlainDateTime_try_new_constrain_result temporal_rs_PlainDateTime_try_new_constrain(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, AnyCalendarKind calendar);

typedef struct temporal_rs_PlainDateTime_try_new_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_try_new_result;
temporal_rs_PlainDateTime_try_new_result temporal_rs_PlainDateTime_try_new(int32_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond, uint16_t microsecond, uint16_t nanosecond, AnyCalendarKind calendar);

typedef struct temporal_rs_PlainDateTime_from_partial_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_partial_result;
temporal_rs_PlainDateTime_from_partial_result temporal_rs_PlainDateTime_from_partial(PartialDateTime partial, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDateTime_from_parsed_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_parsed_result;
temporal_rs_PlainDateTime_from_parsed_result temporal_rs_PlainDateTime_from_parsed(const ParsedDateTime* parsed);

typedef struct temporal_rs_PlainDateTime_from_epoch_milliseconds_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_milliseconds_result;
temporal_rs_PlainDateTime_from_epoch_milliseconds_result temporal_rs_PlainDateTime_from_epoch_milliseconds(int64_t ms, TimeZone tz);

typedef struct temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider_result;
temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider_result temporal_rs_PlainDateTime_from_epoch_milliseconds_with_provider(int64_t ms, TimeZone tz, const Provider* p);

typedef struct temporal_rs_PlainDateTime_from_epoch_nanoseconds_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_nanoseconds_result;
temporal_rs_PlainDateTime_from_epoch_nanoseconds_result temporal_rs_PlainDateTime_from_epoch_nanoseconds(I128Nanoseconds ns, TimeZone tz);

typedef struct temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider_result;
temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider_result temporal_rs_PlainDateTime_from_epoch_nanoseconds_with_provider(I128Nanoseconds ns, TimeZone tz, const Provider* p);

typedef struct temporal_rs_PlainDateTime_with_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_with_result;
temporal_rs_PlainDateTime_with_result temporal_rs_PlainDateTime_with(const PlainDateTime* self, PartialDateTime partial, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDateTime_with_time_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_with_time_result;
temporal_rs_PlainDateTime_with_time_result temporal_rs_PlainDateTime_with_time(const PlainDateTime* self, const PlainTime* time);

PlainDateTime* temporal_rs_PlainDateTime_with_calendar(const PlainDateTime* self, AnyCalendarKind calendar);

typedef struct temporal_rs_PlainDateTime_from_utf8_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_utf8_result;
temporal_rs_PlainDateTime_from_utf8_result temporal_rs_PlainDateTime_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_PlainDateTime_from_utf16_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_from_utf16_result;
temporal_rs_PlainDateTime_from_utf16_result temporal_rs_PlainDateTime_from_utf16(DiplomatString16View s);

uint8_t temporal_rs_PlainDateTime_hour(const PlainDateTime* self);

uint8_t temporal_rs_PlainDateTime_minute(const PlainDateTime* self);

uint8_t temporal_rs_PlainDateTime_second(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_millisecond(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_microsecond(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_nanosecond(const PlainDateTime* self);

const Calendar* temporal_rs_PlainDateTime_calendar(const PlainDateTime* self);

int32_t temporal_rs_PlainDateTime_year(const PlainDateTime* self);

uint8_t temporal_rs_PlainDateTime_month(const PlainDateTime* self);

void temporal_rs_PlainDateTime_month_code(const PlainDateTime* self, DiplomatWrite* write);

uint8_t temporal_rs_PlainDateTime_day(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_day_of_week(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_day_of_year(const PlainDateTime* self);

typedef struct temporal_rs_PlainDateTime_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_PlainDateTime_week_of_year_result;
temporal_rs_PlainDateTime_week_of_year_result temporal_rs_PlainDateTime_week_of_year(const PlainDateTime* self);

typedef struct temporal_rs_PlainDateTime_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDateTime_year_of_week_result;
temporal_rs_PlainDateTime_year_of_week_result temporal_rs_PlainDateTime_year_of_week(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_days_in_week(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_days_in_month(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_days_in_year(const PlainDateTime* self);

uint16_t temporal_rs_PlainDateTime_months_in_year(const PlainDateTime* self);

bool temporal_rs_PlainDateTime_in_leap_year(const PlainDateTime* self);

void temporal_rs_PlainDateTime_era(const PlainDateTime* self, DiplomatWrite* write);

typedef struct temporal_rs_PlainDateTime_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDateTime_era_year_result;
temporal_rs_PlainDateTime_era_year_result temporal_rs_PlainDateTime_era_year(const PlainDateTime* self);

typedef struct temporal_rs_PlainDateTime_add_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_add_result;
temporal_rs_PlainDateTime_add_result temporal_rs_PlainDateTime_add(const PlainDateTime* self, const Duration* duration, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDateTime_subtract_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_subtract_result;
temporal_rs_PlainDateTime_subtract_result temporal_rs_PlainDateTime_subtract(const PlainDateTime* self, const Duration* duration, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDateTime_until_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_until_result;
temporal_rs_PlainDateTime_until_result temporal_rs_PlainDateTime_until(const PlainDateTime* self, const PlainDateTime* other, DifferenceSettings settings);

typedef struct temporal_rs_PlainDateTime_since_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_since_result;
temporal_rs_PlainDateTime_since_result temporal_rs_PlainDateTime_since(const PlainDateTime* self, const PlainDateTime* other, DifferenceSettings settings);

bool temporal_rs_PlainDateTime_equals(const PlainDateTime* self, const PlainDateTime* other);

int8_t temporal_rs_PlainDateTime_compare(const PlainDateTime* one, const PlainDateTime* two);

typedef struct temporal_rs_PlainDateTime_round_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_round_result;
temporal_rs_PlainDateTime_round_result temporal_rs_PlainDateTime_round(const PlainDateTime* self, RoundingOptions options);

PlainDate* temporal_rs_PlainDateTime_to_plain_date(const PlainDateTime* self);

PlainTime* temporal_rs_PlainDateTime_to_plain_time(const PlainDateTime* self);

typedef struct temporal_rs_PlainDateTime_to_zoned_date_time_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_to_zoned_date_time_result;
temporal_rs_PlainDateTime_to_zoned_date_time_result temporal_rs_PlainDateTime_to_zoned_date_time(const PlainDateTime* self, TimeZone time_zone, Disambiguation disambiguation);

typedef struct temporal_rs_PlainDateTime_to_zoned_date_time_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_to_zoned_date_time_with_provider_result;
temporal_rs_PlainDateTime_to_zoned_date_time_with_provider_result temporal_rs_PlainDateTime_to_zoned_date_time_with_provider(const PlainDateTime* self, TimeZone time_zone, Disambiguation disambiguation, const Provider* p);

typedef struct temporal_rs_PlainDateTime_to_ixdtf_string_result {union { TemporalError err;}; bool is_ok;} temporal_rs_PlainDateTime_to_ixdtf_string_result;
temporal_rs_PlainDateTime_to_ixdtf_string_result temporal_rs_PlainDateTime_to_ixdtf_string(const PlainDateTime* self, ToStringRoundingOptions options, DisplayCalendar display_calendar, DiplomatWrite* write);

PlainDateTime* temporal_rs_PlainDateTime_clone(const PlainDateTime* self);

void temporal_rs_PlainDateTime_destroy(PlainDateTime* self);





#endif // PlainDateTime_H
