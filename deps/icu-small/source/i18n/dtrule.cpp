/*
*******************************************************************************
* Copyright (C) 2007-2012, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "utypeinfo.h"  // for 'typeid' to work

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dtrule.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DateTimeRule)

DateTimeRule::DateTimeRule(int32_t month,
                           int32_t dayOfMonth,
                           int32_t millisInDay,
                           TimeRuleType timeType)
: fMonth(month), fDayOfMonth(dayOfMonth), fDayOfWeek(0), fWeekInMonth(0), fMillisInDay(millisInDay),
  fDateRuleType(DateTimeRule::DOM), fTimeRuleType(timeType) {
}

DateTimeRule::DateTimeRule(int32_t month,
                           int32_t weekInMonth,
                           int32_t dayOfWeek,
                           int32_t millisInDay,
                           TimeRuleType timeType)
: fMonth(month), fDayOfMonth(0), fDayOfWeek(dayOfWeek), fWeekInMonth(weekInMonth), fMillisInDay(millisInDay),
  fDateRuleType(DateTimeRule::DOW), fTimeRuleType(timeType) {
}

DateTimeRule::DateTimeRule(int32_t month,
                           int32_t dayOfMonth,
                           int32_t dayOfWeek,
                           UBool after,
                           int32_t millisInDay,
                           TimeRuleType timeType)
: UObject(),
  fMonth(month), fDayOfMonth(dayOfMonth), fDayOfWeek(dayOfWeek), fWeekInMonth(0), fMillisInDay(millisInDay),
  fTimeRuleType(timeType) {
    if (after) {
        fDateRuleType = DateTimeRule::DOW_GEQ_DOM;
    } else {
        fDateRuleType = DateTimeRule::DOW_LEQ_DOM;
    }
}

DateTimeRule::DateTimeRule(const DateTimeRule& source)
: UObject(source),
  fMonth(source.fMonth), fDayOfMonth(source.fDayOfMonth), fDayOfWeek(source.fDayOfWeek),
  fWeekInMonth(source.fWeekInMonth), fMillisInDay(source.fMillisInDay),
  fDateRuleType(source.fDateRuleType), fTimeRuleType(source.fTimeRuleType) {
}

DateTimeRule::~DateTimeRule() {
}

DateTimeRule*
DateTimeRule::clone() const {
    return new DateTimeRule(*this);
}

DateTimeRule&
DateTimeRule::operator=(const DateTimeRule& right) {
    if (this != &right) {
        fMonth = right.fMonth;
        fDayOfMonth = right.fDayOfMonth;
        fDayOfWeek = right.fDayOfWeek;
        fWeekInMonth = right.fWeekInMonth;
        fMillisInDay = right.fMillisInDay;
        fDateRuleType = right.fDateRuleType;
        fTimeRuleType = right.fTimeRuleType;
    }
    return *this;
}

UBool
DateTimeRule::operator==(const DateTimeRule& that) const {
    return ((this == &that) ||
            (typeid(*this) == typeid(that) &&
            fMonth == that.fMonth &&
            fDayOfMonth == that.fDayOfMonth &&
            fDayOfWeek == that.fDayOfWeek &&
            fWeekInMonth == that.fWeekInMonth &&
            fMillisInDay == that.fMillisInDay &&
            fDateRuleType == that.fDateRuleType &&
            fTimeRuleType == that.fTimeRuleType));
}

UBool
DateTimeRule::operator!=(const DateTimeRule& that) const {
    return !operator==(that);
}

DateTimeRule::DateRuleType
DateTimeRule::getDateRuleType(void) const {
    return fDateRuleType;
}

DateTimeRule::TimeRuleType
DateTimeRule::getTimeRuleType(void) const {
    return fTimeRuleType;
}

int32_t
DateTimeRule::getRuleMonth(void) const {
    return fMonth;
}

int32_t
DateTimeRule::getRuleDayOfMonth(void) const {
    return fDayOfMonth;
}

int32_t
DateTimeRule::getRuleDayOfWeek(void) const {
    return fDayOfWeek;
}

int32_t
DateTimeRule::getRuleWeekInMonth(void) const {
    return fWeekInMonth;
}

int32_t
DateTimeRule::getRuleMillisInDay(void) const {
    return fMillisInDay;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
