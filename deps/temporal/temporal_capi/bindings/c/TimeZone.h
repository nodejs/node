#ifndef TimeZone_H
#define TimeZone_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Provider.d.h"
#include "TemporalError.d.h"

#include "TimeZone.d.h"






typedef struct temporal_rs_TimeZone_try_from_identifier_str_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_identifier_str_result;
temporal_rs_TimeZone_try_from_identifier_str_result temporal_rs_TimeZone_try_from_identifier_str(DiplomatStringView ident);

typedef struct temporal_rs_TimeZone_try_from_identifier_str_with_provider_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_identifier_str_with_provider_result;
temporal_rs_TimeZone_try_from_identifier_str_with_provider_result temporal_rs_TimeZone_try_from_identifier_str_with_provider(DiplomatStringView ident, const Provider* p);

typedef struct temporal_rs_TimeZone_try_from_offset_str_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_offset_str_result;
temporal_rs_TimeZone_try_from_offset_str_result temporal_rs_TimeZone_try_from_offset_str(DiplomatStringView ident);

typedef struct temporal_rs_TimeZone_try_from_str_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_str_result;
temporal_rs_TimeZone_try_from_str_result temporal_rs_TimeZone_try_from_str(DiplomatStringView ident);

typedef struct temporal_rs_TimeZone_try_from_str_with_provider_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_try_from_str_with_provider_result;
temporal_rs_TimeZone_try_from_str_with_provider_result temporal_rs_TimeZone_try_from_str_with_provider(DiplomatStringView ident, const Provider* p);

void temporal_rs_TimeZone_identifier(TimeZone self, DiplomatWrite* write);

typedef struct temporal_rs_TimeZone_identifier_with_provider_result {union { TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_identifier_with_provider_result;
temporal_rs_TimeZone_identifier_with_provider_result temporal_rs_TimeZone_identifier_with_provider(TimeZone self, const Provider* p, DiplomatWrite* write);

TimeZone temporal_rs_TimeZone_utc(void);

typedef struct temporal_rs_TimeZone_utc_with_provider_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_utc_with_provider_result;
temporal_rs_TimeZone_utc_with_provider_result temporal_rs_TimeZone_utc_with_provider(const Provider* p);

TimeZone temporal_rs_TimeZone_zero(void);

typedef struct temporal_rs_TimeZone_primary_identifier_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_primary_identifier_result;
temporal_rs_TimeZone_primary_identifier_result temporal_rs_TimeZone_primary_identifier(TimeZone self);

typedef struct temporal_rs_TimeZone_primary_identifier_with_provider_result {union {TimeZone ok; TemporalError err;}; bool is_ok;} temporal_rs_TimeZone_primary_identifier_with_provider_result;
temporal_rs_TimeZone_primary_identifier_with_provider_result temporal_rs_TimeZone_primary_identifier_with_provider(TimeZone self, const Provider* p);





#endif // TimeZone_H
