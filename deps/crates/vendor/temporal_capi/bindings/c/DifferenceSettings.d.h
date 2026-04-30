#ifndef DifferenceSettings_D_H
#define DifferenceSettings_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "RoundingMode.d.h"
#include "Unit.d.h"




typedef struct DifferenceSettings {
  Unit_option largest_unit;
  Unit_option smallest_unit;
  RoundingMode_option rounding_mode;
  OptionU32 increment;
} DifferenceSettings;

typedef struct DifferenceSettings_option {union { DifferenceSettings ok; }; bool is_ok; } DifferenceSettings_option;



#endif // DifferenceSettings_D_H
