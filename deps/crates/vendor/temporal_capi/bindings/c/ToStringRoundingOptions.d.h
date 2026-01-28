#ifndef ToStringRoundingOptions_D_H
#define ToStringRoundingOptions_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "Precision.d.h"
#include "RoundingMode.d.h"
#include "Unit.d.h"




typedef struct ToStringRoundingOptions {
  Precision precision;
  Unit_option smallest_unit;
  RoundingMode_option rounding_mode;
} ToStringRoundingOptions;

typedef struct ToStringRoundingOptions_option {union { ToStringRoundingOptions ok; }; bool is_ok; } ToStringRoundingOptions_option;



#endif // ToStringRoundingOptions_D_H
