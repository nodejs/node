// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2003-2009,2012,2016 International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File JAPANCAL.CPP
*
* Modification History:
*  05/16/2003    srl     copied from buddhcal.cpp
*
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#if U_PLATFORM_HAS_WINUWP_API == 0
#include <stdlib.h> // getenv() is not available in UWP env
#else
#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#include <windows.h>
#endif
#include "cmemory.h"
#include "erarules.h"
#include "japancal.h"
#include "unicode/gregocal.h"
#include "umutex.h"
#include "uassert.h"
#include "ucln_in.h"
#include "cstring.h"

static icu::EraRules * gJapaneseEraRules = nullptr;
static icu::UInitOnce gJapaneseEraRulesInitOnce {};
static int32_t gCurrentEra = 0;

U_CDECL_BEGIN
static UBool japanese_calendar_cleanup() {
    if (gJapaneseEraRules) {
        delete gJapaneseEraRules;
        gJapaneseEraRules = nullptr;
    }
    gCurrentEra = 0;
    gJapaneseEraRulesInitOnce.reset();
    return true;
}
U_CDECL_END

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(JapaneseCalendar)

static const int32_t kGregorianEpoch = 1970;    // used as the default value of EXTENDED_YEAR
static const char* TENTATIVE_ERA_VAR_NAME = "ICU_ENABLE_TENTATIVE_ERA";


// Export the following for use by test code.
UBool JapaneseCalendar::enableTentativeEra() {
    // Although start date of next Japanese era is planned ahead, a name of
    // new era might not be available. This implementation allows tester to
    // check a new era without era names by settings below (in priority order).
    // By default, such tentative era is disabled.

    // 1. Environment variable ICU_ENABLE_TENTATIVE_ERA=true or false

    UBool includeTentativeEra = false;

#if U_PLATFORM_HAS_WINUWP_API == 1
    // UWP doesn't allow access to getenv(), but we can call GetEnvironmentVariableW to do the same thing.
    char16_t varName[26] = {};
    u_charsToUChars(TENTATIVE_ERA_VAR_NAME, varName, static_cast<int32_t>(uprv_strlen(TENTATIVE_ERA_VAR_NAME)));
    WCHAR varValue[5] = {};
    DWORD ret = GetEnvironmentVariableW(reinterpret_cast<WCHAR*>(varName), varValue, UPRV_LENGTHOF(varValue));
    if ((ret == 4) && (_wcsicmp(varValue, L"true") == 0)) {
        includeTentativeEra = true;
    }
#else
    char *envVarVal = getenv(TENTATIVE_ERA_VAR_NAME);
    if (envVarVal != nullptr && uprv_stricmp(envVarVal, "true") == 0) {
        includeTentativeEra = true;
    }
#endif
    return includeTentativeEra;
}


// Initialize global Japanese era data
static void U_CALLCONV initializeEras(UErrorCode &status) {
    gJapaneseEraRules = EraRules::createInstance("japanese", JapaneseCalendar::enableTentativeEra(), status);
    if (U_FAILURE(status)) {
        return;
    }
    gCurrentEra = gJapaneseEraRules->getCurrentEraCode();
}

static void init(UErrorCode &status) {
    umtx_initOnce(gJapaneseEraRulesInitOnce, &initializeEras, status);
    ucln_i18n_registerCleanup(UCLN_I18N_JAPANESE_CALENDAR, japanese_calendar_cleanup);
}

/* Some platforms don't like to export constants, like old Palm OS and some z/OS configurations. */
uint32_t JapaneseCalendar::getCurrentEra() {
    return gCurrentEra;
}

JapaneseCalendar::JapaneseCalendar(const Locale& aLocale, UErrorCode& success)
:   GregorianCalendar(aLocale, success)
{
    init(success);
}

JapaneseCalendar::~JapaneseCalendar()
{
}

JapaneseCalendar::JapaneseCalendar(const JapaneseCalendar& source)
: GregorianCalendar(source)
{
    UErrorCode status = U_ZERO_ERROR;
    init(status);
    U_ASSERT(U_SUCCESS(status));
}

JapaneseCalendar* JapaneseCalendar::clone() const
{
    return new JapaneseCalendar(*this);
}

const char *JapaneseCalendar::getType() const
{
    return "japanese";
}

int32_t JapaneseCalendar::getDefaultMonthInYear(int32_t extendedYear, UErrorCode& status) 
{
    if (U_FAILURE(status)) {
      return 0;
    }
    int32_t era = internalGetEra();
    // TODO do we assume we can trust 'era'?  What if it is denormalized?

    if (era == 0) {
        // era 0 in Japanese calendar is Gregorian BC
        return GregorianCalendar::getDefaultMonthInYear(extendedYear, status);
    }

    // Find out if we are at the edge of an era
    int32_t eraStart[3] = {0, 0, 0};
    gJapaneseEraRules->getStartDate(era, eraStart, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    if (extendedYear == eraStart[0]) {
        return eraStart[1] - 1; // return 0-based month
    } else {
        return 0;
    }
}

int32_t JapaneseCalendar::getDefaultDayInMonth(int32_t extendedYear, int32_t month, UErrorCode& status) 
{
    if (U_FAILURE(status)) {
        return 0;
    }
    int32_t era = internalGetEra();

    if (era == 0) {
        // era 0 in Japanese calendar is Gregorian BC
        return GregorianCalendar::getDefaultDayInMonth(extendedYear, month, status);
    }

    int32_t eraStart[3] = {0, 0, 0};
    gJapaneseEraRules->getStartDate(era, eraStart, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    if (extendedYear == eraStart[0]) { // if it is year 1..
        if (month == (eraStart[1] - 1)) { // if it is the emperor's first month..
            return eraStart[2]; // return the D_O_M of accession
        }
    }
    return 1;
}


int32_t JapaneseCalendar::internalGetEra() const
{
    return internalGet(UCAL_ERA, gCurrentEra);
}

int32_t JapaneseCalendar::handleGetExtendedYear(UErrorCode& status)
{
    if (U_FAILURE(status)) {
        return 0;
    }

    // EXTENDED_YEAR in JapaneseCalendar is a Gregorian year
    // The default value of EXTENDED_YEAR is 1970 (Showa 45)
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR &&
        newerField(UCAL_EXTENDED_YEAR, UCAL_ERA) == UCAL_EXTENDED_YEAR) {
        return internalGet(UCAL_EXTENDED_YEAR, kGregorianEpoch);
    }

    // extended year is a gregorian year, where 1 = 1AD,  0 = 1BC, -1 = 2BC, etc
    int32_t era = internalGet(UCAL_ERA, gCurrentEra);
    int32_t year = internalGet(UCAL_YEAR, 1);
    if (era == 0) {
        // era 0 in Japanese calendar is Gregorian BC
        // year = 1 - year;
        if (uprv_mul32_overflow(year, -1, &year) || uprv_add32_overflow(year, 1, &year)) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
    } else {
        int32_t eraStartYear = gJapaneseEraRules->getStartYear(internalGet(UCAL_ERA, gCurrentEra), status);
        if (U_FAILURE(status)) {
            return 0;
        }
        // add gregorian starting year, subtract one because year starts at 1
        if (uprv_add32_overflow(year, eraStartYear - 1,  &year)) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
    }
    return year;
}


void JapaneseCalendar::handleComputeFields(int32_t julianDay, UErrorCode& status)
{
    if (U_FAILURE(status)) {
        return;
    }
    //Calendar::timeToFields(theTime, quick, status);
    GregorianCalendar::handleComputeFields(julianDay, status);
    int32_t extendedYear = internalGet(UCAL_EXTENDED_YEAR); // Gregorian year
    int32_t eraCode = gJapaneseEraRules->getEraCode(extendedYear, internalGetMonth(status) + 1, internalGet(UCAL_DAY_OF_MONTH), status);
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(eraCode >= 0); // getEraCode() returns -1 only when status is failure

    internalSet(UCAL_ERA, eraCode);
    int32_t year;
    if (eraCode == 0) {
        // Gregorian BC
        // year = 1 - extendedYear;
        if (uprv_mul32_overflow(extendedYear, -1, &year) || uprv_add32_overflow(year, 1, &year)) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return;
        }
    } else if (eraCode == 1) {
        // Gregorian AD
        year = extendedYear;
    } else {
        // Japanese era. extendedYear is at least 1868 (Meiji 1), so it should never
        // cause integer overflow here.
        year = extendedYear - gJapaneseEraRules->getStartYear(eraCode, status) + 1;
    }
    internalSet(YEAR, year);
}

/*
Disable pivoting 
*/
UBool JapaneseCalendar::haveDefaultCentury() const
{
    return false;
}

UDate JapaneseCalendar::defaultCenturyStart() const
{
    return 0;// WRONG
}

int32_t JapaneseCalendar::defaultCenturyStartYear() const
{
    return 0;
}

int32_t JapaneseCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const
{
    switch(field) {
    case UCAL_ERA:
        if (limitType == UCAL_LIMIT_MINIMUM || limitType == UCAL_LIMIT_GREATEST_MINIMUM) {
            return 0;
        }
        return gJapaneseEraRules->getMaxEraCode(); // max known era, not gCurrentEra
    case UCAL_YEAR:
        {
            switch (limitType) {
            case UCAL_LIMIT_MINIMUM:
            case UCAL_LIMIT_GREATEST_MINIMUM:
                return 1;
            case UCAL_LIMIT_LEAST_MAXIMUM:
                return 1;
            case  UCAL_LIMIT_COUNT: //added to avoid warning
            case UCAL_LIMIT_MAXIMUM:
            {
                UErrorCode status = U_ZERO_ERROR;
                int32_t eraStartYear = gJapaneseEraRules->getStartYear(gCurrentEra, status);
                U_ASSERT(U_SUCCESS(status));
                return GregorianCalendar::handleGetLimit(UCAL_YEAR, UCAL_LIMIT_MAXIMUM) - eraStartYear;
            }
            default:
                return 1;    // Error condition, invalid limitType
            }
        }
    default:
        return GregorianCalendar::handleGetLimit(field,limitType);
    }
}

int32_t JapaneseCalendar::getActualMaximum(UCalendarDateFields field, UErrorCode& status) const {
    if (field != UCAL_YEAR) {
        return GregorianCalendar::getActualMaximum(field, status);
    }
    int32_t era = get(UCAL_ERA, status);
    if (U_FAILURE(status)) {
        return 0; // error case... any value
    }
    if (era >= gJapaneseEraRules->getMaxEraCode()) { // max known era, not gCurrentEra
        // TODO: Investigate what value should be used here - revisit after 4.0.
        return handleGetLimit(UCAL_YEAR, UCAL_LIMIT_MAXIMUM);
    }
    if (era == 0) {
        // era 0 is Gregorian BC and has no era start year data.
        return GregorianCalendar::getActualMaximum(UCAL_YEAR, status);
    }

    // Use getNextEraCode() instead of +1, because there might be gaps between eras.
    int32_t nextEra = gJapaneseEraRules->getNextEraCode(era);
    U_ASSERT(nextEra != era);
    int32_t nextEraStart[3] = {0, 0, 0};
    gJapaneseEraRules->getStartDate(nextEra, nextEraStart, status);
    int32_t nextEraYear = nextEraStart[0];
    int32_t nextEraMonth = nextEraStart[1]; // 1-base
    int32_t nextEraDate = nextEraStart[2];

    int32_t eraStartYear = gJapaneseEraRules->getStartYear(era, status);
    if (U_FAILURE(status)) {
        return 0;
    }
    int32_t maxYear = nextEraYear - eraStartYear + 1;   // 1-base
    if (nextEraMonth == 1 && nextEraDate == 1) {
        // Subtract 1, because the next era starts at Jan 1
        maxYear--;
    }
    return maxYear;
}

U_NAMESPACE_END

#endif
