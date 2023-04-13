// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2009-2013, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* This file contains declarations for the class SimpleDateFormatStaticSets
*
* SimpleDateFormatStaticSets holds the UnicodeSets that are needed for lenient
* parsing of literal characters in date/time strings.
********************************************************************************
*/

#ifndef SMPDTFST_H
#define SMPDTFST_H

#include "unicode/uobject.h"
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udat.h"

U_NAMESPACE_BEGIN

class  UnicodeSet;


class SimpleDateFormatStaticSets : public UMemory
{
public:
    SimpleDateFormatStaticSets(UErrorCode &status);
    ~SimpleDateFormatStaticSets();
    
    static void    initSets(UErrorCode *status);
    static UBool   cleanup();
    
    static UnicodeSet *getIgnorables(UDateFormatField fieldIndex);
    
private:
    UnicodeSet *fDateIgnorables;
    UnicodeSet *fTimeIgnorables;
    UnicodeSet *fOtherIgnorables;
};


U_NAMESPACE_END

#endif   // #if !UCONFIG_NO_FORMATTING
#endif   // SMPDTFST_H
