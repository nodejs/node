// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 * Copyright (C) 2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ******************************************************************************
 *
 * File DANGICAL.CPP
 *****************************************************************************
 */

#include "chnsecal.h"
#include "dangical.h"

#if !UCONFIG_NO_FORMATTING

#include "gregoimp.h" // Math
#include "uassert.h"
#include "ucln_in.h"
#include "umutex.h"
#include "unicode/rbtz.h"
#include "unicode/tzrule.h"

// --- The cache --
static icu::TimeZone *gDangiCalendarZoneAstroCalc = NULL;
static icu::UInitOnce gDangiCalendarInitOnce = U_INITONCE_INITIALIZER;

/**
 * The start year of the Korean traditional calendar (Dan-gi) is the inaugural
 * year of Dan-gun (BC 2333).
 */
static const int32_t DANGI_EPOCH_YEAR = -2332; // Gregorian year

U_CDECL_BEGIN
static UBool calendar_dangi_cleanup(void) {
    if (gDangiCalendarZoneAstroCalc) {
        delete gDangiCalendarZoneAstroCalc;
        gDangiCalendarZoneAstroCalc = NULL;
    }
    gDangiCalendarInitOnce.reset();
    return TRUE;
}
U_CDECL_END

U_NAMESPACE_BEGIN

// Implementation of the DangiCalendar class

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

DangiCalendar::DangiCalendar(const Locale& aLocale, UErrorCode& success)
:   ChineseCalendar(aLocale, DANGI_EPOCH_YEAR, getDangiCalZoneAstroCalc(), success)
{
}

DangiCalendar::DangiCalendar (const DangiCalendar& other) 
: ChineseCalendar(other)
{
}

DangiCalendar::~DangiCalendar()
{
}

Calendar*
DangiCalendar::clone() const
{
    return new DangiCalendar(*this);
}

const char *DangiCalendar::getType() const { 
    return "dangi";
}

/**
 * The time zone used for performing astronomical computations for
 * Dangi calendar. In Korea various timezones have been used historically 
 * (cf. http://www.math.snu.ac.kr/~kye/others/lunar.html): 
 *  
 *            - 1908/04/01: GMT+8 
 * 1908/04/01 - 1911/12/31: GMT+8.5 
 * 1912/01/01 - 1954/03/20: GMT+9 
 * 1954/03/21 - 1961/08/09: GMT+8.5 
 * 1961/08/10 -           : GMT+9 
 *  
 * Note that, in 1908-1911, the government did not apply the timezone change 
 * but used GMT+8. In addition, 1954-1961's timezone change does not affect 
 * the lunar date calculation. Therefore, the following simpler rule works: 
 *   
 * -1911: GMT+8 
 * 1912-: GMT+9 
 *  
 * Unfortunately, our astronomer's approximation doesn't agree with the 
 * references (http://www.math.snu.ac.kr/~kye/others/lunar.html and 
 * http://astro.kasi.re.kr/Life/ConvertSolarLunarForm.aspx?MenuID=115) 
 * in 1897/7/30. So the following ad hoc fix is used here: 
 *  
 *     -1896: GMT+8 
 *      1897: GMT+7 
 * 1898-1911: GMT+8 
 * 1912-    : GMT+9 
 */
static void U_CALLCONV initDangiCalZoneAstroCalc(void) {
    U_ASSERT(gDangiCalendarZoneAstroCalc == NULL);
    const UDate millis1897[] = { (UDate)((1897 - 1970) * 365 * kOneDay) }; // some days of error is not a problem here
    const UDate millis1898[] = { (UDate)((1898 - 1970) * 365 * kOneDay) }; // some days of error is not a problem here
    const UDate millis1912[] = { (UDate)((1912 - 1970) * 365 * kOneDay) }; // this doesn't create an issue for 1911/12/20
    InitialTimeZoneRule* initialTimeZone = new InitialTimeZoneRule(UNICODE_STRING_SIMPLE("GMT+8"), 8*kOneHour, 0);
    TimeZoneRule* rule1897 = new TimeArrayTimeZoneRule(UNICODE_STRING_SIMPLE("Korean 1897"), 7*kOneHour, 0, millis1897, 1, DateTimeRule::STANDARD_TIME);
    TimeZoneRule* rule1898to1911 = new TimeArrayTimeZoneRule(UNICODE_STRING_SIMPLE("Korean 1898-1911"), 8*kOneHour, 0, millis1898, 1, DateTimeRule::STANDARD_TIME);
    TimeZoneRule* ruleFrom1912 = new TimeArrayTimeZoneRule(UNICODE_STRING_SIMPLE("Korean 1912-"), 9*kOneHour, 0, millis1912, 1, DateTimeRule::STANDARD_TIME);
    UErrorCode status = U_ZERO_ERROR;
    RuleBasedTimeZone* dangiCalZoneAstroCalc = new RuleBasedTimeZone(UNICODE_STRING_SIMPLE("KOREA_ZONE"), initialTimeZone); // adopts initialTimeZone
    dangiCalZoneAstroCalc->addTransitionRule(rule1897, status); // adopts rule1897
    dangiCalZoneAstroCalc->addTransitionRule(rule1898to1911, status);
    dangiCalZoneAstroCalc->addTransitionRule(ruleFrom1912, status);
    dangiCalZoneAstroCalc->complete(status);
    if (U_SUCCESS(status)) {
        gDangiCalendarZoneAstroCalc = dangiCalZoneAstroCalc;
    } else {
        delete dangiCalZoneAstroCalc;
        gDangiCalendarZoneAstroCalc = NULL;
    }
    ucln_i18n_registerCleanup(UCLN_I18N_DANGI_CALENDAR, calendar_dangi_cleanup);
}

const TimeZone* DangiCalendar::getDangiCalZoneAstroCalc(void) const {
    umtx_initOnce(gDangiCalendarInitOnce, &initDangiCalZoneAstroCalc);
    return gDangiCalendarZoneAstroCalc;
}


UOBJECT_DEFINE_RTTI_IMPLEMENTATION(DangiCalendar)

U_NAMESPACE_END

#endif

