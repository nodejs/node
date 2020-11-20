// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2005-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINTZ.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if U_PLATFORM_USES_ONLY_WIN32_API

#include "wintz.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"

#include "unicode/ures.h"
#include "unicode/unistr.h"
#include "uresimp.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#include <windows.h>

U_NAMESPACE_BEGIN

/**
* Main Windows time zone detection function.
* Returns the Windows time zone converted to an ICU time zone as a heap-allocated buffer, or nullptr upon failure.
*
* Note: We use the Win32 API GetDynamicTimeZoneInformation (available since Vista+) to get the current time zone info.
* This API returns a non-localized time zone name, which is mapped to an ICU time zone ID (~ Olsen ID).
*/
U_CAPI const char* U_EXPORT2
uprv_detectWindowsTimeZone()
{
    // Obtain the DYNAMIC_TIME_ZONE_INFORMATION info to get the non-localized time zone name.
    DYNAMIC_TIME_ZONE_INFORMATION dynamicTZI;
    uprv_memset(&dynamicTZI, 0, sizeof(dynamicTZI));
    SYSTEMTIME systemTimeAllZero;
    uprv_memset(&systemTimeAllZero, 0, sizeof(systemTimeAllZero));

    if (GetDynamicTimeZoneInformation(&dynamicTZI) == TIME_ZONE_ID_INVALID) {
        return nullptr;
    }

    // If the DST setting has been turned off in the Control Panel, then return "Etc/GMT<offset>".
    //
    // Note: This logic is based on how the Control Panel itself determines if DST is 'off' on Windows.
    // The code is somewhat convoluted; in a sort of pseudo-code it looks like this:
    //
    //   IF (GetDynamicTimeZoneInformation != TIME_ZONE_ID_INVALID) && (DynamicDaylightTimeDisabled != 0) &&
    //      (StandardDate == DaylightDate) &&
    //      (
    //       (TimeZoneKeyName != Empty && StandardDate == 0) ||
    //       (TimeZoneKeyName == Empty && StandardDate != 0)
    //      )
    //   THEN
    //     DST setting is "Disabled".
    //
    if (dynamicTZI.DynamicDaylightTimeDisabled != 0 &&
        uprv_memcmp(&dynamicTZI.StandardDate, &dynamicTZI.DaylightDate, sizeof(dynamicTZI.StandardDate)) == 0 &&
        ((dynamicTZI.TimeZoneKeyName[0] != L'\0' && uprv_memcmp(&dynamicTZI.StandardDate, &systemTimeAllZero, sizeof(systemTimeAllZero)) == 0) ||
         (dynamicTZI.TimeZoneKeyName[0] == L'\0' && uprv_memcmp(&dynamicTZI.StandardDate, &systemTimeAllZero, sizeof(systemTimeAllZero)) != 0)))
    {
        LONG utcOffsetMins = dynamicTZI.Bias;
        if (utcOffsetMins == 0) {
            return uprv_strdup("Etc/UTC");
        }

        // No way to support when DST is turned off and the offset in minutes is not a multiple of 60.
        if (utcOffsetMins % 60 == 0) {
            char gmtOffsetTz[11] = {}; // "Etc/GMT+dd" is 11-char long with a terminal null.
            // Note '-' before 'utcOffsetMin'. The timezone ID's sign convention
            // is that a timezone ahead of UTC is Etc/GMT-<offset> and a timezone
            // behind UTC is Etc/GMT+<offset>.
            int ret = snprintf(gmtOffsetTz, UPRV_LENGTHOF(gmtOffsetTz), "Etc/GMT%+d", -utcOffsetMins / 60);
            if (ret > 0 && ret < UPRV_LENGTHOF(gmtOffsetTz)) {
                return uprv_strdup(gmtOffsetTz);
            }
        }
    }

    // If DST is NOT disabled, but we have an empty TimeZoneKeyName, then it is unclear
    // what we should do as this should not happen.
    if (dynamicTZI.TimeZoneKeyName[0] == 0) {
        return nullptr;
    }

    CharString winTZ;
    UErrorCode status = U_ZERO_ERROR;
    winTZ.appendInvariantChars(UnicodeString(TRUE, dynamicTZI.TimeZoneKeyName, -1), status);

    // Map Windows Timezone name (non-localized) to ICU timezone ID (~ Olson timezone id).
    StackUResourceBundle winTZBundle;
    ures_openDirectFillIn(winTZBundle.getAlias(), nullptr, "windowsZones", &status);
    ures_getByKey(winTZBundle.getAlias(), "mapTimezones", winTZBundle.getAlias(), &status);
    ures_getByKey(winTZBundle.getAlias(), winTZ.data(), winTZBundle.getAlias(), &status);

    if (U_FAILURE(status)) {
        return nullptr;
    }

    // Note: Since the ISO 3166 country/region codes are all invariant ASCII chars, we can
    // directly downcast from wchar_t to do the conversion.
    // We could call the A version of the GetGeoInfo API, but that would be slightly slower than calling the W API,
    // as the A version of the API will end up calling MultiByteToWideChar anyways internally.
    wchar_t regionCodeW[3] = {};
    char regionCode[3] = {}; // 2 letter ISO 3166 country/region code made entirely of invariant chars.
    int geoId = GetUserGeoID(GEOCLASS_NATION);
    int regionCodeLen = GetGeoInfoW(geoId, GEO_ISO2, regionCodeW, UPRV_LENGTHOF(regionCodeW), 0);

    const UChar *icuTZ16 = nullptr;
    int32_t tzLen;

    if (regionCodeLen != 0) {
        for (int i = 0; i < UPRV_LENGTHOF(regionCodeW); i++) {
            regionCode[i] = static_cast<char>(regionCodeW[i]);
        }
        icuTZ16 = ures_getStringByKey(winTZBundle.getAlias(), regionCode, &tzLen, &status);
    }
    if (regionCodeLen == 0 || U_FAILURE(status)) {
        // fallback to default "001" (world)
        status = U_ZERO_ERROR;
        icuTZ16 = ures_getStringByKey(winTZBundle.getAlias(), "001", &tzLen, &status);
    }

    // Note: cloneData returns nullptr if the status is a failure, so this
    // will return nullptr if the above look-up fails.
    CharString icuTZStr;
    return icuTZStr.appendInvariantChars(icuTZ16, tzLen, status).cloneData(status);
}

U_NAMESPACE_END
#endif /* U_PLATFORM_USES_ONLY_WIN32_API  */
