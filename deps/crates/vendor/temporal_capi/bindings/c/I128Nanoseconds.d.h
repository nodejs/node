#ifndef I128Nanoseconds_D_H
#define I128Nanoseconds_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef struct I128Nanoseconds {
  uint64_t high;
  uint64_t low;
} I128Nanoseconds;

typedef struct I128Nanoseconds_option {union { I128Nanoseconds ok; }; bool is_ok; } I128Nanoseconds_option;



#endif // I128Nanoseconds_D_H
