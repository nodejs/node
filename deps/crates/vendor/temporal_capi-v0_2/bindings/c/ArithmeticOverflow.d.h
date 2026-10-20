#ifndef ArithmeticOverflow_D_H
#define ArithmeticOverflow_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum ArithmeticOverflow {
  ArithmeticOverflow_Constrain = 0,
  ArithmeticOverflow_Reject = 1,
} ArithmeticOverflow;

typedef struct ArithmeticOverflow_option {union { ArithmeticOverflow ok; }; bool is_ok; } ArithmeticOverflow_option;



#endif // ArithmeticOverflow_D_H
