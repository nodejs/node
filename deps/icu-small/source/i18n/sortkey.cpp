// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1996-2012, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/
//===============================================================================
//
// File sortkey.cpp
//
//
//
// Created by: Helena Shih
//
// Modification History:
//
//  Date         Name          Description
//
//  6/20/97      helena        Java class name change.
//  6/23/97      helena        Added comments to make code more readable.
//  6/26/98      erm           Canged to use byte arrays instead of UnicodeString
//  7/31/98      erm           hashCode: minimum inc should be 2 not 1,
//                             Cleaned up operator=
// 07/12/99      helena        HPUX 11 CC port.
// 03/06/01      synwee        Modified compareTo, to handle the result of
//                             2 string similar in contents, but one is longer
//                             than the other
//===============================================================================

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/sortkey.h"
#include "cmemory.h"
#include "uelement.h"
#include "ustr_imp.h"

U_NAMESPACE_BEGIN

// A hash code of kInvalidHashCode indicates that the hash code needs
// to be computed. A hash code of kEmptyHashCode is used for empty keys
// and for any key whose computed hash code is kInvalidHashCode.
static const int32_t kInvalidHashCode = 0;
static const int32_t kEmptyHashCode = 1;
// The "bogus hash code" replaces a separate fBogus flag.
static const int32_t kBogusHashCode = 2;

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CollationKey)

CollationKey::CollationKey()
    : UObject(), fFlagAndLength(0),
      fHashCode(kEmptyHashCode)
{
}

// Create a collation key from a bit array.
CollationKey::CollationKey(const uint8_t* newValues, int32_t count)
    : UObject(), fFlagAndLength(count),
      fHashCode(kInvalidHashCode)
{
    if (count < 0 || (newValues == NULL && count != 0) ||
            (count > getCapacity() && reallocate(count, 0) == NULL)) {
        setToBogus();
        return;
    }

    if (count > 0) {
        uprv_memcpy(getBytes(), newValues, count);
    }
}

CollationKey::CollationKey(const CollationKey& other)
    : UObject(other), fFlagAndLength(other.getLength()),
      fHashCode(other.fHashCode)
{
    if (other.isBogus())
    {
        setToBogus();
        return;
    }

    int32_t length = fFlagAndLength;
    if (length > getCapacity() && reallocate(length, 0) == NULL) {
        setToBogus();
        return;
    }

    if (length > 0) {
        uprv_memcpy(getBytes(), other.getBytes(), length);
    }
}

CollationKey::~CollationKey()
{
    if(fFlagAndLength < 0) { uprv_free(fUnion.fFields.fBytes); }
}

uint8_t *CollationKey::reallocate(int32_t newCapacity, int32_t length) {
    uint8_t *newBytes = static_cast<uint8_t *>(uprv_malloc(newCapacity));
    if(newBytes == NULL) { return NULL; }
    if(length > 0) {
        uprv_memcpy(newBytes, getBytes(), length);
    }
    if(fFlagAndLength < 0) { uprv_free(fUnion.fFields.fBytes); }
    fUnion.fFields.fBytes = newBytes;
    fUnion.fFields.fCapacity = newCapacity;
    fFlagAndLength |= 0x80000000;
    return newBytes;
}

void CollationKey::setLength(int32_t newLength) {
    // U_ASSERT(newLength >= 0 && newLength <= getCapacity());
    fFlagAndLength = (fFlagAndLength & 0x80000000) | newLength;
    fHashCode = kInvalidHashCode;
}

// set the key to an empty state
CollationKey&
CollationKey::reset()
{
    fFlagAndLength &= 0x80000000;
    fHashCode = kEmptyHashCode;

    return *this;
}

// set the key to a "bogus" or invalid state
CollationKey&
CollationKey::setToBogus()
{
    fFlagAndLength &= 0x80000000;
    fHashCode = kBogusHashCode;

    return *this;
}

UBool
CollationKey::operator==(const CollationKey& source) const
{
    return getLength() == source.getLength() &&
            (this == &source ||
             uprv_memcmp(getBytes(), source.getBytes(), getLength()) == 0);
}

const CollationKey&
CollationKey::operator=(const CollationKey& other)
{
    if (this != &other)
    {
        if (other.isBogus())
        {
            return setToBogus();
        }

        int32_t length = other.getLength();
        if (length > getCapacity() && reallocate(length, 0) == NULL) {
            return setToBogus();
        }
        if (length > 0) {
            uprv_memcpy(getBytes(), other.getBytes(), length);
        }
        fFlagAndLength = (fFlagAndLength & 0x80000000) | length;
        fHashCode = other.fHashCode;
    }

    return *this;
}

// Bitwise comparison for the collation keys.
Collator::EComparisonResult
CollationKey::compareTo(const CollationKey& target) const
{
    UErrorCode errorCode = U_ZERO_ERROR;
    return static_cast<Collator::EComparisonResult>(compareTo(target, errorCode));
}

// Bitwise comparison for the collation keys.
UCollationResult
CollationKey::compareTo(const CollationKey& target, UErrorCode &status) const
{
  if(U_SUCCESS(status)) {
    const uint8_t *src = getBytes();
    const uint8_t *tgt = target.getBytes();

    // are we comparing the same string
    if (src == tgt)
        return  UCOL_EQUAL;

    UCollationResult result;

    // are we comparing different lengths?
    int32_t minLength = getLength();
    int32_t targetLength = target.getLength();
    if (minLength < targetLength) {
        result = UCOL_LESS;
    } else if (minLength == targetLength) {
        result = UCOL_EQUAL;
    } else {
        minLength = targetLength;
        result = UCOL_GREATER;
    }

    if (minLength > 0) {
        int diff = uprv_memcmp(src, tgt, minLength);
        if (diff > 0) {
            return UCOL_GREATER;
        }
        else
            if (diff < 0) {
                return UCOL_LESS;
            }
    }

    return result;
  } else {
    return UCOL_EQUAL;
  }
}

#ifdef U_USE_COLLATION_KEY_DEPRECATES
// Create a copy of the byte array.
uint8_t*
CollationKey::toByteArray(int32_t& count) const
{
    uint8_t *result = (uint8_t*) uprv_malloc( sizeof(uint8_t) * fCount );

    if (result == NULL)
    {
        count = 0;
    }
    else
    {
        count = fCount;
        if (count > 0) {
            uprv_memcpy(result, fBytes, fCount);
        }
    }

    return result;
}
#endif

static int32_t
computeHashCode(const uint8_t *key, int32_t  length) {
    const char *s = reinterpret_cast<const char *>(key);
    int32_t hash;
    if (s == NULL || length == 0) {
        hash = kEmptyHashCode;
    } else {
        hash = ustr_hashCharsN(s, length);
        if (hash == kInvalidHashCode || hash == kBogusHashCode) {
            hash = kEmptyHashCode;
        }
    }
    return hash;
}

int32_t
CollationKey::hashCode() const
{
    // (Cribbed from UnicodeString)
    // We cache the hashCode; when it becomes invalid, due to any change to the
    // string, we note this by setting it to kInvalidHashCode. [LIU]

    // Note: This method is semantically const, but physically non-const.

    if (fHashCode == kInvalidHashCode)
    {
        fHashCode = computeHashCode(getBytes(), getLength());
    }

    return fHashCode;
}

U_NAMESPACE_END

U_CAPI int32_t U_EXPORT2
ucol_keyHashCode(const uint8_t *key,
                       int32_t  length)
{
    return icu::computeHashCode(key, length);
}

#endif /* #if !UCONFIG_NO_COLLATION */
