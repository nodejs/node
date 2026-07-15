#ifndef PartialDuration_D_H
#define PartialDuration_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct PartialDuration {
  OptionI64 years;
  OptionI64 months;
  OptionI64 weeks;
  OptionI64 days;
  OptionI64 hours;
  OptionI64 minutes;
  OptionI64 seconds;
  OptionI64 milliseconds;
  OptionF64 microseconds;
  OptionF64 nanoseconds;
} PartialDuration;

typedef struct PartialDuration_option {union { PartialDuration ok; }; bool is_ok; } PartialDuration_option;



#endif // PartialDuration_D_H
