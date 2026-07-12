#ifndef Duration_H
#define Duration_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "PartialDuration.d.h"
#include "Provider.d.h"
#include "RelativeTo.d.h"
#include "RoundingOptions.d.h"
#include "Sign.d.h"
#include "TemporalError.d.h"
#include "ToStringRoundingOptions.d.h"
#include "Unit.d.h"

#include "Duration.d.h"






typedef struct temporal_rs_Duration_create_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_create_result;
temporal_rs_Duration_create_result temporal_rs_Duration_create(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

typedef struct temporal_rs_Duration_try_new_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_try_new_result;
temporal_rs_Duration_try_new_result temporal_rs_Duration_try_new(int64_t years, int64_t months, int64_t weeks, int64_t days, int64_t hours, int64_t minutes, int64_t seconds, int64_t milliseconds, double microseconds, double nanoseconds);

typedef struct temporal_rs_Duration_from_partial_duration_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_partial_duration_result;
temporal_rs_Duration_from_partial_duration_result temporal_rs_Duration_from_partial_duration(PartialDuration partial);

typedef struct temporal_rs_Duration_from_utf8_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_utf8_result;
temporal_rs_Duration_from_utf8_result temporal_rs_Duration_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_Duration_from_utf16_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_from_utf16_result;
temporal_rs_Duration_from_utf16_result temporal_rs_Duration_from_utf16(DiplomatString16View s);

bool temporal_rs_Duration_is_time_within_range(const Duration* self);

int64_t temporal_rs_Duration_years(const Duration* self);

int64_t temporal_rs_Duration_months(const Duration* self);

int64_t temporal_rs_Duration_weeks(const Duration* self);

int64_t temporal_rs_Duration_days(const Duration* self);

int64_t temporal_rs_Duration_hours(const Duration* self);

int64_t temporal_rs_Duration_minutes(const Duration* self);

int64_t temporal_rs_Duration_seconds(const Duration* self);

int64_t temporal_rs_Duration_milliseconds(const Duration* self);

double temporal_rs_Duration_microseconds(const Duration* self);

double temporal_rs_Duration_nanoseconds(const Duration* self);

Sign temporal_rs_Duration_sign(const Duration* self);

bool temporal_rs_Duration_is_zero(const Duration* self);

Duration* temporal_rs_Duration_abs(const Duration* self);

Duration* temporal_rs_Duration_negated(const Duration* self);

typedef struct temporal_rs_Duration_add_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_add_result;
temporal_rs_Duration_add_result temporal_rs_Duration_add(const Duration* self, const Duration* other);

typedef struct temporal_rs_Duration_subtract_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_subtract_result;
temporal_rs_Duration_subtract_result temporal_rs_Duration_subtract(const Duration* self, const Duration* other);

typedef struct temporal_rs_Duration_to_string_result {union { TemporalError err;}; bool is_ok;} temporal_rs_Duration_to_string_result;
temporal_rs_Duration_to_string_result temporal_rs_Duration_to_string(const Duration* self, ToStringRoundingOptions options, DiplomatWrite* write);

typedef struct temporal_rs_Duration_round_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_round_result;
temporal_rs_Duration_round_result temporal_rs_Duration_round(const Duration* self, RoundingOptions options, RelativeTo relative_to);

typedef struct temporal_rs_Duration_round_with_provider_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_round_with_provider_result;
temporal_rs_Duration_round_with_provider_result temporal_rs_Duration_round_with_provider(const Duration* self, RoundingOptions options, RelativeTo relative_to, const Provider* p);

typedef struct temporal_rs_Duration_compare_result {union {int8_t ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_compare_result;
temporal_rs_Duration_compare_result temporal_rs_Duration_compare(const Duration* self, const Duration* other, RelativeTo relative_to);

typedef struct temporal_rs_Duration_compare_with_provider_result {union {int8_t ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_compare_with_provider_result;
temporal_rs_Duration_compare_with_provider_result temporal_rs_Duration_compare_with_provider(const Duration* self, const Duration* other, RelativeTo relative_to, const Provider* p);

typedef struct temporal_rs_Duration_total_result {union {double ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_total_result;
temporal_rs_Duration_total_result temporal_rs_Duration_total(const Duration* self, Unit unit, RelativeTo relative_to);

typedef struct temporal_rs_Duration_total_with_provider_result {union {double ok; TemporalError err;}; bool is_ok;} temporal_rs_Duration_total_with_provider_result;
temporal_rs_Duration_total_with_provider_result temporal_rs_Duration_total_with_provider(const Duration* self, Unit unit, RelativeTo relative_to, const Provider* p);

Duration* temporal_rs_Duration_clone(const Duration* self);

void temporal_rs_Duration_destroy(Duration* self);





#endif // Duration_H
