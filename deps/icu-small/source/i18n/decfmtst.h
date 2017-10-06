// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2009-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* This file contains declarations for the class DecimalFormatStaticSets
*
* DecimalFormatStaticSets holds the UnicodeSets that are needed for lenient
* parsing of decimal and group separators.
********************************************************************************
*/

#ifndef DECFMTST_H
#define DECFMTST_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"

U_NAMESPACE_BEGIN

class  UnicodeSet;


class DecimalFormatStaticSets : public UMemory
{
public:
    // Constructor and Destructor not for general use.
    //   Public to permit access from plain C implementation functions.
    DecimalFormatStaticSets(UErrorCode &status);
    ~DecimalFormatStaticSets();

    /**
      * Return a pointer to a lazy-initialized singleton instance of this class.
      */
    static const DecimalFormatStaticSets *getStaticSets(UErrorCode &status);

    static const UnicodeSet *getSimilarDecimals(UChar32 decimal, UBool strictParse);

    UnicodeSet *fDotEquivalents;
    UnicodeSet *fCommaEquivalents;
    UnicodeSet *fOtherGroupingSeparators;
    UnicodeSet *fDashEquivalents;

    UnicodeSet *fStrictDotEquivalents;
    UnicodeSet *fStrictCommaEquivalents;
    UnicodeSet *fStrictOtherGroupingSeparators;
    UnicodeSet *fStrictDashEquivalents;

    UnicodeSet *fDefaultGroupingSeparators;
    UnicodeSet *fStrictDefaultGroupingSeparators;

    UnicodeSet *fMinusSigns;
    UnicodeSet *fPlusSigns;
private:
    void cleanup();

};


U_NAMESPACE_END

#endif   // !UCONFIG_NO_FORMATTING
#endif   // DECFMTST_H
