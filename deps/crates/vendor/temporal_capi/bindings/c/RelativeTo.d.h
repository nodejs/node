#ifndef RelativeTo_D_H
#define RelativeTo_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "PlainDate.d.h"
#include "ZonedDateTime.d.h"




typedef struct RelativeTo {
  const PlainDate* date;
  const ZonedDateTime* zoned;
} RelativeTo;

typedef struct RelativeTo_option {union { RelativeTo ok; }; bool is_ok; } RelativeTo_option;



#endif // RelativeTo_D_H
