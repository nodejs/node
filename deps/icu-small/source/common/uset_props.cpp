// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2002-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uset_props.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004aug30
*   created by: Markus W. Scherer
*
*   C wrappers around UnicodeSet functions that are implemented in
*   uniset_props.cpp, split off for modularization.
*/

#include "unicode/utypes.h"
#include "unicode/uobject.h"
#include "unicode/uset.h"
#include "unicode/uniset.h"
#include "cmemory.h"
#include "unicode/ustring.h"
#include "unicode/parsepos.h"

U_NAMESPACE_USE

U_CAPI USet* U_EXPORT2
uset_openPattern(const UChar* pattern, int32_t patternLength,
                 UErrorCode* ec)
{
    UnicodeString pat(patternLength==-1, pattern, patternLength);
    UnicodeSet* set = new UnicodeSet(pat, *ec);
    /* test for NULL */
    if(set == 0) {
        *ec = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }

    if (U_FAILURE(*ec)) {
        delete set;
        set = NULL;
    }
    return (USet*) set;
}

U_CAPI USet* U_EXPORT2
uset_openPatternOptions(const UChar* pattern, int32_t patternLength,
                 uint32_t options,
                 UErrorCode* ec)
{
    UnicodeString pat(patternLength==-1, pattern, patternLength);
    UnicodeSet* set = new UnicodeSet(pat, options, NULL, *ec);
    /* test for NULL */
    if(set == 0) {
        *ec = U_MEMORY_ALLOCATION_ERROR;
        return 0;
    }

    if (U_FAILURE(*ec)) {
        delete set;
        set = NULL;
    }
    return (USet*) set;
}


U_CAPI int32_t U_EXPORT2
uset_applyPattern(USet *set,
                  const UChar *pattern, int32_t patternLength,
                  uint32_t options,
                  UErrorCode *status){

    // status code needs to be checked since we
    // dereference it
    if(status == NULL || U_FAILURE(*status)){
        return 0;
    }

    // check only the set paramenter
    // if pattern is NULL or null terminate
    // UnicodeString constructor takes care of it
    if(set == NULL){
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UnicodeString pat(pattern, patternLength);

    ParsePosition pos;

    ((UnicodeSet*) set)->applyPattern(pat, pos, options, NULL, *status);

    return pos.getIndex();
}

U_CAPI void U_EXPORT2
uset_applyIntPropertyValue(USet* set,
               UProperty prop, int32_t value, UErrorCode* ec) {
    ((UnicodeSet*) set)->applyIntPropertyValue(prop, value, *ec);
}

U_CAPI void U_EXPORT2
uset_applyPropertyAlias(USet* set,
                        const UChar *prop, int32_t propLength,
                        const UChar *value, int32_t valueLength,
            UErrorCode* ec) {

    UnicodeString p(prop, propLength);
    UnicodeString v(value, valueLength);

    ((UnicodeSet*) set)->applyPropertyAlias(p, v, *ec);
}

U_CAPI UBool U_EXPORT2
uset_resemblesPattern(const UChar *pattern, int32_t patternLength,
                      int32_t pos) {

    UnicodeString pat(pattern, patternLength);

    return ((pos+1) < pat.length() &&
            pat.charAt(pos) == (UChar)91/*[*/) ||
            UnicodeSet::resemblesPattern(pat, pos);
}

U_CAPI int32_t U_EXPORT2
uset_toPattern(const USet* set,
               UChar* result, int32_t resultCapacity,
               UBool escapeUnprintable,
               UErrorCode* ec) {
    UnicodeString pat;
    ((const UnicodeSet*) set)->toPattern(pat, escapeUnprintable);
    return pat.extract(result, resultCapacity, *ec);
}

U_CAPI void U_EXPORT2
uset_closeOver(USet* set, int32_t attributes) {
    ((UnicodeSet*) set)->UnicodeSet::closeOver(attributes);
}
