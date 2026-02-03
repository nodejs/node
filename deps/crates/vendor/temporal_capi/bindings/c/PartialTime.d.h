#ifndef PartialTime_D_H
#define PartialTime_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct PartialTime {
  OptionU8 hour;
  OptionU8 minute;
  OptionU8 second;
  OptionU16 millisecond;
  OptionU16 microsecond;
  OptionU16 nanosecond;
} PartialTime;

typedef struct PartialTime_option {union { PartialTime ok; }; bool is_ok; } PartialTime_option;



#endif // PartialTime_D_H
