#ifndef UnsignedRoundingMode_D_H
#define UnsignedRoundingMode_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum UnsignedRoundingMode {
  UnsignedRoundingMode_Infinity = 0,
  UnsignedRoundingMode_Zero = 1,
  UnsignedRoundingMode_HalfInfinity = 2,
  UnsignedRoundingMode_HalfZero = 3,
  UnsignedRoundingMode_HalfEven = 4,
} UnsignedRoundingMode;

typedef struct UnsignedRoundingMode_option {union { UnsignedRoundingMode ok; }; bool is_ok; } UnsignedRoundingMode_option;



#endif // UnsignedRoundingMode_D_H
