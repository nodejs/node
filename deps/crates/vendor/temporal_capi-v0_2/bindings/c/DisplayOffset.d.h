#ifndef DisplayOffset_D_H
#define DisplayOffset_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum DisplayOffset {
  DisplayOffset_Auto = 0,
  DisplayOffset_Never = 1,
} DisplayOffset;

typedef struct DisplayOffset_option {union { DisplayOffset ok; }; bool is_ok; } DisplayOffset_option;



#endif // DisplayOffset_D_H
