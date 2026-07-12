#ifndef RoundingMode_D_H
#define RoundingMode_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum RoundingMode {
  RoundingMode_Ceil = 0,
  RoundingMode_Floor = 1,
  RoundingMode_Expand = 2,
  RoundingMode_Trunc = 3,
  RoundingMode_HalfCeil = 4,
  RoundingMode_HalfFloor = 5,
  RoundingMode_HalfExpand = 6,
  RoundingMode_HalfTrunc = 7,
  RoundingMode_HalfEven = 8,
} RoundingMode;

typedef struct RoundingMode_option {union { RoundingMode ok; }; bool is_ok; } RoundingMode_option;



#endif // RoundingMode_D_H
