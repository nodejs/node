#ifndef ParsedDateTime_H
#define ParsedDateTime_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "TemporalError.d.h"

#include "ParsedDateTime.d.h"






typedef struct temporal_rs_ParsedDateTime_from_utf8_result {union {ParsedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDateTime_from_utf8_result;
temporal_rs_ParsedDateTime_from_utf8_result temporal_rs_ParsedDateTime_from_utf8(DiplomatStringView s);

typedef struct temporal_rs_ParsedDateTime_from_utf16_result {union {ParsedDateTime* ok; TemporalError err;}; bool is_ok;} temporal_rs_ParsedDateTime_from_utf16_result;
temporal_rs_ParsedDateTime_from_utf16_result temporal_rs_ParsedDateTime_from_utf16(DiplomatString16View s);

void temporal_rs_ParsedDateTime_destroy(ParsedDateTime* self);





#endif // ParsedDateTime_H
