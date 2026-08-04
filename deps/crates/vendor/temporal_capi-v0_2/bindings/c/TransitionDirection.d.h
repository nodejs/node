#ifndef TransitionDirection_D_H
#define TransitionDirection_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum TransitionDirection {
  TransitionDirection_Next = 0,
  TransitionDirection_Previous = 1,
} TransitionDirection;

typedef struct TransitionDirection_option {union { TransitionDirection ok; }; bool is_ok; } TransitionDirection_option;



#endif // TransitionDirection_D_H
