#ifndef Disambiguation_D_H
#define Disambiguation_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum Disambiguation {
  Disambiguation_Compatible = 0,
  Disambiguation_Earlier = 1,
  Disambiguation_Later = 2,
  Disambiguation_Reject = 3,
} Disambiguation;

typedef struct Disambiguation_option {union { Disambiguation ok; }; bool is_ok; } Disambiguation_option;



#endif // Disambiguation_D_H
