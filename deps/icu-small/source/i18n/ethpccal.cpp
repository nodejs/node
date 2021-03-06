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
#include "ethpccal.h"
#include "cecal.h"
#include <float.h>

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(EthiopicCalendar)

//static const int32_t JD_EPOCH_OFFSET_AMETE_ALEM = -285019;
static const int32_t JD_EPOCH_OFFSET_AMETE_MIHRET = 1723856;
static const int32_t AMETE_MIHRET_DELTA = 5500; // 5501 - 1 (Amete Alem 5501 = Amete Mihret 1)

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

EthiopicCalendar::EthiopicCalendar(const Locale& aLocale,
                                   UErrorCode& success,
                                   EEraType type /*= AMETE_MIHRET_ERA*/)
:   CECalendar(aLocale, success),
    eraType(type)
{
}

EthiopicCalendar::EthiopicCalendar(const EthiopicCalendar& other)
:   CECalendar(other),
    eraType(other.eraType)
{
}

EthiopicCalendar::~EthiopicCalendar()
{
}

EthiopicCalendar*
EthiopicCalendar::clone() const
{
    return new EthiopicCalendar(*this);
}

const char *
EthiopicCalendar::getType() const
{
    if (isAmeteAlemEra()) {
        return "ethiopic-amete-alem";
    }
    return "ethiopic";
}

void
EthiopicCalendar::setAmeteAlemEra(UBool onOff)
{
    eraType = onOff ? AMETE_ALEM_ERA : AMETE_MIHRET_ERA;
}

UBool
EthiopicCalendar::isAmeteAlemEra() const
{
    return (eraType == AMETE_ALEM_ERA);
}

//-------------------------------------------------------------------------
// Calendar framework
//-------------------------------------------------------------------------

int32_t
EthiopicCalendar::handleGetExtendedYear()
{
    // Ethiopic calendar uses EXTENDED_YEAR aligned to
    // Amelete Hihret year always.
    int32_t eyear;
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        eyear = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else if (isAmeteAlemEra()) {
        eyear = internalGet(UCAL_YEAR, 1 + AMETE_MIHRET_DELTA)
            - AMETE_MIHRET_DELTA; // Default to year 1 of Amelete Mihret
    } else {
        // The year defaults to the epoch start, the era to AMETE_MIHRET
        int32_t era = internalGet(UCAL_ERA, AMETE_MIHRET);
        if (era == AMETE_MIHRET) {
            eyear = internalGet(UCAL_YEAR, 1); // Default to year 1
        } else {
            eyear = internalGet(UCAL_YEAR, 1) - AMETE_MIHRET_DELTA;
        }
    }
    return eyear;
}

void
EthiopicCalendar::handleComputeFields(int32_t julianDay, UErrorCode &/*status*/)
{
    int32_t eyear, month, day, era, year;
    jdToCE(julianDay, getJDEpochOffset(), eyear, month, day);

    if (isAmeteAlemEra()) {
        era = AMETE_ALEM;
        year = eyear + AMETE_MIHRET_DELTA;
    } else {
        if (eyear > 0) {
            era = AMETE_MIHRET;
            year = eyear;
        } else {
            era = AMETE_ALEM;
            year = eyear + AMETE_MIHRET_DELTA;
        }
    }

    internalSet(UCAL_EXTENDED_YEAR, eyear);
    internalSet(UCAL_ERA, era);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_DATE, day);
    internalSet(UCAL_DAY_OF_YEAR, (30 * month) + day);
}

int32_t
EthiopicCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const
{
    if (isAmeteAlemEra() && field == UCAL_ERA) {
        return 0; // Only one era in this mode, era is always 0
    }
    return CECalendar::handleGetLimit(field, limitType);
}

/**
 * The system maintains a static default century start date and Year.  They are
 * initialized the first time they are used.  Once the system default century date
 * and year are set, they do not change.
 */
static UDate           gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t         gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce  gSystemDefaultCenturyInit        = U_INITONCE_INITIALIZER;

static void U_CALLCONV initializeSystemDefaultCentury()
{
    UErrorCode status = U_ZERO_ERROR;
    EthiopicCalendar calendar(Locale("@calendar=ethiopic"), status);
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
EthiopicCalendar::defaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t
EthiopicCalendar::defaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStartYear
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    if (isAmeteAlemEra()) {
        return gSystemDefaultCenturyStartYear + AMETE_MIHRET_DELTA;
    }
    return gSystemDefaultCenturyStartYear;
}


int32_t
EthiopicCalendar::getJDEpochOffset() const
{
    return JD_EPOCH_OFFSET_AMETE_MIHRET;
}


#if 0
// We do not want to introduce this API in ICU4C.
// It was accidentally introduced in ICU4J as a public API.

//-------------------------------------------------------------------------
// Calendar system Conversion methods...
//-------------------------------------------------------------------------

int32_t
EthiopicCalendar::ethiopicToJD(int32_t year, int32_t month, int32_t date)
{
    return ceToJD(year, month, date, JD_EPOCH_OFFSET_AMETE_MIHRET);
}
#endif

U_NAMESPACE_END

#endif
