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
#include "cmemory.h"
#include "cstring.h"

#include "unicode/ures.h"
#include "unicode/ustring.h"
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

// The value of MAX_TIMEZONE_ID_LENGTH is 128, which is defined in DYNAMIC_TIME_ZONE_INFORMATION
#define MAX_TIMEZONE_ID_LENGTH 128

/**
* Main Windows time zone detection function.
* Returns the Windows time zone converted to an ICU time zone as a heap-allocated buffer, or nullptr upon failure.
* Note: We use the Win32 API GetDynamicTimeZoneInformation to get the current time zone info.
* This API returns a non-localized time zone name, which we can then map to an ICU time zone name.
*/
U_CFUNC const char* U_EXPORT2
uprv_detectWindowsTimeZone()
{
    UErrorCode status = U_ZERO_ERROR;
    char* icuid = nullptr;
    char dynamicTZKeyName[MAX_TIMEZONE_ID_LENGTH];
    char tmpid[MAX_TIMEZONE_ID_LENGTH];
    int32_t len;
    int id = GEOID_NOT_AVAILABLE;
    int errorCode;
    wchar_t ISOcodeW[3] = {}; /* 2 letter ISO code in UTF-16 */
    char ISOcode[3] = {}; /* 2 letter ISO code in UTF-8 */

    DYNAMIC_TIME_ZONE_INFORMATION dynamicTZI;
    uprv_memset(&dynamicTZI, 0, sizeof(dynamicTZI));
    uprv_memset(dynamicTZKeyName, 0, sizeof(dynamicTZKeyName));
    uprv_memset(tmpid, 0, sizeof(tmpid));

    /* Obtain TIME_ZONE_INFORMATION from the API and get the non-localized time zone name. */
    if (TIME_ZONE_ID_INVALID == GetDynamicTimeZoneInformation(&dynamicTZI)) {
        return nullptr;
    }

    id = GetUserGeoID(GEOCLASS_NATION);
    errorCode = GetGeoInfoW(id, GEO_ISO2, ISOcodeW, 3, 0);

    // convert from wchar_t* (UTF-16 on Windows) to char* (UTF-8).
    u_strToUTF8(ISOcode, UPRV_LENGTHOF(ISOcode), nullptr,
        reinterpret_cast<const UChar*>(ISOcodeW), UPRV_LENGTHOF(ISOcodeW), &status);

    LocalUResourceBundlePointer bundle(ures_openDirect(nullptr, "windowsZones", &status));
    ures_getByKey(bundle.getAlias(), "mapTimezones", bundle.getAlias(), &status);

    // convert from wchar_t* (UTF-16 on Windows) to char* (UTF-8).
    u_strToUTF8(dynamicTZKeyName, UPRV_LENGTHOF(dynamicTZKeyName), nullptr,
        reinterpret_cast<const UChar*>(dynamicTZI.TimeZoneKeyName), UPRV_LENGTHOF(dynamicTZI.TimeZoneKeyName), &status);

    if (U_FAILURE(status)) {
        return nullptr;
    }

    if (dynamicTZI.TimeZoneKeyName[0] != 0) {
        UResourceBundle winTZ;
        ures_initStackObject(&winTZ);
        ures_getByKey(bundle.getAlias(), dynamicTZKeyName, &winTZ, &status);

        if (U_SUCCESS(status)) {
            const UChar* icuTZ = nullptr;
            if (errorCode != 0) {
                icuTZ = ures_getStringByKey(&winTZ, ISOcode, &len, &status);
            }
            if (errorCode == 0 || icuTZ == nullptr) {
                /* fallback to default "001" and reset status */
                status = U_ZERO_ERROR;
                icuTZ = ures_getStringByKey(&winTZ, "001", &len, &status);
            }

            if (U_SUCCESS(status)) {
                int index = 0;

                while (!(*icuTZ == '\0' || *icuTZ == ' ')) {
                    // time zone IDs only contain ASCII invariant characters.
                    tmpid[index++] = (char)(*icuTZ++);
                }
                tmpid[index] = '\0';
            }
        }
        ures_close(&winTZ);
    }

    // Copy the timezone ID to icuid to be returned.
    if (tmpid[0] != 0) {
        icuid = uprv_strdup(tmpid);
    }

    return icuid;
}

U_NAMESPACE_END
#endif /* U_PLATFORM_USES_ONLY_WIN32_API  */
