// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2012, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/tzrule.h"
#include "unicode/ucal.h"
#include "gregoimp.h"
#include "cmemory.h"
#include "uarrsort.h"

U_CDECL_BEGIN
// UComparator function for sorting start times
static int32_t U_CALLCONV
compareDates(const void * /*context*/, const void *left, const void *right) {
    UDate l = *((UDate*)left);
    UDate r = *((UDate*)right);
    int32_t res = l < r ? -1 : (l == r ? 0 : 1);
    return res;
}
U_CDECL_END

U_NAMESPACE_BEGIN

TimeZoneRule::TimeZoneRule(const UnicodeString& name, int32_t rawOffset, int32_t dstSavings)
: UObject(), fName(name), fRawOffset(rawOffset), fDSTSavings(dstSavings) {
}

TimeZoneRule::TimeZoneRule(const TimeZoneRule& source)
: UObject(source), fName(source.fName), fRawOffset(source.fRawOffset), fDSTSavings(source.fDSTSavings) {
}

TimeZoneRule::~TimeZoneRule() {
}

TimeZoneRule&
TimeZoneRule::operator=(const TimeZoneRule& right) {
    if (this != &right) {
        fName = right.fName;
        fRawOffset = right.fRawOffset;
        fDSTSavings = right.fDSTSavings;
    }
    return *this;
}

UBool
TimeZoneRule::operator==(const TimeZoneRule& that) const {
    return ((this == &that) ||
            (typeid(*this) == typeid(that) &&
            fName == that.fName &&
            fRawOffset == that.fRawOffset &&
            fDSTSavings == that.fDSTSavings));
}

UBool
TimeZoneRule::operator!=(const TimeZoneRule& that) const {
    return !operator==(that);
}

UnicodeString&
TimeZoneRule::getName(UnicodeString& name) const {
    name = fName;
    return name;
}

int32_t
TimeZoneRule::getRawOffset(void) const {
    return fRawOffset;
}

int32_t
TimeZoneRule::getDSTSavings(void) const {
    return fDSTSavings;
}

UBool
TimeZoneRule::isEquivalentTo(const TimeZoneRule& other) const {
    return ((this == &other) ||
            (typeid(*this) == typeid(other) &&
            fRawOffset == other.fRawOffset &&
            fDSTSavings == other.fDSTSavings));
}


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(InitialTimeZoneRule)

InitialTimeZoneRule::InitialTimeZoneRule(const UnicodeString& name,
                                         int32_t rawOffset,
                                         int32_t dstSavings)
: TimeZoneRule(name, rawOffset, dstSavings) {
}

InitialTimeZoneRule::InitialTimeZoneRule(const InitialTimeZoneRule& source)
: TimeZoneRule(source) {
}

InitialTimeZoneRule::~InitialTimeZoneRule() {
}

InitialTimeZoneRule*
InitialTimeZoneRule::clone(void) const {
    return new InitialTimeZoneRule(*this);
}

InitialTimeZoneRule&
InitialTimeZoneRule::operator=(const InitialTimeZoneRule& right) {
    if (this != &right) {
        TimeZoneRule::operator=(right);
    }
    return *this;
}

UBool
InitialTimeZoneRule::operator==(const TimeZoneRule& that) const {
    return ((this == &that) ||
            (typeid(*this) == typeid(that) &&
            TimeZoneRule::operator==(that)));
}

UBool
InitialTimeZoneRule::operator!=(const TimeZoneRule& that) const {
    return !operator==(that);
}

UBool
InitialTimeZoneRule::isEquivalentTo(const TimeZoneRule& other) const {
    if (this == &other) {
        return TRUE;
    }
    if (typeid(*this) != typeid(other) || TimeZoneRule::isEquivalentTo(other) == FALSE) {
        return FALSE;
    }
    return TRUE;
}

UBool
InitialTimeZoneRule::getFirstStart(int32_t /*prevRawOffset*/,
                                  int32_t /*prevDSTSavings*/,
                                  UDate& /*result*/) const {
    return FALSE;
}

UBool
InitialTimeZoneRule::getFinalStart(int32_t /*prevRawOffset*/,
                                  int32_t /*prevDSTSavings*/,
                                  UDate& /*result*/) const {
    return FALSE;
}

UBool
InitialTimeZoneRule::getNextStart(UDate /*base*/,
                                 int32_t /*prevRawOffset*/,
                                 int32_t /*prevDSTSavings*/,
                                 UBool /*inclusive*/,
                                 UDate& /*result*/) const {
    return FALSE;
}

UBool
InitialTimeZoneRule::getPreviousStart(UDate /*base*/,
                                     int32_t /*prevRawOffset*/,
                                     int32_t /*prevDSTSavings*/,
                                     UBool /*inclusive*/,
                                     UDate& /*result*/) const {
    return FALSE;
}


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(AnnualTimeZoneRule)

const int32_t AnnualTimeZoneRule::MAX_YEAR = 0x7FFFFFFF; /* max signed int32 */

AnnualTimeZoneRule::AnnualTimeZoneRule(const UnicodeString& name,
                                       int32_t rawOffset,
                                       int32_t dstSavings,
                                       const DateTimeRule& dateTimeRule,
                                       int32_t startYear,
                                       int32_t endYear)
: TimeZoneRule(name, rawOffset, dstSavings), fDateTimeRule(new DateTimeRule(dateTimeRule)),
  fStartYear(startYear), fEndYear(endYear) {
}

AnnualTimeZoneRule::AnnualTimeZoneRule(const UnicodeString& name,
                                       int32_t rawOffset,
                                       int32_t dstSavings,
                                       DateTimeRule* dateTimeRule,
                                       int32_t startYear,
                                       int32_t endYear)
: TimeZoneRule(name, rawOffset, dstSavings), fDateTimeRule(dateTimeRule),
  fStartYear(startYear), fEndYear(endYear) {
}

AnnualTimeZoneRule::AnnualTimeZoneRule(const AnnualTimeZoneRule& source)
: TimeZoneRule(source), fDateTimeRule(new DateTimeRule(*(source.fDateTimeRule))),
  fStartYear(source.fStartYear), fEndYear(source.fEndYear) {
}

AnnualTimeZoneRule::~AnnualTimeZoneRule() {
    delete fDateTimeRule;
}

AnnualTimeZoneRule*
AnnualTimeZoneRule::clone(void) const {
    return new AnnualTimeZoneRule(*this);
}

AnnualTimeZoneRule&
AnnualTimeZoneRule::operator=(const AnnualTimeZoneRule& right) {
    if (this != &right) {
        TimeZoneRule::operator=(right);
        delete fDateTimeRule;
        fDateTimeRule = right.fDateTimeRule->clone();
        fStartYear = right.fStartYear;
        fEndYear = right.fEndYear;
    }
    return *this;
}

UBool
AnnualTimeZoneRule::operator==(const TimeZoneRule& that) const {
    if (this == &that) {
        return TRUE;
    }
    if (typeid(*this) != typeid(that)) {
        return FALSE;
    }
    AnnualTimeZoneRule *atzr = (AnnualTimeZoneRule*)&that;
    return (*fDateTimeRule == *(atzr->fDateTimeRule) &&
            fStartYear == atzr->fStartYear &&
            fEndYear == atzr->fEndYear);
}

UBool
AnnualTimeZoneRule::operator!=(const TimeZoneRule& that) const {
    return !operator==(that);
}

const DateTimeRule*
AnnualTimeZoneRule::getRule() const {
    return fDateTimeRule;
}

int32_t
AnnualTimeZoneRule::getStartYear() const {
    return fStartYear;
}

int32_t
AnnualTimeZoneRule::getEndYear() const {
    return fEndYear;
}

UBool
AnnualTimeZoneRule::getStartInYear(int32_t year,
                                   int32_t prevRawOffset,
                                   int32_t prevDSTSavings,
                                   UDate &result) const {
    if (year < fStartYear || year > fEndYear) {
        return FALSE;
    }
    double ruleDay;
    DateTimeRule::DateRuleType type = fDateTimeRule->getDateRuleType();
    if (type == DateTimeRule::DOM) {
        ruleDay = Grego::fieldsToDay(year, fDateTimeRule->getRuleMonth(), fDateTimeRule->getRuleDayOfMonth());
    } else {
        UBool after = TRUE;
        if (type == DateTimeRule::DOW) {
            // Normalize DOW rule into DOW_GEQ_DOM or DOW_LEQ_DOM
            int32_t weeks = fDateTimeRule->getRuleWeekInMonth();
            if (weeks > 0) {
                ruleDay = Grego::fieldsToDay(year, fDateTimeRule->getRuleMonth(), 1);
                ruleDay += 7 * (weeks - 1);
            } else {
                after = FALSE;
                ruleDay = Grego::fieldsToDay(year, fDateTimeRule->getRuleMonth(),
                    Grego::monthLength(year, fDateTimeRule->getRuleMonth()));
                ruleDay += 7 * (weeks + 1);
           }
        } else {
            int32_t month = fDateTimeRule->getRuleMonth();
            int32_t dom = fDateTimeRule->getRuleDayOfMonth();
            if (type == DateTimeRule::DOW_LEQ_DOM) {
                after = FALSE;
                // Handle Feb <=29
                if (month == UCAL_FEBRUARY && dom == 29 && !Grego::isLeapYear(year)) {
                    dom--;
                }
            }
            ruleDay = Grego::fieldsToDay(year, month, dom);
        }
        int32_t dow = Grego::dayOfWeek(ruleDay);
        int32_t delta = fDateTimeRule->getRuleDayOfWeek() - dow;
        if (after) {
            delta = delta < 0 ? delta + 7 : delta;
        } else {
            delta = delta > 0 ? delta - 7 : delta;
        }
        ruleDay += delta;
    }

    result = ruleDay*U_MILLIS_PER_DAY + fDateTimeRule->getRuleMillisInDay();
    if (fDateTimeRule->getTimeRuleType() != DateTimeRule::UTC_TIME) {
        result -= prevRawOffset;
    }
    if (fDateTimeRule->getTimeRuleType() == DateTimeRule::WALL_TIME) {
        result -= prevDSTSavings;
    }
    return TRUE;
}

UBool
AnnualTimeZoneRule::isEquivalentTo(const TimeZoneRule& other) const {
    if (this == &other) {
        return TRUE;
    }
    if (typeid(*this) != typeid(other) || TimeZoneRule::isEquivalentTo(other) == FALSE) {
        return FALSE;
    }
    AnnualTimeZoneRule* that = (AnnualTimeZoneRule*)&other;
    return (*fDateTimeRule == *(that->fDateTimeRule) &&
            fStartYear == that->fStartYear &&
            fEndYear == that->fEndYear);
}

UBool
AnnualTimeZoneRule::getFirstStart(int32_t prevRawOffset,
                                  int32_t prevDSTSavings,
                                  UDate& result) const {
    return getStartInYear(fStartYear, prevRawOffset, prevDSTSavings, result);
}

UBool
AnnualTimeZoneRule::getFinalStart(int32_t prevRawOffset,
                                  int32_t prevDSTSavings,
                                  UDate& result) const {
    if (fEndYear == MAX_YEAR) {
        return FALSE;
    }
    return getStartInYear(fEndYear, prevRawOffset, prevDSTSavings, result);
}

UBool
AnnualTimeZoneRule::getNextStart(UDate base,
                                 int32_t prevRawOffset,
                                 int32_t prevDSTSavings,
                                 UBool inclusive,
                                 UDate& result) const {
    int32_t year, month, dom, dow, doy, mid;
    Grego::timeToFields(base, year, month, dom, dow, doy, mid);
    if (year < fStartYear) {
        return getFirstStart(prevRawOffset, prevDSTSavings, result);
    }
    UDate tmp;
    if (getStartInYear(year, prevRawOffset, prevDSTSavings, tmp)) {
        if (tmp < base || (!inclusive && (tmp == base))) {
            // Return the next one
            return getStartInYear(year + 1, prevRawOffset, prevDSTSavings, result);
        } else {
            result = tmp;
            return TRUE;
        }
    }
    return FALSE;
}

UBool
AnnualTimeZoneRule::getPreviousStart(UDate base,
                                     int32_t prevRawOffset,
                                     int32_t prevDSTSavings,
                                     UBool inclusive,
                                     UDate& result) const {
    int32_t year, month, dom, dow, doy, mid;
    Grego::timeToFields(base, year, month, dom, dow, doy, mid);
    if (year > fEndYear) {
        return getFinalStart(prevRawOffset, prevDSTSavings, result);
    }
    UDate tmp;
    if (getStartInYear(year, prevRawOffset, prevDSTSavings, tmp)) {
        if (tmp > base || (!inclusive && (tmp == base))) {
            // Return the previous one
            return getStartInYear(year - 1, prevRawOffset, prevDSTSavings, result);
        } else {
            result = tmp;
            return TRUE;
        }
    }
    return FALSE;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(TimeArrayTimeZoneRule)

TimeArrayTimeZoneRule::TimeArrayTimeZoneRule(const UnicodeString& name,
                                             int32_t rawOffset,
                                             int32_t dstSavings,
                                             const UDate* startTimes,
                                             int32_t numStartTimes,
                                             DateTimeRule::TimeRuleType timeRuleType)
: TimeZoneRule(name, rawOffset, dstSavings), fTimeRuleType(timeRuleType),
  fStartTimes(NULL) {
    UErrorCode status = U_ZERO_ERROR;
    initStartTimes(startTimes, numStartTimes, status);
    //TODO - status?
}


TimeArrayTimeZoneRule::TimeArrayTimeZoneRule(const TimeArrayTimeZoneRule& source)
: TimeZoneRule(source), fTimeRuleType(source.fTimeRuleType), fStartTimes(NULL) {
    UErrorCode status = U_ZERO_ERROR;
    initStartTimes(source.fStartTimes, source.fNumStartTimes, status);
    //TODO - status?
}


TimeArrayTimeZoneRule::~TimeArrayTimeZoneRule() {
    if (fStartTimes != NULL && fStartTimes != fLocalStartTimes) {
        uprv_free(fStartTimes);
    }
}

TimeArrayTimeZoneRule*
TimeArrayTimeZoneRule::clone(void) const {
    return new TimeArrayTimeZoneRule(*this);
}


TimeArrayTimeZoneRule&
TimeArrayTimeZoneRule::operator=(const TimeArrayTimeZoneRule& right) {
    if (this != &right) {
        TimeZoneRule::operator=(right);
        UErrorCode status = U_ZERO_ERROR;
        initStartTimes(right.fStartTimes, right.fNumStartTimes, status);
        //TODO - status?
        fTimeRuleType = right.fTimeRuleType;
    }
    return *this;
}

UBool
TimeArrayTimeZoneRule::operator==(const TimeZoneRule& that) const {
    if (this == &that) {
        return TRUE;
    }
    if (typeid(*this) != typeid(that) || TimeZoneRule::operator==(that) == FALSE) {
        return FALSE;
    }
    TimeArrayTimeZoneRule *tatzr = (TimeArrayTimeZoneRule*)&that;
    if (fTimeRuleType != tatzr->fTimeRuleType ||
        fNumStartTimes != tatzr->fNumStartTimes) {
        return FALSE;
    }
    // Compare start times
    UBool res = TRUE;
    for (int32_t i = 0; i < fNumStartTimes; i++) {
        if (fStartTimes[i] != tatzr->fStartTimes[i]) {
            res = FALSE;
            break;
        }
    }
    return res;
}

UBool
TimeArrayTimeZoneRule::operator!=(const TimeZoneRule& that) const {
    return !operator==(that);
}

DateTimeRule::TimeRuleType
TimeArrayTimeZoneRule::getTimeType(void) const {
    return fTimeRuleType;
}

UBool
TimeArrayTimeZoneRule::getStartTimeAt(int32_t index, UDate& result) const {
    if (index >= fNumStartTimes || index < 0) {
        return FALSE;
    }
    result = fStartTimes[index];
    return TRUE;
}

int32_t
TimeArrayTimeZoneRule::countStartTimes(void) const {
    return fNumStartTimes;
}

UBool
TimeArrayTimeZoneRule::isEquivalentTo(const TimeZoneRule& other) const {
    if (this == &other) {
        return TRUE;
    }
    if (typeid(*this) != typeid(other) || TimeZoneRule::isEquivalentTo(other) == FALSE) {
        return FALSE;
    }
    TimeArrayTimeZoneRule* that = (TimeArrayTimeZoneRule*)&other;
    if (fTimeRuleType != that->fTimeRuleType ||
        fNumStartTimes != that->fNumStartTimes) {
        return FALSE;
    }
    // Compare start times
    UBool res = TRUE;
    for (int32_t i = 0; i < fNumStartTimes; i++) {
        if (fStartTimes[i] != that->fStartTimes[i]) {
            res = FALSE;
            break;
        }
    }
    return res;
}

UBool
TimeArrayTimeZoneRule::getFirstStart(int32_t prevRawOffset,
                                             int32_t prevDSTSavings,
                                             UDate& result) const {
    if (fNumStartTimes <= 0 || fStartTimes == NULL) {
        return FALSE;
    }
    result = getUTC(fStartTimes[0], prevRawOffset, prevDSTSavings);
    return TRUE;
}

UBool
TimeArrayTimeZoneRule::getFinalStart(int32_t prevRawOffset,
                                     int32_t prevDSTSavings,
                                     UDate& result) const {
    if (fNumStartTimes <= 0 || fStartTimes == NULL) {
        return FALSE;
    }
    result = getUTC(fStartTimes[fNumStartTimes - 1], prevRawOffset, prevDSTSavings);
    return TRUE;
}

UBool
TimeArrayTimeZoneRule::getNextStart(UDate base,
                                    int32_t prevRawOffset,
                                    int32_t prevDSTSavings,
                                    UBool inclusive,
                                    UDate& result) const {
    int32_t i = fNumStartTimes - 1;
    for (; i >= 0; i--) {
        UDate time = getUTC(fStartTimes[i], prevRawOffset, prevDSTSavings);
        if (time < base || (!inclusive && time == base)) {
            break;
        }
        result = time;
    }
    if (i == fNumStartTimes - 1) {
        return FALSE;
    }
    return TRUE;
}

UBool
TimeArrayTimeZoneRule::getPreviousStart(UDate base,
                                        int32_t prevRawOffset,
                                        int32_t prevDSTSavings,
                                        UBool inclusive,
                                        UDate& result) const {
    int32_t i = fNumStartTimes - 1;
    for (; i >= 0; i--) {
        UDate time = getUTC(fStartTimes[i], prevRawOffset, prevDSTSavings);
        if (time < base || (inclusive && time == base)) {
            result = time;
            return TRUE;
        }
    }
    return FALSE;
}


// ---- private methods ------

UBool
TimeArrayTimeZoneRule::initStartTimes(const UDate source[], int32_t size, UErrorCode& status) {
    // Free old array
    if (fStartTimes != NULL && fStartTimes != fLocalStartTimes) {
        uprv_free(fStartTimes);
    }
    // Allocate new one if needed
    if (size > TIMEARRAY_STACK_BUFFER_SIZE) {
        fStartTimes = (UDate*)uprv_malloc(sizeof(UDate)*size);
        if (fStartTimes == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            fNumStartTimes = 0;
            return FALSE;
        }
    } else {
        fStartTimes = (UDate*)fLocalStartTimes;
    }
    uprv_memcpy(fStartTimes, source, sizeof(UDate)*size);
    fNumStartTimes = size;
    // Sort dates
    uprv_sortArray(fStartTimes, fNumStartTimes, (int32_t)sizeof(UDate), compareDates, NULL, TRUE, &status);
    if (U_FAILURE(status)) {
        if (fStartTimes != NULL && fStartTimes != fLocalStartTimes) {
            uprv_free(fStartTimes);
        }
        fNumStartTimes = 0;
        return FALSE;
    }
    return TRUE;
}

UDate
TimeArrayTimeZoneRule::getUTC(UDate time, int32_t raw, int32_t dst) const {
    if (fTimeRuleType != DateTimeRule::UTC_TIME) {
        time -= raw;
    }
    if (fTimeRuleType == DateTimeRule::WALL_TIME) {
        time -= dst;
    }
    return time;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
