#ifndef AnyCalendarKind_D_H
#define AnyCalendarKind_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"





typedef enum AnyCalendarKind {
  AnyCalendarKind_Buddhist = 0,
  AnyCalendarKind_Chinese = 1,
  AnyCalendarKind_Coptic = 2,
  AnyCalendarKind_Dangi = 3,
  AnyCalendarKind_Ethiopian = 4,
  AnyCalendarKind_EthiopianAmeteAlem = 5,
  AnyCalendarKind_Gregorian = 6,
  AnyCalendarKind_Hebrew = 7,
  AnyCalendarKind_Indian = 8,
  AnyCalendarKind_HijriTabularTypeIIFriday = 9,
  AnyCalendarKind_HijriSimulatedMecca = 10,
  AnyCalendarKind_HijriTabularTypeIIThursday = 11,
  AnyCalendarKind_HijriUmmAlQura = 12,
  AnyCalendarKind_Iso = 13,
  AnyCalendarKind_Japanese = 14,
  AnyCalendarKind_JapaneseExtended = 15,
  AnyCalendarKind_Persian = 16,
  AnyCalendarKind_Roc = 17,
} AnyCalendarKind;

typedef struct AnyCalendarKind_option {union { AnyCalendarKind ok; }; bool is_ok; } AnyCalendarKind_option;



#endif // AnyCalendarKind_D_H
