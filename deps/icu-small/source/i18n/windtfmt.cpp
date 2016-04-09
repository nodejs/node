/*
********************************************************************************
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINDTFMT.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if U_PLATFORM_HAS_WIN32_API

#if !UCONFIG_NO_FORMATTING

#include "unicode/ures.h"
#include "unicode/format.h"
#include "unicode/fmtable.h"
#include "unicode/datefmt.h"
#include "unicode/simpleformatter.h"
#include "unicode/calendar.h"
#include "unicode/gregocal.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "unicode/timezone.h"
#include "unicode/utmscale.h"

#include "cmemory.h"
#include "uresimp.h"
#include "windtfmt.h"
#include "wintzimpl.h"

#   define WIN32_LEAN_AND_MEAN
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#include <windows.h>

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(Win32DateFormat)

#define NEW_ARRAY(type,count) (type *) uprv_malloc((count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *) (array))

#define STACK_BUFFER_SIZE 64

UnicodeString* Win32DateFormat::getTimeDateFormat(const Calendar *cal, const Locale *locale, UErrorCode &status) const
{
    UnicodeString *result = NULL;
    const char *type = cal->getType();
    const char *base = locale->getBaseName();
    UResourceBundle *topBundle = ures_open((char *) 0, base, &status);
    UResourceBundle *calBundle = ures_getByKey(topBundle, "calendar", NULL, &status);
    UResourceBundle *typBundle = ures_getByKeyWithFallback(calBundle, type, NULL, &status);
    UResourceBundle *patBundle = ures_getByKeyWithFallback(typBundle, "DateTimePatterns", NULL, &status);

    if (status == U_MISSING_RESOURCE_ERROR) {
        status = U_ZERO_ERROR;
        typBundle = ures_getByKeyWithFallback(calBundle, "gregorian", typBundle, &status);
        patBundle = ures_getByKeyWithFallback(typBundle, "DateTimePatterns", patBundle, &status);
    }

    if (U_FAILURE(status)) {
        static const UChar defaultPattern[] = {0x007B, 0x0031, 0x007D, 0x0020, 0x007B, 0x0030, 0x007D, 0x0000}; // "{1} {0}"
        return new UnicodeString(defaultPattern, UPRV_LENGTHOF(defaultPattern));
    }

    int32_t resStrLen = 0;
    int32_t glueIndex = DateFormat::kDateTime;
    int32_t patSize = ures_getSize(patBundle);
    if (patSize >= (DateFormat::kDateTimeOffset + DateFormat::kShort + 1)) {
        // Get proper date time format
        glueIndex = (int32_t)(DateFormat::kDateTimeOffset + (fDateStyle - DateFormat::kDateOffset));
    }
    const UChar *resStr = ures_getStringByIndex(patBundle, glueIndex, &resStrLen, &status);

    result = new UnicodeString(TRUE, resStr, resStrLen);

    ures_close(patBundle);
    ures_close(typBundle);
    ures_close(calBundle);
    ures_close(topBundle);

    return result;
}

// TODO: Range-check timeStyle, dateStyle
Win32DateFormat::Win32DateFormat(DateFormat::EStyle timeStyle, DateFormat::EStyle dateStyle, const Locale &locale, UErrorCode &status)
  : DateFormat(), fDateTimeMsg(NULL), fTimeStyle(timeStyle), fDateStyle(dateStyle), fLocale(locale), fZoneID()
{
    if (U_SUCCESS(status)) {
        fLCID = locale.getLCID();
        fTZI = NEW_ARRAY(TIME_ZONE_INFORMATION, 1);
        uprv_memset(fTZI, 0, sizeof(TIME_ZONE_INFORMATION));
        adoptCalendar(Calendar::createInstance(locale, status));
    }
}

Win32DateFormat::Win32DateFormat(const Win32DateFormat &other)
  : DateFormat(other)
{
    *this = other;
}

Win32DateFormat::~Win32DateFormat()
{
//    delete fCalendar;
    uprv_free(fTZI);
    delete fDateTimeMsg;
}

Win32DateFormat &Win32DateFormat::operator=(const Win32DateFormat &other)
{
    // The following handles fCalendar
    DateFormat::operator=(other);

//    delete fCalendar;

    this->fDateTimeMsg = other.fDateTimeMsg == NULL ? NULL : new UnicodeString(*other.fDateTimeMsg);
    this->fTimeStyle   = other.fTimeStyle;
    this->fDateStyle   = other.fDateStyle;
    this->fLocale      = other.fLocale;
    this->fLCID        = other.fLCID;
//    this->fCalendar    = other.fCalendar->clone();
    this->fZoneID      = other.fZoneID;

    this->fTZI = NEW_ARRAY(TIME_ZONE_INFORMATION, 1);
    *this->fTZI = *other.fTZI;

    return *this;
}

Format *Win32DateFormat::clone(void) const
{
    return new Win32DateFormat(*this);
}

// TODO: Is just ignoring pos the right thing?
UnicodeString &Win32DateFormat::format(Calendar &cal, UnicodeString &appendTo, FieldPosition &pos) const
{
    FILETIME ft;
    SYSTEMTIME st_gmt;
    SYSTEMTIME st_local;
    TIME_ZONE_INFORMATION tzi = *fTZI;
    UErrorCode status = U_ZERO_ERROR;
    const TimeZone &tz = cal.getTimeZone();
    int64_t uct, uft;

    setTimeZoneInfo(&tzi, tz);

    uct = utmscale_fromInt64((int64_t) cal.getTime(status), UDTS_ICU4C_TIME, &status);
    uft = utmscale_toInt64(uct, UDTS_WINDOWS_FILE_TIME, &status);

    ft.dwLowDateTime =  (DWORD) (uft & 0xFFFFFFFF);
    ft.dwHighDateTime = (DWORD) ((uft >> 32) & 0xFFFFFFFF);

    FileTimeToSystemTime(&ft, &st_gmt);
    SystemTimeToTzSpecificLocalTime(&tzi, &st_gmt, &st_local);


    if (fDateStyle != DateFormat::kNone && fTimeStyle != DateFormat::kNone) {
        UnicodeString date;
        UnicodeString time;
        UnicodeString *pattern = fDateTimeMsg;

        formatDate(&st_local, date);
        formatTime(&st_local, time);

        if (strcmp(fCalendar->getType(), cal.getType()) != 0) {
            pattern = getTimeDateFormat(&cal, &fLocale, status);
        }

        SimpleFormatter(*pattern, 2, 2, status).format(time, date, appendTo, status);
    } else if (fDateStyle != DateFormat::kNone) {
        formatDate(&st_local, appendTo);
    } else if (fTimeStyle != DateFormat::kNone) {
        formatTime(&st_local, appendTo);
    }

    return appendTo;
}

void Win32DateFormat::parse(const UnicodeString& text, Calendar& cal, ParsePosition& pos) const
{
    pos.setErrorIndex(pos.getIndex());
}

void Win32DateFormat::adoptCalendar(Calendar *newCalendar)
{
    if (fCalendar == NULL || strcmp(fCalendar->getType(), newCalendar->getType()) != 0) {
        UErrorCode status = U_ZERO_ERROR;

        if (fDateStyle != DateFormat::kNone && fTimeStyle != DateFormat::kNone) {
            delete fDateTimeMsg;
            fDateTimeMsg = getTimeDateFormat(newCalendar, &fLocale, status);
        }
    }

    delete fCalendar;
    fCalendar = newCalendar;

    fZoneID = setTimeZoneInfo(fTZI, fCalendar->getTimeZone());
}

void Win32DateFormat::setCalendar(const Calendar &newCalendar)
{
    adoptCalendar(newCalendar.clone());
}

void Win32DateFormat::adoptTimeZone(TimeZone *zoneToAdopt)
{
    fZoneID = setTimeZoneInfo(fTZI, *zoneToAdopt);
    fCalendar->adoptTimeZone(zoneToAdopt);
}

void Win32DateFormat::setTimeZone(const TimeZone& zone)
{
    fZoneID = setTimeZoneInfo(fTZI, zone);
    fCalendar->setTimeZone(zone);
}

static const DWORD dfFlags[] = {DATE_LONGDATE, DATE_LONGDATE, DATE_SHORTDATE, DATE_SHORTDATE};

void Win32DateFormat::formatDate(const SYSTEMTIME *st, UnicodeString &appendTo) const
{
    int result;
    UChar stackBuffer[STACK_BUFFER_SIZE];
    UChar *buffer = stackBuffer;

    result = GetDateFormatW(fLCID, dfFlags[fDateStyle - kDateOffset], st, NULL, buffer, STACK_BUFFER_SIZE);

    if (result == 0) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            int newLength = GetDateFormatW(fLCID, dfFlags[fDateStyle - kDateOffset], st, NULL, NULL, 0);

            buffer = NEW_ARRAY(UChar, newLength);
            GetDateFormatW(fLCID, dfFlags[fDateStyle - kDateOffset], st, NULL, buffer, newLength);
        }
    }

    appendTo.append(buffer, (int32_t) wcslen(buffer));

    if (buffer != stackBuffer) {
        DELETE_ARRAY(buffer);
    }
}

static const DWORD tfFlags[] = {0, 0, 0, TIME_NOSECONDS};

void Win32DateFormat::formatTime(const SYSTEMTIME *st, UnicodeString &appendTo) const
{
    int result;
    UChar stackBuffer[STACK_BUFFER_SIZE];
    UChar *buffer = stackBuffer;

    result = GetTimeFormatW(fLCID, tfFlags[fTimeStyle], st, NULL, buffer, STACK_BUFFER_SIZE);

    if (result == 0) {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            int newLength = GetTimeFormatW(fLCID, tfFlags[fTimeStyle], st, NULL, NULL, 0);

            buffer = NEW_ARRAY(UChar, newLength);
            GetDateFormatW(fLCID, tfFlags[fTimeStyle], st, NULL, buffer, newLength);
        }
    }

    appendTo.append(buffer, (int32_t) wcslen(buffer));

    if (buffer != stackBuffer) {
        DELETE_ARRAY(buffer);
    }
}

UnicodeString Win32DateFormat::setTimeZoneInfo(TIME_ZONE_INFORMATION *tzi, const TimeZone &zone) const
{
    UnicodeString zoneID;

    zone.getID(zoneID);

    if (zoneID.compare(fZoneID) != 0) {
        UnicodeString icuid;

        zone.getID(icuid);
        if (! uprv_getWindowsTimeZoneInfo(tzi, icuid.getBuffer(), icuid.length())) {
            UBool found = FALSE;
            int32_t ec = TimeZone::countEquivalentIDs(icuid);

            for (int z = 0; z < ec; z += 1) {
                UnicodeString equiv = TimeZone::getEquivalentID(icuid, z);

                if (found = uprv_getWindowsTimeZoneInfo(tzi, equiv.getBuffer(), equiv.length())) {
                    break;
                }
            }

            if (! found) {
                GetTimeZoneInformation(tzi);
            }
        }
    }

    return zoneID;
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // U_PLATFORM_HAS_WIN32_API

