#ifndef Instant_H
#define Instant_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "DifferenceSettings.d.h"
#include "Duration.d.h"
#include "I128Nanoseconds.d.h"
#include "Provider.d.h"
#include "RoundingOptions.d.h"
#include "TemporalError.d.h"
#include "TimeZone.d.h"
#include "ToStringRoundingOptions.d.h"
#include "ZonedDateTime.d.h"

#include "Instant.d.h"






typedef struct temporal_rs_Instant_try_new_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_try_new_result;
temporal_rs_Instant_try_new_result temporal_rs_Instant_try_new(I128Nanoseconds ns);

typedef struct temporal_rs_Instant_from_epoch_milliseconds_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_epoch_milliseconds_result;
temporal_rs_Instant_from_epoch_milliseconds_result temporal_rs_Instant_from_epoch_milliseconds(int64_t epoch_milliseconds);

typedef struct temporal_rs_Instant_from_utf8_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_utf8_result;
temporal_rs_Instant_from_utf8_result temporal_rs_Instant_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_Instant_from_utf16_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_from_utf16_result;
temporal_rs_Instant_from_utf16_result temporal_rs_Instant_from_utf16(DiplomatString16View s);

typedef struct temporal_rs_Instant_add_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_add_result;
temporal_rs_Instant_add_result temporal_rs_Instant_add(const Instant* self, const Duration* duration);

typedef struct temporal_rs_Instant_subtract_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_subtract_result;
temporal_rs_Instant_subtract_result temporal_rs_Instant_subtract(const Instant* self, const Duration* duration);

typedef struct temporal_rs_Instant_since_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_since_result;
temporal_rs_Instant_since_result temporal_rs_Instant_since(const Instant* self, const Instant* other, DifferenceSettings settings);

typedef struct temporal_rs_Instant_until_result {union {Duration* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_until_result;
temporal_rs_Instant_until_result temporal_rs_Instant_until(const Instant* self, const Instant* other, DifferenceSettings settings);

typedef struct temporal_rs_Instant_round_result {union {Instant* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_round_result;
temporal_rs_Instant_round_result temporal_rs_Instant_round(const Instant* self, RoundingOptions options);

int8_t temporal_rs_Instant_compare(const Instant* self, const Instant* other);

bool temporal_rs_Instant_equals(const Instant* self, const Instant* other);

int64_t temporal_rs_Instant_epoch_milliseconds(const Instant* self);

I128Nanoseconds temporal_rs_Instant_epoch_nanoseconds(const Instant* self);

typedef struct temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result {union { TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result;
temporal_rs_Instant_to_ixdtf_string_with_compiled_data_result temporal_rs_Instant_to_ixdtf_string_with_compiled_data(const Instant* self, TimeZone_option zone, ToStringRoundingOptions options, DiplomatWrite* write);

typedef struct temporal_rs_Instant_to_ixdtf_string_with_provider_result {union { TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_ixdtf_string_with_provider_result;
temporal_rs_Instant_to_ixdtf_string_with_provider_result temporal_rs_Instant_to_ixdtf_string_with_provider(const Instant* self, TimeZone_option zone, ToStringRoundingOptions options, const Provider* p, DiplomatWrite* write);

typedef struct temporal_rs_Instant_to_zoned_date_time_iso_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_zoned_date_time_iso_result;
temporal_rs_Instant_to_zoned_date_time_iso_result temporal_rs_Instant_to_zoned_date_time_iso(const Instant* self, TimeZone zone);

typedef struct temporal_rs_Instant_to_zoned_date_time_iso_with_provider_result {union {ZonedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_Instant_to_zoned_date_time_iso_with_provider_result;
temporal_rs_Instant_to_zoned_date_time_iso_with_provider_result temporal_rs_Instant_to_zoned_date_time_iso_with_provider(const Instant* self, TimeZone zone, const Provider* p);

Instant* temporal_rs_Instant_clone(const Instant* self);

void temporal_rs_Instant_destroy(Instant* self);





#endif // Instant_H
