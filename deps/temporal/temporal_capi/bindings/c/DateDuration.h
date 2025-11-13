#ifndef DateDuration_H
#define DateDuration_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Sign.d.h"
#include "TemporalError.d.h"

#include "DateDuration.d.h"






typedef struct temporal_rs_DateDuration_try_new_result {union {DateDuration* ok; TemporalError err;}; bool is_ok;} temporal_rs_DateDuration_try_new_result;
temporal_rs_DateDuration_try_new_result temporal_rs_DateDuration_try_new(int64_t years, int64_t months, int64_t weeks, int64_t days);

DateDuration* temporal_rs_DateDuration_abs(const DateDuration* self);

DateDuration* temporal_rs_DateDuration_negated(const DateDuration* self);

Sign temporal_rs_DateDuration_sign(const DateDuration* self);

void temporal_rs_DateDuration_destroy(DateDuration* self);





#endif // DateDuration_H
