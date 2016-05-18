/*
*******************************************************************************
* Copyright (C) 2016, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* dayperiodrules.h
*
* created on: 2016-01-20
* created by: kazede
*/

#ifndef DAYPERIODRULES_H
#define DAYPERIODRULES_H

#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/uobject.h"
#include "unicode/utypes.h"
#include "resource.h"
#include "uhash.h"



U_NAMESPACE_BEGIN

struct DayPeriodRulesDataSink;

class DayPeriodRules : public UMemory {
    friend struct DayPeriodRulesDataSink;
public:
    enum DayPeriod {
        DAYPERIOD_UNKNOWN = -1,
        DAYPERIOD_MIDNIGHT,
        DAYPERIOD_NOON,
        DAYPERIOD_MORNING1,
        DAYPERIOD_AFTERNOON1,
        DAYPERIOD_EVENING1,
        DAYPERIOD_NIGHT1,
        DAYPERIOD_MORNING2,
        DAYPERIOD_AFTERNOON2,
        DAYPERIOD_EVENING2,
        DAYPERIOD_NIGHT2,
        DAYPERIOD_AM,
        DAYPERIOD_PM
    };

    static const DayPeriodRules *getInstance(const Locale &locale, UErrorCode &errorCode);

    UBool hasMidnight() const { return fHasMidnight; }
    UBool hasNoon() const { return fHasNoon; }
    DayPeriod getDayPeriodForHour(int32_t hour) const { return fDayPeriodForHour[hour]; }

    // Returns the center of dayPeriod. Half hours are indicated with a .5 .
    double getMidPointForDayPeriod(DayPeriod dayPeriod, UErrorCode &errorCode) const;

private:
    DayPeriodRules();

    // Translates "morning1" to DAYPERIOD_MORNING1, for example.
    static DayPeriod getDayPeriodFromString(const char *type_str);

    static void load(UErrorCode &errorCode);

    // Sets period type for all hours in [startHour, limitHour).
    void add(int32_t startHour, int32_t limitHour, DayPeriod period);

    // Returns TRUE if for all i, DayPeriodForHour[i] has a type other than UNKNOWN.
    // Values of HasNoon and HasMidnight do not affect the return value.
    UBool allHoursAreSet();

    // Returns the hour that starts dayPeriod. Returns 0 for MIDNIGHT and 12 for NOON.
    int32_t getStartHourForDayPeriod(DayPeriod dayPeriod, UErrorCode &errorCode) const;

    // Returns the hour that ends dayPeriod, i.e. that starts the next period.
    // E.g. if fDayPeriodForHour[13] thru [16] are AFTERNOON1, then this function returns 17 if
    // queried with AFTERNOON1.
    // Returns 0 for MIDNIGHT and 12 for NOON.
    int32_t getEndHourForDayPeriod(DayPeriod dayPeriod, UErrorCode &errorCode) const;

    UBool fHasMidnight;
    UBool fHasNoon;
    DayPeriod fDayPeriodForHour[24];
};

U_NAMESPACE_END

#endif /* DAYPERIODRULES_H */
