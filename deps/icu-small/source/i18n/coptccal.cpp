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
CopticCalendar::handleGetExtendedYear(UErrorCode& status)
{
    if (U_FAILURE(status)) {
        return 0;
    }
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        return internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    }
    // The year defaults to the epoch start, the era to CE
    int32_t era = internalGet(UCAL_ERA, CE);
    if (era == BCE) {
        return 1 - internalGet(UCAL_YEAR, 1); // Convert to extended year
    }
    if (era == CE){
        return internalGet(UCAL_YEAR, 1); // Default to year 1
    }
    status = U_ILLEGAL_ARGUMENT_ERROR;
    return 0;
}

IMPL_SYSTEM_DEFAULT_CENTURY(CopticCalendar, "@calendar=coptic")

int32_t
CopticCalendar::getJDEpochOffset() const
{
    return COPTIC_JD_EPOCH_OFFSET;
}

int32_t CopticCalendar::extendedYearToEra(int32_t extendedYear) const {
    return extendedYear <= 0 ? BCE : CE;
}

int32_t CopticCalendar::extendedYearToYear(int32_t extendedYear) const {
    return extendedYear <= 0 ? 1 - extendedYear : extendedYear;
}

bool CopticCalendar::isEra0CountingBackward() const {
    return true;
}

int32_t
CopticCalendar::getRelatedYearDifference() const {
    constexpr int32_t kCopticCalendarRelatedYearDifference = 284;
    return kCopticCalendarRelatedYearDifference;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
//eof
