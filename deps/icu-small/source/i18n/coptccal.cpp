// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2003 - 2013, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "umutex.h"
#include "coptccal.h"
#include "cecal.h"
#include <float.h>

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CopticCalendar)

static const int32_t COPTIC_JD_EPOCH_OFFSET  = 1824665;

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

CopticCalendar::CopticCalendar(const Locale& aLocale, UErrorCode& success)
: CECalendar(aLocale, success)
{
}

CopticCalendar::CopticCalendar (const CopticCalendar& other)
: CECalendar(other)
{
}

CopticCalendar::~CopticCalendar()
{
}

CopticCalendar*
CopticCalendar::clone() const
{
    return new CopticCalendar(*this);
}

const char*
CopticCalendar::getType() const
{
    return "coptic";
}

//-------------------------------------------------------------------------
// Calendar framework
//-------------------------------------------------------------------------

int32_t
CopticCalendar::handleGetExtendedYear()
{
    int32_t eyear;
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        eyear = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        // The year defaults to the epoch start, the era to CE
        int32_t era = internalGet(UCAL_ERA, CE);
        if (era == BCE) {
            eyear = 1 - internalGet(UCAL_YEAR, 1); // Convert to extended year
        } else {
            eyear = internalGet(UCAL_YEAR, 1); // Default to year 1
        }
    }
    return eyear;
}

void
CopticCalendar::handleComputeFields(int32_t julianDay, UErrorCode &/*status*/)
{
    int32_t eyear, month, day, era, year;
    jdToCE(julianDay, getJDEpochOffset(), eyear, month, day);

    if (eyear <= 0) {
        era = BCE;
        year = 1 - eyear;
    } else {
        era = CE;
        year = eyear;
    }

    internalSet(UCAL_EXTENDED_YEAR, eyear);
    internalSet(UCAL_ERA, era);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_DATE, day);
    internalSet(UCAL_DAY_OF_YEAR, (30 * month) + day);
}

/**
 * The system maintains a static default century start date and Year.  They are
 * initialized the first time they are used.  Once the system default century date
 * and year are set, they do not change.
 */
static UDate           gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t         gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce  gSystemDefaultCenturyInit        = U_INITONCE_INITIALIZER;


static void U_CALLCONV initializeSystemDefaultCentury() {
    UErrorCode status = U_ZERO_ERROR;
    CopticCalendar calendar(Locale("@calendar=coptic"), status);
    if (U_SUCCESS(status)) {
        calendar.setTime(Calendar::getNow(), status);
        calendar.add(UCAL_YEAR, -80, status);
        gSystemDefaultCenturyStart = calendar.getTime(status);
        gSystemDefaultCenturyStartYear = calendar.get(UCAL_YEAR, status);
    }
    // We have no recourse upon failure unless we want to propagate the failure
    // out.
}

UDate
CopticCalendar::defaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t
CopticCalendar::defaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStartYear;
}


int32_t
CopticCalendar::getJDEpochOffset() const
{
    return COPTIC_JD_EPOCH_OFFSET;
}


#if 0
// We do not want to introduce this API in ICU4C.
// It was accidentally introduced in ICU4J as a public API.

//-------------------------------------------------------------------------
// Calendar system Conversion methods...
//-------------------------------------------------------------------------

int32_t
CopticCalendar::copticToJD(int32_t year, int32_t month, int32_t day)
{
    return CECalendar::ceToJD(year, month, day, COPTIC_JD_EPOCH_OFFSET);
}
#endif

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
//eof
