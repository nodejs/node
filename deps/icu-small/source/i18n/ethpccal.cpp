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

#include "gregoimp.h"
#include "umutex.h"
#include "ethpccal.h"
#include "cecal.h"
#include <float.h>

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(EthiopicCalendar)
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(EthiopicAmeteAlemCalendar)

static const int32_t JD_EPOCH_OFFSET_AMETE_ALEM = -285019;
static const int32_t JD_EPOCH_OFFSET_AMETE_MIHRET = 1723856;
static const int32_t AMETE_MIHRET_DELTA = 5500; // 5501 - 1 (Amete Alem 5501 = Amete Mihret 1)

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

EthiopicCalendar::EthiopicCalendar(const Locale& aLocale,
                                   UErrorCode& success)
:   CECalendar(aLocale, success)
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
    return "ethiopic";
}

//-------------------------------------------------------------------------
// Calendar framework
//-------------------------------------------------------------------------

int32_t
EthiopicCalendar::handleGetExtendedYear(UErrorCode& status)
{
    if (U_FAILURE(status)) {
        return 0;
    }
    // Ethiopic calendar uses EXTENDED_YEAR aligned to
    // Amelete Hihret year always.
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        return internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    }
    // The year defaults to the epoch start, the era to AMETE_MIHRET
    if (internalGet(UCAL_ERA, AMETE_MIHRET) == AMETE_MIHRET) {
        return internalGet(UCAL_YEAR, 1); // Default to year 1
    }
    int32_t year = internalGet(UCAL_YEAR, 1);
    if (uprv_add32_overflow(year, -AMETE_MIHRET_DELTA, &year)) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    return year;
}

IMPL_SYSTEM_DEFAULT_CENTURY(EthiopicCalendar, "@calendar=ethiopic")

int32_t
EthiopicCalendar::getJDEpochOffset() const
{
    return JD_EPOCH_OFFSET_AMETE_MIHRET;
}

int32_t EthiopicCalendar::extendedYearToEra(int32_t extendedYear) const {
    return extendedYear <= 0 ? AMETE_ALEM : AMETE_MIHRET;
}

int32_t EthiopicCalendar::extendedYearToYear(int32_t extendedYear) const {
    return extendedYear <= 0 ? extendedYear + AMETE_MIHRET_DELTA : extendedYear;
}

int32_t EthiopicCalendar::getRelatedYearDifference() const {
    constexpr int32_t kEthiopicCalendarRelatedYearDifference = 8;
    return kEthiopicCalendarRelatedYearDifference;
}

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

EthiopicAmeteAlemCalendar::EthiopicAmeteAlemCalendar(const Locale& aLocale,
                                   UErrorCode& success)
:   EthiopicCalendar(aLocale, success)
{
}

EthiopicAmeteAlemCalendar::~EthiopicAmeteAlemCalendar()
{
}

EthiopicAmeteAlemCalendar*
EthiopicAmeteAlemCalendar::clone() const
{
    return new EthiopicAmeteAlemCalendar(*this);
}

//-------------------------------------------------------------------------
// Calendar framework
//-------------------------------------------------------------------------

const char *
EthiopicAmeteAlemCalendar::getType() const
{
    return "ethiopic-amete-alem";
}

int32_t
EthiopicAmeteAlemCalendar::handleGetExtendedYear(UErrorCode& status)
{
    if (U_FAILURE(status)) {
        return 0;
    }
    // Ethiopic calendar uses EXTENDED_YEAR aligned to
    // Amelete Hihret year always.
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        return internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    }
    // Default to year 1
    return internalGet(UCAL_YEAR, 1);
}

int32_t
EthiopicAmeteAlemCalendar::getJDEpochOffset() const
{
    return JD_EPOCH_OFFSET_AMETE_ALEM;
}

int32_t EthiopicAmeteAlemCalendar::extendedYearToEra(int32_t /* extendedYear */) const {
    return AMETE_ALEM;
}

int32_t EthiopicAmeteAlemCalendar::extendedYearToYear(int32_t extendedYear) const {
    return extendedYear;
}


int32_t
EthiopicAmeteAlemCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const
{
    if (field == UCAL_ERA) {
        return 0; // Only one era in this mode, era is always 0
    }
    return EthiopicCalendar::handleGetLimit(field, limitType);
}

U_NAMESPACE_END

#endif
