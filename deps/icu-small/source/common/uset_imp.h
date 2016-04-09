/*
*******************************************************************************
*
*   Copyright (C) 2004-2007, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uset_imp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004sep07
*   created by: Markus W. Scherer
*
*   Internal USet definitions.
*/

#ifndef __USET_IMP_H__
#define __USET_IMP_H__

#include "unicode/utypes.h"
#include "unicode/uset.h"

U_CDECL_BEGIN

typedef void U_CALLCONV
USetAdd(USet *set, UChar32 c);

typedef void U_CALLCONV
USetAddRange(USet *set, UChar32 start, UChar32 end);

typedef void U_CALLCONV
USetAddString(USet *set, const UChar *str, int32_t length);

typedef void U_CALLCONV
USetRemove(USet *set, UChar32 c);

typedef void U_CALLCONV
USetRemoveRange(USet *set, UChar32 start, UChar32 end);

/**
 * Interface for adding items to a USet, to keep low-level code from
 * statically depending on the USet implementation.
 * Calls will look like sa->add(sa->set, c);
 */
struct USetAdder {
    USet *set;
    USetAdd *add;
    USetAddRange *addRange;
    USetAddString *addString;
    USetRemove *remove;
    USetRemoveRange *removeRange;
};
typedef struct USetAdder USetAdder;

U_CDECL_END

#endif

