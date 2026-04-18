#ifndef RoundingOptions_D_H
#define RoundingOptions_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "RoundingMode.d.h"
#include "Unit.d.h"




typedef struct RoundingOptions {
  Unit_option largest_unit;
  Unit_option smallest_unit;
  RoundingMode_option rounding_mode;
  OptionU32 increment;
} RoundingOptions;

typedef struct RoundingOptions_option {union { RoundingOptions ok; }; bool is_ok; } RoundingOptions_option;



#endif // RoundingOptions_D_H
