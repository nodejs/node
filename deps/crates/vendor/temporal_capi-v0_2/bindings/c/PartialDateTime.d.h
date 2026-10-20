#ifndef PartialDateTime_D_H
#define PartialDateTime_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "PartialDate.d.h"
#include "PartialTime.d.h"




typedef struct PartialDateTime {
  PartialDate date;
  PartialTime time;
} PartialDateTime;

typedef struct PartialDateTime_option {union { PartialDateTime ok; }; bool is_ok; } PartialDateTime_option;



#endif // PartialDateTime_D_H
