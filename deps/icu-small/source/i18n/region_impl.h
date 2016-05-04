/*
*******************************************************************************
* Copyright (C) 2013, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File REGION_IMPL.H
*
*******************************************************************************
*/

#ifndef __REGION_IMPL_H__
#define __REGION_IMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
    
#include "uvector.h"
#include "unicode/strenum.h"

U_NAMESPACE_BEGIN


class RegionNameEnumeration : public StringEnumeration {
public:
    RegionNameEnumeration(UVector *fNameList, UErrorCode& status);
    virtual ~RegionNameEnumeration();
    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const;
    virtual const UnicodeString* snext(UErrorCode& status);
    virtual void reset(UErrorCode& status);
    virtual int32_t count(UErrorCode& status) const;
private:
    int32_t pos;
    UVector *fRegionNames;
};

U_NAMESPACE_END

#endif

#endif
