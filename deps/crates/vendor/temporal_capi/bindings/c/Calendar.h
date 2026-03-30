#ifndef Calendar_H
#define Calendar_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "AnyCalendarKind.d.h"
#include "TemporalError.d.h"

#include "Calendar.d.h"






Calendar* temporal_rs_Calendar_try_new_constrain(AnyCalendarKind kind);

typedef struct temporal_rs_Calendar_from_utf8_result {union {Calendar* ok; TemporalError err;}; bool is_ok;} temporal_rs_Calendar_from_utf8_result;
temporal_rs_Calendar_from_utf8_result temporal_rs_Calendar_from_utf8(DiplomatStringView s);

bool temporal_rs_Calendar_is_iso(const Calendar* self);

DiplomatStringView temporal_rs_Calendar_identifier(const Calendar* self);

AnyCalendarKind temporal_rs_Calendar_kind(const Calendar* self);

void temporal_rs_Calendar_destroy(Calendar* self);





#endif // Calendar_H
