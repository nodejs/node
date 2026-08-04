#ifndef OwnedRelativeTo_H
#define OwnedRelativeTo_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Provider.d.h"
#include "TemporalError.d.h"

#include "OwnedRelativeTo.d.h"






typedef struct temporal_rs_OwnedRelativeTo_from_utf8_result {union {OwnedRelativeTo ok; TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf8_result;
temporal_rs_OwnedRelativeTo_from_utf8_result temporal_rs_OwnedRelativeTo_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_OwnedRelativeTo_from_utf8_with_provider_result {union {OwnedRelativeTo ok; TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf8_with_provider_result;
temporal_rs_OwnedRelativeTo_from_utf8_with_provider_result temporal_rs_OwnedRelativeTo_from_utf8_with_provider(DiplomatStringView s, const Provider* p);

typedef struct temporal_rs_OwnedRelativeTo_from_utf16_result {union {OwnedRelativeTo ok; TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf16_result;
temporal_rs_OwnedRelativeTo_from_utf16_result temporal_rs_OwnedRelativeTo_from_utf16(DiplomatString16View s);

typedef struct temporal_rs_OwnedRelativeTo_from_utf16_with_provider_result {union {OwnedRelativeTo ok; TemporalError err;}; bool is_ok;} temporal_rs_OwnedRelativeTo_from_utf16_with_provider_result;
temporal_rs_OwnedRelativeTo_from_utf16_with_provider_result temporal_rs_OwnedRelativeTo_from_utf16_with_provider(DiplomatString16View s, const Provider* p);

OwnedRelativeTo temporal_rs_OwnedRelativeTo_empty(void);





#endif // OwnedRelativeTo_H
