#ifndef TimeZone_D_H
#define TimeZone_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct TimeZone {
  int16_t offset_minutes;
  size_t resolved_id;
  size_t normalized_id;
  bool is_iana_id;
} TimeZone;

typedef struct TimeZone_option {union { TimeZone ok; }; bool is_ok; } TimeZone_option;



#endif // TimeZone_D_H
