#ifndef PartialDate_D_H
#define PartialDate_D_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "diplomat_runtime.h"

#include "AnyCalendarKind.d.h"




typedef struct PartialDate {
  OptionI32 year;
  OptionU8 month;
  DiplomatStringView month_code;
  OptionU8 day;
  DiplomatStringView era;
  OptionI32 era_year;
  AnyCalendarKind calendar;
} PartialDate;

typedef struct PartialDate_option {union { PartialDate ok; }; bool is_ok; } PartialDate_option;



#endif // PartialDate_D_H
