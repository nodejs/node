// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 1997-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uelement.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011jul04
*   created by: Markus W. Scherer
*
*   Common definitions for UHashTable and UVector.
*   UHashTok moved here from uhash.h and renamed UElement.
*   This allows users of UVector to avoid the confusing #include of uhash.h.
*   uhash.h aliases UElement to UHashTok,
*   so that we need not change all of its code and its users.
*/

#ifndef __UELEMENT_H__
#define __UELEMENT_H__

#include "unicode/utypes.h"

U_CDECL_BEGIN

/**
 * A UVector element, or a key or value within a UHashtable.
 * It may be either a 32-bit integral value or an opaque void* pointer.
 * The void* pointer may be smaller than 32 bits (e.g. 24 bits)
 * or may be larger (e.g. 64 bits).
 *
 * Because a UElement is the size of a native pointer or a 32-bit
 * integer, we pass it around by value.
 */
union UElement {
    void*   pointer;
    int32_t integer;
};
typedef union UElement UElement;

/**
 * An element-equality (boolean) comparison function.
 * @param e1 An element (object or integer)
 * @param e2 An element (object or integer)
 * @return TRUE if the two elements are equal.
 */
typedef UBool U_CALLCONV UElementsAreEqual(const UElement e1, const UElement e2);

/**
 * An element sorting (three-way) comparison function.
 * @param e1 An element (object or integer)
 * @param e2 An element (object or integer)
 * @return 0 if the two elements are equal, -1 if e1 is < e2, or +1 if e1 is > e2.
 */
typedef int8_t U_CALLCONV UElementComparator(UElement e1, UElement e2);

/**
 * An element assignment function.  It may copy an integer, copy
 * a pointer, or clone a pointer, as appropriate.
 * @param dst The element to be assigned to
 * @param src The element to assign from
 */
typedef void U_CALLCONV UElementAssigner(UElement *dst, UElement *src);

U_CDECL_END

/**
 * Comparator function for UnicodeString* keys. Implements UElementsAreEqual.
 * @param key1 The string for comparison
 * @param key2 The string for comparison
 * @return true if key1 and key2 are equal, return false otherwise.
 */
U_CAPI UBool U_EXPORT2 
uhash_compareUnicodeString(const UElement key1, const UElement key2);

/**
 * Comparator function for UnicodeString* keys (case insensitive).
 * Make sure to use together with uhash_hashCaselessUnicodeString.
 * Implements UElementsAreEqual.
 * @param key1 The string for comparison
 * @param key2 The string for comparison
 * @return true if key1 and key2 are equal, return false otherwise.
 */
U_CAPI UBool U_EXPORT2 
uhash_compareCaselessUnicodeString(const UElement key1, const UElement key2);

#endif  /* __UELEMENT_H__ */
