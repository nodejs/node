#ifndef Precision_D_H
#define Precision_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct Precision {
  bool is_minute;
  OptionU8 precision;
} Precision;

typedef struct Precision_option {union { Precision ok; }; bool is_ok; } Precision_option;



#endif // Precision_D_H
