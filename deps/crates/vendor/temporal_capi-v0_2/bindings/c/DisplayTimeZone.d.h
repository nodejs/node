#ifndef DisplayTimeZone_D_H
#define DisplayTimeZone_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DisplayTimeZone {
  DisplayTimeZone_Auto = 0,
  DisplayTimeZone_Never = 1,
  DisplayTimeZone_Critical = 2,
} DisplayTimeZone;

typedef struct DisplayTimeZone_option {union { DisplayTimeZone ok; }; bool is_ok; } DisplayTimeZone_option;



#endif // DisplayTimeZone_D_H
