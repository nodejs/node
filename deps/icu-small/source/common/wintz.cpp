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

// This file contains only desktop Windows behavior
// Windows UWP calls Windows::Globalization directly, so this isn't needed there.
#if U_PLATFORM_USES_ONLY_WIN32_API && (U_PLATFORM_HAS_WINUWP_API == 0)

#include "wintz.h"
#include "cmemory.h"
#include "cstring.h"

#include "unicode/ures.h"
#include "unicode/ustring.h"

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX
#include <windows.h>

#define MAX_LENGTH_ID 40

/* The layout of the Tzi value in the registry */
typedef struct
{
    int32_t bias;
    int32_t standardBias;
    int32_t daylightBias;
    SYSTEMTIME standardDate;
    SYSTEMTIME daylightDate;
} TZI;

/**
 * Various registry keys and key fragments.
 */
static const char CURRENT_ZONE_REGKEY[] = "SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation\\";
static const char STANDARD_TIME_REGKEY[] = " Standard Time";
static const char TZI_REGKEY[] = "TZI";
static const char STD_REGKEY[] = "Std";

/**
 * The time zone root keys (under HKLM) for Win7+
 */
static const char TZ_REGKEY[] = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones\\";

static LONG openTZRegKey(HKEY *hkey, const char *winid)
{
    char subKeyName[110]; /* TODO: why 110?? */
    char *name;
    LONG result;

    uprv_strcpy(subKeyName, TZ_REGKEY);
    name = &subKeyName[strlen(subKeyName)];
    uprv_strcat(subKeyName, winid);

    result = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                            subKeyName,
                            0,
                            KEY_QUERY_VALUE,
                            hkey);
    return result;
}

static LONG getTZI(const char *winid, TZI *tzi)
{
    DWORD cbData = sizeof(TZI);
    LONG result;
    HKEY hkey;

    result = openTZRegKey(&hkey, winid);

    if (result == ERROR_SUCCESS)
    {
        result = RegQueryValueExA(hkey,
                                    TZI_REGKEY,
                                    NULL,
                                    NULL,
                                    (LPBYTE)tzi,
                                    &cbData);
        RegCloseKey(hkey);
    }

    return result;
}

static LONG getSTDName(const char *winid, char *regStdName, int32_t length)
{
    DWORD cbData = length;
    LONG result;
    HKEY hkey;

    result = openTZRegKey(&hkey, winid);

    if (result == ERROR_SUCCESS)
    {
        result = RegQueryValueExA(hkey,
                                    STD_REGKEY,
                                    NULL,
                                    NULL,
                                    (LPBYTE)regStdName,
                                    &cbData);
        RegCloseKey(hkey);
    }

    return result;
}

static LONG getTZKeyName(char* tzKeyName, int32_t length)
{
    HKEY hkey;
    LONG result = FALSE;
    DWORD cbData = length;

    if(ERROR_SUCCESS == RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        CURRENT_ZONE_REGKEY,
        0,
        KEY_QUERY_VALUE,
        &hkey))
    {
         result = RegQueryValueExA(
             hkey,
             "TimeZoneKeyName",
             NULL,
             NULL,
             (LPBYTE)tzKeyName,
             &cbData);

        RegCloseKey(hkey);
    }

    return result;
}

/*
  This code attempts to detect the Windows time zone directly,
  as set in the Windows Date and Time control panel.  It attempts
  to work on versions greater than Windows Vista and on localized
  installs.  It works by directly interrogating the registry and
  comparing the data there with the data returned by the
  GetTimeZoneInformation API, along with some other strategies.  The
  registry contains time zone data under this key:

    HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones\

  Under this key are several subkeys, one for each time zone.  For
  example these subkeys are named "Pacific Standard Time" on Vista+.
  There are some other wrinkles; see the code for
  details.  The subkey name is NOT LOCALIZED, allowing us to support
  localized installs.

  Under the subkey are data values.  We care about:

    Std   Standard time display name, localized
    TZI   Binary block of data

  The TZI data is of particular interest.  It contains the offset, two
  more offsets for standard and daylight time, and the start and end
  rules.  This is the same data returned by the GetTimeZoneInformation
  API.  The API may modify the data on the way out, so we have to be
  careful, but essentially we do a binary comparison against the TZI
  blocks of various registry keys.  When we find a match, we know what
  time zone Windows is set to.  Since the registry key is not
  localized, we can then translate the key through a simple table
  lookup into the corresponding ICU time zone.

  This strategy doesn't always work because there are zones which
  share an offset and rules, so more than one TZI block will match.
  For example, both Tokyo and Seoul are at GMT+9 with no DST rules;
  their TZI blocks are identical.  For these cases, we fall back to a
  name lookup.  We attempt to match the display name as stored in the
  registry for the current zone to the display name stored in the
  registry for various Windows zones.  By comparing the registry data
  directly we avoid conversion complications.

  Author: Alan Liu
  Since: ICU 2.6
  Based on original code by Carl Brown <cbrown@xnetinc.com>
*/

/**
 * Main Windows time zone detection function.  Returns the Windows
 * time zone, translated to an ICU time zone, or NULL upon failure.
 */
U_CFUNC const char* U_EXPORT2
uprv_detectWindowsTimeZone()
{
    UErrorCode status = U_ZERO_ERROR;
    UResourceBundle* bundle = NULL;
    char* icuid = NULL;
    char apiStdName[MAX_LENGTH_ID];
    char regStdName[MAX_LENGTH_ID];
    char tmpid[MAX_LENGTH_ID];
    int32_t len;
    int id;
    int errorCode;
    wchar_t ISOcodeW[3]; /* 2 letter iso code in UTF-16*/
    char  ISOcodeA[3]; /* 2 letter iso code in ansi */

    LONG result;
    TZI tziKey;
    TZI tziReg;
    TIME_ZONE_INFORMATION apiTZI;

    BOOL tryPreVistaFallback;
    OSVERSIONINFO osVerInfo;

    /* Obtain TIME_ZONE_INFORMATION from the API, and then convert it
       to TZI.  We could also interrogate the registry directly; we do
       this below if needed. */
    uprv_memset(&apiTZI, 0, sizeof(apiTZI));
    uprv_memset(&tziKey, 0, sizeof(tziKey));
    uprv_memset(&tziReg, 0, sizeof(tziReg));
    GetTimeZoneInformation(&apiTZI);
    tziKey.bias = apiTZI.Bias;
    uprv_memcpy((char *)&tziKey.standardDate, (char*)&apiTZI.StandardDate,
           sizeof(apiTZI.StandardDate));
    uprv_memcpy((char *)&tziKey.daylightDate, (char*)&apiTZI.DaylightDate,
           sizeof(apiTZI.DaylightDate));

    /* Convert the wchar_t* standard name to char* */
    uprv_memset(apiStdName, 0, sizeof(apiStdName));
    wcstombs(apiStdName, apiTZI.StandardName, MAX_LENGTH_ID);

    tmpid[0] = 0;

    id = GetUserGeoID(GEOCLASS_NATION);
    errorCode = GetGeoInfoW(id, GEO_ISO2, ISOcodeW, 3, 0);
    u_strToUTF8(ISOcodeA, 3, NULL, (const UChar *)ISOcodeW, 3, &status);

    bundle = ures_openDirect(NULL, "windowsZones", &status);
    ures_getByKey(bundle, "mapTimezones", bundle, &status);

    /*
        Windows Vista+ provides us with a "TimeZoneKeyName" that is not localized
        and can be used to directly map a name in our bundle. Try to use that first
        if we're on Vista or higher
    */
    uprv_memset(&osVerInfo, 0, sizeof(osVerInfo));
    osVerInfo.dwOSVersionInfoSize = sizeof(osVerInfo);
    tryPreVistaFallback = TRUE;
    result = getTZKeyName(regStdName, sizeof(regStdName));
    if(ERROR_SUCCESS == result)
    {
        UResourceBundle* winTZ = ures_getByKey(bundle, regStdName, NULL, &status);
        if(U_SUCCESS(status))
        {
            const UChar* icuTZ = NULL;
            if (errorCode != 0)
            {
                icuTZ = ures_getStringByKey(winTZ, ISOcodeA, &len, &status);
            }
            if (errorCode==0 || icuTZ==NULL)
            {
                /* fallback to default "001" and reset status */
                status = U_ZERO_ERROR;
                icuTZ = ures_getStringByKey(winTZ, "001", &len, &status);
            }

            if(U_SUCCESS(status))
            {
                int index=0;
                while (! (*icuTZ == '\0' || *icuTZ ==' '))
                {
                    tmpid[index++]=(char)(*icuTZ++);  /* safe to assume 'char' is ASCII compatible on windows */
                }
                tmpid[index]='\0';
                tryPreVistaFallback = FALSE;
            }
        }
        ures_close(winTZ);
    }

    if(tryPreVistaFallback)
    {
        /* Note: We get the winid not from static tables but from resource bundle. */
        while (U_SUCCESS(status) && ures_hasNext(bundle))
        {
            UBool idFound = FALSE;
            const char* winid;
            UResourceBundle* winTZ = ures_getNextResource(bundle, NULL, &status);
            if (U_FAILURE(status))
            {
                break;
            }
            winid = ures_getKey(winTZ);
            result = getTZI(winid, &tziReg);

            if (result == ERROR_SUCCESS)
            {
                /* Windows alters the DaylightBias in some situations.
                   Using the bias and the rules suffices, so overwrite
                   these unreliable fields. */
                tziKey.standardBias = tziReg.standardBias;
                tziKey.daylightBias = tziReg.daylightBias;

                if (uprv_memcmp((char *)&tziKey, (char*)&tziReg, sizeof(tziKey)) == 0)
                {
                    const UChar* icuTZ = NULL;
                    if (errorCode != 0)
                    {
                        icuTZ = ures_getStringByKey(winTZ, ISOcodeA, &len, &status);
                    }
                    if (errorCode==0 || icuTZ==NULL)
                    {
                        /* fallback to default "001" and reset status */
                        status = U_ZERO_ERROR;
                        icuTZ = ures_getStringByKey(winTZ, "001", &len, &status);
                    }

                    if (U_SUCCESS(status))
                    {
                        /* Get the standard name from the registry key to compare with
                           the one from Windows API call. */
                        uprv_memset(regStdName, 0, sizeof(regStdName));
                        result = getSTDName(winid, regStdName, sizeof(regStdName));
                        if (result == ERROR_SUCCESS)
                        {
                            if (uprv_strcmp(apiStdName, regStdName) == 0)
                            {
                                idFound = TRUE;
                            }
                        }

                        /* tmpid buffer holds the ICU timezone ID corresponding to the timezone ID from Windows.
                         * If none is found, tmpid buffer will contain a fallback ID (i.e. the time zone ID matching
                         * the current time zone information)
                         */
                        if (idFound || tmpid[0] == 0)
                        {
                            /* if icuTZ has more than one city, take only the first (i.e. terminate icuTZ at first space) */
                            int index=0;
                            while (! (*icuTZ == '\0' || *icuTZ ==' '))
                            {
                                tmpid[index++]=(char)(*icuTZ++);  /* safe to assume 'char' is ASCII compatible on windows */
                            }
                            tmpid[index]='\0';
                        }
                    }
                }
            }
            ures_close(winTZ);
            if (idFound)
            {
                break;
            }
        }
    }

    /*
     * Copy the timezone ID to icuid to be returned.
     */
    if (tmpid[0] != 0)
    {
        len = uprv_strlen(tmpid);
        icuid = (char*)uprv_calloc(len + 1, sizeof(char));
        if (icuid != NULL)
        {
            uprv_strcpy(icuid, tmpid);
        }
    }

    ures_close(bundle);

    return icuid;
}

#endif /* U_PLATFORM_USES_ONLY_WIN32_API && (U_PLATFORM_HAS_WINUWP_API == 0) */
