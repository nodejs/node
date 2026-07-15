#include "ArithmeticOverflow.d.h"
#include "Calendar.h"
#include "DisplayCalendar.d.h"
#include "PlainDate.h"
#include "diplomat_runtime.h"
#include <stdio.h>

int main() {
    temporal_rs_PlainDate_try_new_with_overflow_result result =
        temporal_rs_PlainDate_try_new_with_overflow(2025, 1, 33, AnyCalendarKind_Gregorian, ArithmeticOverflow_Constrain);

    if (!result.is_ok) {
        fprintf(stderr, "failed to create a PlainDate\n");
        return 1;
    }

    PlainDate *date = result.ok;
    char formatted[40];
    DiplomatWrite write = diplomat_simple_write(formatted, 40);

    temporal_rs_PlainDate_to_ixdtf_string(date, DisplayCalendar_Always, &write);
    if (write.grow_failed) {
        fprintf(stderr, "format overflowed the string\n");
        temporal_rs_PlainDate_destroy(date);
        return 1;
    }

    printf("%s\n", formatted);

    temporal_rs_PlainDate_destroy(date);

    return 0;
}
