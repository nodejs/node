#ifndef AnyCalendarKind_D_H
#define AnyCalendarKind_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum AnyCalendarKind {
  AnyCalendarKind_Iso = 0,
  AnyCalendarKind_Buddhist = 1,
  AnyCalendarKind_Chinese = 2,
  AnyCalendarKind_Coptic = 3,
  AnyCalendarKind_Dangi = 4,
  AnyCalendarKind_Ethiopian = 5,
  AnyCalendarKind_EthiopianAmeteAlem = 6,
  AnyCalendarKind_Gregorian = 7,
  AnyCalendarKind_Hebrew = 8,
  AnyCalendarKind_Indian = 9,
  AnyCalendarKind_HijriTabularTypeIIFriday = 10,
  AnyCalendarKind_HijriTabularTypeIIThursday = 11,
  AnyCalendarKind_HijriUmmAlQura = 12,
  AnyCalendarKind_Japanese = 13,
  AnyCalendarKind_JapaneseExtended = 14,
  AnyCalendarKind_Persian = 15,
  AnyCalendarKind_Roc = 16,
} AnyCalendarKind;

typedef struct AnyCalendarKind_option {union { AnyCalendarKind ok; }; bool is_ok; } AnyCalendarKind_option;



#endif // AnyCalendarKind_D_H
