#ifndef TemporalError_D_H
#define TemporalError_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "ErrorKind.d.h"




typedef struct TemporalError {
  ErrorKind kind;
  OptionStringView msg;
} TemporalError;

typedef struct TemporalError_option {union { TemporalError ok; }; bool is_ok; } TemporalError_option;



#endif // TemporalError_D_H
