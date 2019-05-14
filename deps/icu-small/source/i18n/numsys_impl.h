// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File NUMSYS_IMPL.H
*
*******************************************************************************
*/

#ifndef __NUMSYS_IMPL_H__
#define __NUMSYS_IMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/numsys.h"
#include "uvector.h"
#include "unicode/strenum.h"

U_NAMESPACE_BEGIN

class NumsysNameEnumeration : public StringEnumeration {
public:
    NumsysNameEnumeration(UErrorCode& status);

    virtual ~NumsysNameEnumeration();
    static UClassID U_EXPORT2 getStaticClassID(void);
    virtual UClassID getDynamicClassID(void) const override;
    virtual const UnicodeString* snext(UErrorCode& status) override;
    virtual void reset(UErrorCode& status) override;
    virtual int32_t count(UErrorCode& status) const override;
private:
    int32_t pos;
};

U_NAMESPACE_END

#endif

#endif
