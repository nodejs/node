// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2011-2012, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef __TZGNAMES_H
#define __TZGNAMES_H

/**
 * \file 
 * \brief C API: Time zone generic names classe
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/tzfmt.h"
#include "unicode/tznames.h"

U_CDECL_BEGIN

typedef enum UTimeZoneGenericNameType {
    UTZGNM_UNKNOWN      = 0x00,
    UTZGNM_LOCATION     = 0x01,
    UTZGNM_LONG         = 0x02,
    UTZGNM_SHORT        = 0x04
} UTimeZoneGenericNameType;

U_CDECL_END

U_NAMESPACE_BEGIN

class TimeZone;
struct TZGNCoreRef;

class U_I18N_API TimeZoneGenericNames : public UMemory {
public:
    virtual ~TimeZoneGenericNames();

    static TimeZoneGenericNames* createInstance(const Locale& locale, UErrorCode& status);

    virtual UBool operator==(const TimeZoneGenericNames& other) const;
    virtual UBool operator!=(const TimeZoneGenericNames& other) const {return !operator==(other);};
    virtual TimeZoneGenericNames* clone() const;

    UnicodeString& getDisplayName(const TimeZone& tz, UTimeZoneGenericNameType type,
                        UDate date, UnicodeString& name) const;

    UnicodeString& getGenericLocationName(const UnicodeString& tzCanonicalID, UnicodeString& name) const;

    int32_t findBestMatch(const UnicodeString& text, int32_t start, uint32_t types,
        UnicodeString& tzID, UTimeZoneFormatTimeType& timeType, UErrorCode& status) const;

private:
    TimeZoneGenericNames();
    TZGNCoreRef* fRef;
};

U_NAMESPACE_END
#endif
#endif
