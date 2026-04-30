#ifndef ErrorKind_D_H
#define ErrorKind_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum ErrorKind {
  ErrorKind_Generic = 0,
  ErrorKind_Type = 1,
  ErrorKind_Range = 2,
  ErrorKind_Syntax = 3,
  ErrorKind_Assert = 4,
} ErrorKind;

typedef struct ErrorKind_option {union { ErrorKind ok; }; bool is_ok; } ErrorKind_option;



#endif // ErrorKind_D_H
