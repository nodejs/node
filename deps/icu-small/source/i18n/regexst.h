//
//  regexst.h
//
//  Copyright (C) 2003-2010, International Business Machines Corporation and others.
//  All Rights Reserved.
//
//  This file contains declarations for the class RegexStaticSets
//
//  This class is internal to the regular expression implementation.
//  For the public Regular Expression API, see the file "unicode/regex.h"
//
//  RegexStaticSets groups together the common UnicodeSets that are needed
//   for compiling or executing RegularExpressions.  This grouping simplifies
//   the thread safe lazy creation and sharing of these sets across
//   all instances of regular expressions.
//

#ifndef REGEXST_H
#define REGEXST_H

#include "unicode/utypes.h"
#include "unicode/utext.h"
#if !UCONFIG_NO_REGULAR_EXPRESSIONS

#include "regeximp.h"

U_NAMESPACE_BEGIN

class  UnicodeSet;


class RegexStaticSets : public UMemory {
public:
    static RegexStaticSets *gStaticSets;  // Ptr to all lazily initialized constant
                                          //   shared sets.

    RegexStaticSets(UErrorCode *status);         
    ~RegexStaticSets();
    static void    initGlobals(UErrorCode *status);
    static UBool   cleanup();

    UnicodeSet    *fPropSets[URX_LAST_SET];     // The sets for common regex items, e.g. \s
    Regex8BitSet   fPropSets8[URX_LAST_SET];    // Fast bitmap sets for latin-1 range for above.

    UnicodeSet    fRuleSets[10];               // Sets used while parsing regexp patterns.
    UnicodeSet    fUnescapeCharSet;            // Set of chars handled by unescape when
                                               //   encountered with a \ in a pattern.
    UnicodeSet    *fRuleDigitsAlias;
    UText         *fEmptyText;                 // An empty string, to be used when a matcher
                                               //   is created with no input.

};


U_NAMESPACE_END
#endif   // !UCONFIG_NO_REGULAR_EXPRESSIONS
#endif   // REGEXST_H

