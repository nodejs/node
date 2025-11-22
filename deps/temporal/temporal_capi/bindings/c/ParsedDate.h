#ifndef ParsedDate_H
#define ParsedDate_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "TemporalError.d.h"

#include "ParsedDate.d.h"






typedef struct temporal_rs_ParsedDate_from_utf8_result {union {ParsedDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_from_utf8_result;
temporal_rs_ParsedDate_from_utf8_result temporal_rs_ParsedDate_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_ParsedDate_from_utf16_result {union {ParsedDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_from_utf16_result;
temporal_rs_ParsedDate_from_utf16_result temporal_rs_ParsedDate_from_utf16(DiplomatString16View s);

typedef struct temporal_rs_ParsedDate_year_month_from_utf8_result {union {ParsedDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_year_month_from_utf8_result;
temporal_rs_ParsedDate_year_month_from_utf8_result temporal_rs_ParsedDate_year_month_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_ParsedDate_year_month_from_utf16_result {union {ParsedDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_year_month_from_utf16_result;
temporal_rs_ParsedDate_year_month_from_utf16_result temporal_rs_ParsedDate_year_month_from_utf16(DiplomatString16View s);

typedef struct temporal_rs_ParsedDate_month_day_from_utf8_result {union {ParsedDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_month_day_from_utf8_result;
temporal_rs_ParsedDate_month_day_from_utf8_result temporal_rs_ParsedDate_month_day_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_ParsedDate_month_day_from_utf16_result {union {ParsedDate* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDate_month_day_from_utf16_result;
temporal_rs_ParsedDate_month_day_from_utf16_result temporal_rs_ParsedDate_month_day_from_utf16(DiplomatString16View s);

void temporal_rs_ParsedDate_destroy(ParsedDate* self);





#endif // ParsedDate_H
