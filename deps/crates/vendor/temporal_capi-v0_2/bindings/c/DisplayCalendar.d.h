#ifndef DisplayCalendar_D_H
#define DisplayCalendar_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DisplayCalendar {
  DisplayCalendar_Auto = 0,
  DisplayCalendar_Always = 1,
  DisplayCalendar_Never = 2,
  DisplayCalendar_Critical = 3,
} DisplayCalendar;

typedef struct DisplayCalendar_option {union { DisplayCalendar ok; }; bool is_ok; } DisplayCalendar_option;



#endif // DisplayCalendar_D_H
