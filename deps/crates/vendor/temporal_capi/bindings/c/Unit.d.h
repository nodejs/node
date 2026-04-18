#ifndef Unit_D_H
#define Unit_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum Unit {
  Unit_Auto = 0,
  Unit_Nanosecond = 1,
  Unit_Microsecond = 2,
  Unit_Millisecond = 3,
  Unit_Second = 4,
  Unit_Minute = 5,
  Unit_Hour = 6,
  Unit_Day = 7,
  Unit_Week = 8,
  Unit_Month = 9,
  Unit_Year = 10,
} Unit;

typedef struct Unit_option {union { Unit ok; }; bool is_ok; } Unit_option;



#endif // Unit_D_H
