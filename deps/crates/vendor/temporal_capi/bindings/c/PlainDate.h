#ifndef PlainDate_H
#define PlainDate_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "AnyCalendarKind.d.h"
#include "ArithmeticOverflow.d.h"
#include "Calendar.d.h"
#include "DifferenceSettings.d.h"
#include "DisplayCalendar.d.h"
#include "Duration.d.h"
#include "I128Nanoseconds.d.h"
#include "ParsedDate.d.h"
#include "PartialDate.d.h"
#include "PlainDateTime.d.h"
#include "PlainMonthDay.d.h"
#include "PlainTime.d.h"
#include "PlainYearMonth.d.h"
#include "Provider.d.h"
#include "TemporalError.d.h"
#include "TimeZone.d.h"
#include "ZonedDateTime.d.h"

#include "PlainDate.d.h"






typedef struct temporal_rs_PlainDate_try_new_constrain_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_try_new_constrain_result;
temporal_rs_PlainDate_try_new_constrain_result temporal_rs_PlainDate_try_new_constrain(int32_t year, uint8_t month, uint8_t day, AnyCalendarKind calendar);

typedef struct temporal_rs_PlainDate_try_new_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_try_new_result;
temporal_rs_PlainDate_try_new_result temporal_rs_PlainDate_try_new(int32_t year, uint8_t month, uint8_t day, AnyCalendarKind calendar);

typedef struct temporal_rs_PlainDate_try_new_with_overflow_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_try_new_with_overflow_result;
temporal_rs_PlainDate_try_new_with_overflow_result temporal_rs_PlainDate_try_new_with_overflow(int32_t year, uint8_t month, uint8_t day, AnyCalendarKind calendar, ArithmeticOverflow overflow);

typedef struct temporal_rs_PlainDate_from_partial_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_partial_result;
temporal_rs_PlainDate_from_partial_result temporal_rs_PlainDate_from_partial(PartialDate partial, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDate_from_parsed_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_parsed_result;
temporal_rs_PlainDate_from_parsed_result temporal_rs_PlainDate_from_parsed(const ParsedDate* parsed);

typedef struct temporal_rs_PlainDate_from_epoch_milliseconds_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_epoch_milliseconds_result;
temporal_rs_PlainDate_from_epoch_milliseconds_result temporal_rs_PlainDate_from_epoch_milliseconds(int64_t ms, TimeZone tz);

typedef struct temporal_rs_PlainDate_from_epoch_milliseconds_with_provider_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_epoch_milliseconds_with_provider_result;
temporal_rs_PlainDate_from_epoch_milliseconds_with_provider_result temporal_rs_PlainDate_from_epoch_milliseconds_with_provider(int64_t ms, TimeZone tz, const Provider* p);

typedef struct temporal_rs_PlainDate_from_epoch_nanoseconds_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_epoch_nanoseconds_result;
temporal_rs_PlainDate_from_epoch_nanoseconds_result temporal_rs_PlainDate_from_epoch_nanoseconds(I128Nanoseconds ns, TimeZone tz);

typedef struct temporal_rs_PlainDate_from_epoch_nanoseconds_with_provider_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_epoch_nanoseconds_with_provider_result;
temporal_rs_PlainDate_from_epoch_nanoseconds_with_provider_result temporal_rs_PlainDate_from_epoch_nanoseconds_with_provider(I128Nanoseconds ns, TimeZone tz, const Provider* p);

typedef struct temporal_rs_PlainDate_with_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_with_result;
temporal_rs_PlainDate_with_result temporal_rs_PlainDate_with(const PlainDate* self, PartialDate partial, ArithmeticOverflow_option overflow);

PlainDate* temporal_rs_PlainDate_with_calendar(const PlainDate* self, AnyCalendarKind calendar);

typedef struct temporal_rs_PlainDate_from_utf8_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_utf8_result;
temporal_rs_PlainDate_from_utf8_result temporal_rs_PlainDate_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_PlainDate_from_utf16_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_from_utf16_result;
temporal_rs_PlainDate_from_utf16_result temporal_rs_PlainDate_from_utf16(DiplomatString16View s);

const Calendar* temporal_rs_PlainDate_calendar(const PlainDate* self);

bool temporal_rs_PlainDate_is_valid(const PlainDate* self);

typedef struct temporal_rs_PlainDate_add_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_add_result;
temporal_rs_PlainDate_add_result temporal_rs_PlainDate_add(const PlainDate* self, const Duration* duration, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDate_subtract_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_subtract_result;
temporal_rs_PlainDate_subtract_result temporal_rs_PlainDate_subtract(const PlainDate* self, const Duration* duration, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainDate_until_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_until_result;
temporal_rs_PlainDate_until_result temporal_rs_PlainDate_until(const PlainDate* self, const PlainDate* other, DifferenceSettings settings);

typedef struct temporal_rs_PlainDate_since_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_since_result;
temporal_rs_PlainDate_since_result temporal_rs_PlainDate_since(const PlainDate* self, const PlainDate* other, DifferenceSettings settings);

bool temporal_rs_PlainDate_equals(const PlainDate* self, const PlainDate* other);

int8_t temporal_rs_PlainDate_compare(const PlainDate* one, const PlainDate* two);

int32_t temporal_rs_PlainDate_year(const PlainDate* self);

uint8_t temporal_rs_PlainDate_month(const PlainDate* self);

void temporal_rs_PlainDate_month_code(const PlainDate* self, DiplomatWrite* write);

uint8_t temporal_rs_PlainDate_day(const PlainDate* self);

uint16_t temporal_rs_PlainDate_day_of_week(const PlainDate* self);

uint16_t temporal_rs_PlainDate_day_of_year(const PlainDate* self);

typedef struct temporal_rs_PlainDate_week_of_year_result {union {uint8_t ok; }; bool is_ok;} temporal_rs_PlainDate_week_of_year_result;
temporal_rs_PlainDate_week_of_year_result temporal_rs_PlainDate_week_of_year(const PlainDate* self);

typedef struct temporal_rs_PlainDate_year_of_week_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDate_year_of_week_result;
temporal_rs_PlainDate_year_of_week_result temporal_rs_PlainDate_year_of_week(const PlainDate* self);

uint16_t temporal_rs_PlainDate_days_in_week(const PlainDate* self);

uint16_t temporal_rs_PlainDate_days_in_month(const PlainDate* self);

uint16_t temporal_rs_PlainDate_days_in_year(const PlainDate* self);

uint16_t temporal_rs_PlainDate_months_in_year(const PlainDate* self);

bool temporal_rs_PlainDate_in_leap_year(const PlainDate* self);

void temporal_rs_PlainDate_era(const PlainDate* self, DiplomatWrite* write);

typedef struct temporal_rs_PlainDate_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainDate_era_year_result;
temporal_rs_PlainDate_era_year_result temporal_rs_PlainDate_era_year(const PlainDate* self);

typedef struct temporal_rs_PlainDate_to_plain_date_time_result {union {PlainDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_plain_date_time_result;
temporal_rs_PlainDate_to_plain_date_time_result temporal_rs_PlainDate_to_plain_date_time(const PlainDate* self, const PlainTime* time);

typedef struct temporal_rs_PlainDate_to_plain_month_day_result {union {PlainMonthDay* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_plain_month_day_result;
temporal_rs_PlainDate_to_plain_month_day_result temporal_rs_PlainDate_to_plain_month_day(const PlainDate* self);

typedef struct temporal_rs_PlainDate_to_plain_year_month_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_plain_year_month_result;
temporal_rs_PlainDate_to_plain_year_month_result temporal_rs_PlainDate_to_plain_year_month(const PlainDate* self);

typedef struct temporal_rs_PlainDate_to_zoned_date_time_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_zoned_date_time_result;
temporal_rs_PlainDate_to_zoned_date_time_result temporal_rs_PlainDate_to_zoned_date_time(const PlainDate* self, TimeZone time_zone, const PlainTime* time);

typedef struct temporal_rs_PlainDate_to_zoned_date_time_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainDate_to_zoned_date_time_with_provider_result;
temporal_rs_PlainDate_to_zoned_date_time_with_provider_result temporal_rs_PlainDate_to_zoned_date_time_with_provider(const PlainDate* self, TimeZone time_zone, const PlainTime* time, const Provider* p);

void temporal_rs_PlainDate_to_ixdtf_string(const PlainDate* self, DisplayCalendar display_calendar, DiplomatWrite* write);

PlainDate* temporal_rs_PlainDate_clone(const PlainDate* self);

void temporal_rs_PlainDate_destroy(PlainDate* self);





#endif // PlainDate_H
