#ifndef OffsetDisambiguation_D_H
#define OffsetDisambiguation_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum OffsetDisambiguation {
  OffsetDisambiguation_Use = 0,
  OffsetDisambiguation_Prefer = 1,
  OffsetDisambiguation_Ignore = 2,
  OffsetDisambiguation_Reject = 3,
} OffsetDisambiguation;

typedef struct OffsetDisambiguation_option {union { OffsetDisambiguation ok; }; bool is_ok; } OffsetDisambiguation_option;



#endif // OffsetDisambiguation_D_H
