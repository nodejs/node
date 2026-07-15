#ifndef Sign_D_H
#define Sign_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum Sign {
  Sign_Positive = 1,
  Sign_Zero = 0,
  Sign_Negative = -1,
} Sign;

typedef struct Sign_option {union { Sign ok; }; bool is_ok; } Sign_option;



#endif // Sign_D_H
