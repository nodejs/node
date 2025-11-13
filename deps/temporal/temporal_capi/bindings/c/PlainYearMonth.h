#ifndef PlainYearMonth_H
#define PlainYearMonth_H

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
#include "ParsedDate.d.h"
#include "PartialDate.d.h"
#include "PlainDate.d.h"
#include "Provider.d.h"
#include "TemporalError.d.h"
#include "TimeZone.d.h"

#include "PlainYearMonth.d.h"






typedef struct temporal_rs_PlainYearMonth_try_new_with_overflow_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_try_new_with_overflow_result;
temporal_rs_PlainYearMonth_try_new_with_overflow_result temporal_rs_PlainYearMonth_try_new_with_overflow(int32_t year, uint8_t month, OptionU8 reference_day, AnyCalendarKind calendar, ArithmeticOverflow overflow);

typedef struct temporal_rs_PlainYearMonth_from_partial_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_partial_result;
temporal_rs_PlainYearMonth_from_partial_result temporal_rs_PlainYearMonth_from_partial(PartialDate partial, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainYearMonth_from_parsed_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_parsed_result;
temporal_rs_PlainYearMonth_from_parsed_result temporal_rs_PlainYearMonth_from_parsed(const ParsedDate* parsed);

typedef struct temporal_rs_PlainYearMonth_with_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_with_result;
temporal_rs_PlainYearMonth_with_result temporal_rs_PlainYearMonth_with(const PlainYearMonth* self, PartialDate partial, ArithmeticOverflow_option overflow);

typedef struct temporal_rs_PlainYearMonth_from_utf8_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_utf8_result;
temporal_rs_PlainYearMonth_from_utf8_result temporal_rs_PlainYearMonth_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_PlainYearMonth_from_utf16_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_from_utf16_result;
temporal_rs_PlainYearMonth_from_utf16_result temporal_rs_PlainYearMonth_from_utf16(DiplomatString16View s);

int32_t temporal_rs_PlainYearMonth_year(const PlainYearMonth* self);

uint8_t temporal_rs_PlainYearMonth_month(const PlainYearMonth* self);

void temporal_rs_PlainYearMonth_month_code(const PlainYearMonth* self, DiplomatWrite* write);

bool temporal_rs_PlainYearMonth_in_leap_year(const PlainYearMonth* self);

uint16_t temporal_rs_PlainYearMonth_days_in_month(const PlainYearMonth* self);

uint16_t temporal_rs_PlainYearMonth_days_in_year(const PlainYearMonth* self);

uint16_t temporal_rs_PlainYearMonth_months_in_year(const PlainYearMonth* self);

void temporal_rs_PlainYearMonth_era(const PlainYearMonth* self, DiplomatWrite* write);

typedef struct temporal_rs_PlainYearMonth_era_year_result {union {int32_t ok; }; bool is_ok;} temporal_rs_PlainYearMonth_era_year_result;
temporal_rs_PlainYearMonth_era_year_result temporal_rs_PlainYearMonth_era_year(const PlainYearMonth* self);

const Calendar* temporal_rs_PlainYearMonth_calendar(const PlainYearMonth* self);

typedef struct temporal_rs_PlainYearMonth_add_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_add_result;
temporal_rs_PlainYearMonth_add_result temporal_rs_PlainYearMonth_add(const PlainYearMonth* self, const Duration* duration, ArithmeticOverflow overflow);

typedef struct temporal_rs_PlainYearMonth_subtract_result {union {PlainYearMonth* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_subtract_result;
temporal_rs_PlainYearMonth_subtract_result temporal_rs_PlainYearMonth_subtract(const PlainYearMonth* self, const Duration* duration, ArithmeticOverflow overflow);

typedef struct temporal_rs_PlainYearMonth_until_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_until_result;
temporal_rs_PlainYearMonth_until_result temporal_rs_PlainYearMonth_until(const PlainYearMonth* self, const PlainYearMonth* other, DifferenceSettings settings);

typedef struct temporal_rs_PlainYearMonth_since_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_since_result;
temporal_rs_PlainYearMonth_since_result temporal_rs_PlainYearMonth_since(const PlainYearMonth* self, const PlainYearMonth* other, DifferenceSettings settings);

bool temporal_rs_PlainYearMonth_equals(const PlainYearMonth* self, const PlainYearMonth* other);

int8_t temporal_rs_PlainYearMonth_compare(const PlainYearMonth* one, const PlainYearMonth* two);

typedef struct temporal_rs_PlainYearMonth_to_plain_date_result {union {PlainDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_to_plain_date_result;
temporal_rs_PlainYearMonth_to_plain_date_result temporal_rs_PlainYearMonth_to_plain_date(const PlainYearMonth* self, PartialDate_option day);

typedef struct temporal_rs_PlainYearMonth_epoch_ms_for_result {union {int64_t ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_epoch_ms_for_result;
temporal_rs_PlainYearMonth_epoch_ms_for_result temporal_rs_PlainYearMonth_epoch_ms_for(const PlainYearMonth* self, TimeZone time_zone);

typedef struct temporal_rs_PlainYearMonth_epoch_ms_for_with_provider_result {union {int64_t ok; TemporalError err;}; bool is_ok;} temporal_rs_PlainYearMonth_epoch_ms_for_with_provider_result;
temporal_rs_PlainYearMonth_epoch_ms_for_with_provider_result temporal_rs_PlainYearMonth_epoch_ms_for_with_provider(const PlainYearMonth* self, TimeZone time_zone, const Provider* p);

void temporal_rs_PlainYearMonth_to_ixdtf_string(const PlainYearMonth* self, DisplayCalendar display_calendar, DiplomatWrite* write);

PlainYearMonth* temporal_rs_PlainYearMonth_clone(const PlainYearMonth* self);

void temporal_rs_PlainYearMonth_destroy(PlainYearMonth* self);





#endif // PlainYearMonth_H
