/*
******************************************************************************
*
*   Copyright (C) 2008-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  uspoof_buildwsconf.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2009Jan19
*   created by: Andy Heninger
*
*   Internal classes and functions
*   for compiling whole script confusable data into its binary (runtime) form.
*/

#ifndef __USPOOF_BUILDWSCONF_H__
#define __USPOOF_BUILDWSCONF_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#if !UCONFIG_NO_REGULAR_EXPRESSIONS 

#include "uspoof_impl.h"
#include "utrie2.h"


U_NAMESPACE_BEGIN

//
// class BuilderScriptSet.   Represents the set of scripts (Script Codes)
//             containing characters that are confusable with one specific
//             code point.
//

class BuilderScriptSet: public UMemory {
  public:
    UChar32      codePoint;       // The source code point.
    UTrie2      *trie;            // Any-case or Lower-case Trie.
                                  //   These Trie tables are the final result of the
                                  //   build.  This flag indicates which of the two
                                  //   this set of data is for.
    ScriptSet   *sset;            // The set of scripts itself.

                                  // Vectors of all B
    uint32_t     index;           // Index of this set in the Build Time vector
                                  //   of script sets.
    uint32_t     rindex;          // Index of this set in the final (runtime)
                                  //   array of sets.
    UBool        scriptSetOwned;  // True if this BuilderScriptSet owns (should delete)
                                  //   its underlying sset.

    BuilderScriptSet();
    ~BuilderScriptSet();
};


void buildWSConfusableData(SpoofImpl *spImpl, const char * confusablesWS,
          int32_t confusablesWSLen, UParseError *pe, UErrorCode &status); 

U_NAMESPACE_END

#endif // !UCONFIG_NO_REGULAR_EXPRESSIONS 
#endif // !UCONFIG_NO_NORMALIZATION 
#endif
