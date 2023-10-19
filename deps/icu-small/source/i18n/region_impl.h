// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
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
    /**
     * Construct an string enumeration over the supplied name list.
     * Makes a copy of the supplied input name list; does not retain a reference to the original.
     */
    RegionNameEnumeration(UVector *nameList, UErrorCode& status);
    virtual ~RegionNameEnumeration();
    static UClassID U_EXPORT2 getStaticClassID();
    virtual UClassID getDynamicClassID() const override;
    virtual const UnicodeString* snext(UErrorCode& status) override;
    virtual void reset(UErrorCode& status) override;
    virtual int32_t count(UErrorCode& status) const override;
private:
    int32_t pos;
    UVector *fRegionNames;
};

U_NAMESPACE_END

#endif

#endif
