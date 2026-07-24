#ifndef OwnedRelativeTo_D_H
#define OwnedRelativeTo_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "PlainDate.d.h"
#include "ZonedDateTime.d.h"




typedef struct OwnedRelativeTo {
  PlainDate* date;
  ZonedDateTime* zoned;
} OwnedRelativeTo;

typedef struct OwnedRelativeTo_option {union { OwnedRelativeTo ok; }; bool is_ok; } OwnedRelativeTo_option;



#endif // OwnedRelativeTo_D_H
