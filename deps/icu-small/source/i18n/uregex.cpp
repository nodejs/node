// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2004-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  uregex.cpp
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_REGULAR_EXPRESSIONS

#include "unicode/regex.h"
#include "unicode/uregex.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "unicode/uchar.h"
#include "unicode/uobject.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "uassert.h"
#include "uhash.h"
#include "umutex.h"
#include "uvectr32.h"

#include "regextxt.h"

U_NAMESPACE_BEGIN

#define REMAINING_CAPACITY(idx,len) ((((len)-(idx))>0)?((len)-(idx)):0)

struct RegularExpression: public UMemory {
public:
    RegularExpression();
    ~RegularExpression();
    int32_t           fMagic;
    RegexPattern     *fPat;
    u_atomic_int32_t *fPatRefCount;
    UChar            *fPatString;
    int32_t           fPatStringLen;
    RegexMatcher     *fMatcher;
    const UChar      *fText;         // Text from setText()
    int32_t           fTextLength;   // Length provided by user with setText(), which
                                     //  may be -1.
    UBool             fOwnsText;
};

static const int32_t REXP_MAGIC = 0x72657870; // "rexp" in ASCII

RegularExpression::RegularExpression() {
    fMagic        = REXP_MAGIC;
    fPat          = NULL;
    fPatRefCount  = NULL;
    fPatString    = NULL;
    fPatStringLen = 0;
    fMatcher      = NULL;
    fText         = NULL;
    fTextLength   = 0;
    fOwnsText     = FALSE;
}

RegularExpression::~RegularExpression() {
    delete fMatcher;
    fMatcher = NULL;
    if (fPatRefCount!=NULL && umtx_atomic_dec(fPatRefCount)==0) {
        delete fPat;
        uprv_free(fPatString);
        uprv_free((void *)fPatRefCount);
    }
    if (fOwnsText && fText!=NULL) {
        uprv_free((void *)fText);
    }
    fMagic = 0;
}

U_NAMESPACE_END

U_NAMESPACE_USE

//----------------------------------------------------------------------------------------
//
//   validateRE    Do boilerplate style checks on API function parameters.
//                 Return TRUE if they look OK.
//----------------------------------------------------------------------------------------
static UBool validateRE(const RegularExpression *re, UBool requiresText, UErrorCode *status) {
    if (U_FAILURE(*status)) {
        return FALSE;
    }
    if (re == NULL || re->fMagic != REXP_MAGIC) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return FALSE;
    }
    // !!! Not sure how to update this with the new UText backing, which is stored in re->fMatcher anyway
    if (requiresText && re->fText == NULL && !re->fOwnsText) {
        *status = U_REGEX_INVALID_STATE;
        return FALSE;
    }
    return TRUE;
}

//----------------------------------------------------------------------------------------
//
//    uregex_open
//
//----------------------------------------------------------------------------------------
U_CAPI URegularExpression *  U_EXPORT2
uregex_open( const  UChar          *pattern,
                    int32_t         patternLength,
                    uint32_t        flags,
                    UParseError    *pe,
                    UErrorCode     *status) {

    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (pattern == NULL || patternLength < -1 || patternLength == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }
    int32_t actualPatLen = patternLength;
    if (actualPatLen == -1) {
        actualPatLen = u_strlen(pattern);
    }

    RegularExpression  *re     = new RegularExpression;
    u_atomic_int32_t   *refC   = (u_atomic_int32_t *)uprv_malloc(sizeof(int32_t));
    UChar              *patBuf = (UChar *)uprv_malloc(sizeof(UChar)*(actualPatLen+1));
    if (re == NULL || refC == NULL || patBuf == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        delete re;
        uprv_free((void *)refC);
        uprv_free(patBuf);
        return NULL;
    }
    re->fPatRefCount = refC;
    *re->fPatRefCount = 1;

    //
    // Make a copy of the pattern string, so we can return it later if asked.
    //    For compiling the pattern, we will use a UText wrapper around
    //    this local copy, to avoid making even more copies.
    //
    re->fPatString    = patBuf;
    re->fPatStringLen = patternLength;
    u_memcpy(patBuf, pattern, actualPatLen);
    patBuf[actualPatLen] = 0;

    UText patText = UTEXT_INITIALIZER;
    utext_openUChars(&patText, patBuf, patternLength, status);

    //
    // Compile the pattern
    //
    if (pe != NULL) {
        re->fPat = RegexPattern::compile(&patText, flags, *pe, *status);
    } else {
        re->fPat = RegexPattern::compile(&patText, flags, *status);
    }
    utext_close(&patText);

    if (U_FAILURE(*status)) {
        goto ErrorExit;
    }

    //
    // Create the matcher object
    //
    re->fMatcher = re->fPat->matcher(*status);
    if (U_SUCCESS(*status)) {
        return (URegularExpression*)re;
    }

ErrorExit:
    delete re;
    return NULL;

}

//----------------------------------------------------------------------------------------
//
//    uregex_openUText
//
//----------------------------------------------------------------------------------------
U_CAPI URegularExpression *  U_EXPORT2
uregex_openUText(UText          *pattern,
                 uint32_t        flags,
                 UParseError    *pe,
                 UErrorCode     *status) {

    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (pattern == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    int64_t patternNativeLength = utext_nativeLength(pattern);

    if (patternNativeLength == 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    RegularExpression *re     = new RegularExpression;

    UErrorCode lengthStatus = U_ZERO_ERROR;
    int32_t pattern16Length = utext_extract(pattern, 0, patternNativeLength, NULL, 0, &lengthStatus);

    u_atomic_int32_t   *refC   = (u_atomic_int32_t *)uprv_malloc(sizeof(int32_t));
    UChar              *patBuf = (UChar *)uprv_malloc(sizeof(UChar)*(pattern16Length+1));
    if (re == NULL || refC == NULL || patBuf == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        delete re;
        uprv_free((void *)refC);
        uprv_free(patBuf);
        return NULL;
    }
    re->fPatRefCount = refC;
    *re->fPatRefCount = 1;

    //
    // Make a copy of the pattern string, so we can return it later if asked.
    //    For compiling the pattern, we will use a read-only UText wrapper
    //    around this local copy, to avoid making even more copies.
    //
    re->fPatString    = patBuf;
    re->fPatStringLen = pattern16Length;
    utext_extract(pattern, 0, patternNativeLength, patBuf, pattern16Length+1, status);

    UText patText = UTEXT_INITIALIZER;
    utext_openUChars(&patText, patBuf, pattern16Length, status);

    //
    // Compile the pattern
    //
    if (pe != NULL) {
        re->fPat = RegexPattern::compile(&patText, flags, *pe, *status);
    } else {
        re->fPat = RegexPattern::compile(&patText, flags, *status);
    }
    utext_close(&patText);

    if (U_FAILURE(*status)) {
        goto ErrorExit;
    }

    //
    // Create the matcher object
    //
    re->fMatcher = re->fPat->matcher(*status);
    if (U_SUCCESS(*status)) {
        return (URegularExpression*)re;
    }

ErrorExit:
    delete re;
    return NULL;

}

//----------------------------------------------------------------------------------------
//
//    uregex_close
//
//----------------------------------------------------------------------------------------
U_CAPI void  U_EXPORT2
uregex_close(URegularExpression  *re2) {
    RegularExpression *re = (RegularExpression*)re2;
    UErrorCode  status = U_ZERO_ERROR;
    if (validateRE(re, FALSE, &status) == FALSE) {
        return;
    }
    delete re;
}


//----------------------------------------------------------------------------------------
//
//    uregex_clone
//
//----------------------------------------------------------------------------------------
U_CAPI URegularExpression * U_EXPORT2
uregex_clone(const URegularExpression *source2, UErrorCode *status)  {
    RegularExpression *source = (RegularExpression*)source2;
    if (validateRE(source, FALSE, status) == FALSE) {
        return NULL;
    }

    RegularExpression *clone = new RegularExpression;
    if (clone == NULL) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    }

    clone->fMatcher = source->fPat->matcher(*status);
    if (U_FAILURE(*status)) {
        delete clone;
        return NULL;
    }

    clone->fPat          = source->fPat;
    clone->fPatRefCount  = source->fPatRefCount;
    clone->fPatString    = source->fPatString;
    clone->fPatStringLen = source->fPatStringLen;
    umtx_atomic_inc(source->fPatRefCount);
    // Note:  fText is not cloned.

    return (URegularExpression*)clone;
}




//------------------------------------------------------------------------------
//
//    uregex_pattern
//
//------------------------------------------------------------------------------
U_CAPI const UChar * U_EXPORT2
uregex_pattern(const  URegularExpression *regexp2,
                      int32_t            *patLength,
                      UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;

    if (validateRE(regexp, FALSE, status) == FALSE) {
        return NULL;
    }
    if (patLength != NULL) {
        *patLength = regexp->fPatStringLen;
    }
    return regexp->fPatString;
}


//------------------------------------------------------------------------------
//
//    uregex_patternUText
//
//------------------------------------------------------------------------------
U_CAPI UText * U_EXPORT2
uregex_patternUText(const URegularExpression *regexp2,
                          UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    return regexp->fPat->patternText(*status);
}


//------------------------------------------------------------------------------
//
//    uregex_flags
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_flags(const URegularExpression *regexp2, UErrorCode *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return 0;
    }
    int32_t flags = regexp->fPat->flags();
    return flags;
}


//------------------------------------------------------------------------------
//
//    uregex_setText
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setText(URegularExpression *regexp2,
               const UChar        *text,
               int32_t             textLength,
               UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return;
    }
    if (text == NULL || textLength < -1) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (regexp->fOwnsText && regexp->fText != NULL) {
        uprv_free((void *)regexp->fText);
    }

    regexp->fText       = text;
    regexp->fTextLength = textLength;
    regexp->fOwnsText   = FALSE;

    UText input = UTEXT_INITIALIZER;
    utext_openUChars(&input, text, textLength, status);
    regexp->fMatcher->reset(&input);
    utext_close(&input); // reset() made a shallow clone, so we don't need this copy
}


//------------------------------------------------------------------------------
//
//    uregex_setUText
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setUText(URegularExpression *regexp2,
                UText              *text,
                UErrorCode         *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return;
    }
    if (text == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    if (regexp->fOwnsText && regexp->fText != NULL) {
        uprv_free((void *)regexp->fText);
    }

    regexp->fText       = NULL; // only fill it in on request
    regexp->fTextLength = -1;
    regexp->fOwnsText   = TRUE;
    regexp->fMatcher->reset(text);
}



//------------------------------------------------------------------------------
//
//    uregex_getText
//
//------------------------------------------------------------------------------
U_CAPI const UChar * U_EXPORT2
uregex_getText(URegularExpression *regexp2,
               int32_t            *textLength,
               UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return NULL;
    }

    if (regexp->fText == NULL) {
        // need to fill in the text
        UText *inputText = regexp->fMatcher->inputText();
        int64_t inputNativeLength = utext_nativeLength(inputText);
        if (UTEXT_FULL_TEXT_IN_CHUNK(inputText, inputNativeLength)) {
            regexp->fText = inputText->chunkContents;
            regexp->fTextLength = (int32_t)inputNativeLength;
            regexp->fOwnsText = FALSE; // because the UText owns it
        } else {
            UErrorCode lengthStatus = U_ZERO_ERROR;
            regexp->fTextLength = utext_extract(inputText, 0, inputNativeLength, NULL, 0, &lengthStatus); // buffer overflow error
            UChar *inputChars = (UChar *)uprv_malloc(sizeof(UChar)*(regexp->fTextLength+1));

            utext_extract(inputText, 0, inputNativeLength, inputChars, regexp->fTextLength+1, status);
            regexp->fText = inputChars;
            regexp->fOwnsText = TRUE; // should already be set but just in case
        }
    }

    if (textLength != NULL) {
        *textLength = regexp->fTextLength;
    }
    return regexp->fText;
}


//------------------------------------------------------------------------------
//
//    uregex_getUText
//
//------------------------------------------------------------------------------
U_CAPI UText * U_EXPORT2
uregex_getUText(URegularExpression *regexp2,
                UText              *dest,
                UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return dest;
    }
    return regexp->fMatcher->getInput(dest, *status);
}


//------------------------------------------------------------------------------
//
//    uregex_refreshUText
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_refreshUText(URegularExpression *regexp2,
                    UText              *text,
                    UErrorCode         *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return;
    }
    regexp->fMatcher->refreshInputText(text, *status);
}


//------------------------------------------------------------------------------
//
//    uregex_matches
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_matches(URegularExpression *regexp2,
               int32_t            startIndex,
               UErrorCode        *status)  {
    return uregex_matches64( regexp2, (int64_t)startIndex, status);
}

U_CAPI UBool U_EXPORT2
uregex_matches64(URegularExpression *regexp2,
                 int64_t            startIndex,
                 UErrorCode        *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    UBool result = FALSE;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return result;
    }
    if (startIndex == -1) {
        result = regexp->fMatcher->matches(*status);
    } else {
        result = regexp->fMatcher->matches(startIndex, *status);
    }
    return result;
}


//------------------------------------------------------------------------------
//
//    uregex_lookingAt
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_lookingAt(URegularExpression *regexp2,
                 int32_t             startIndex,
                 UErrorCode         *status)  {
    return uregex_lookingAt64( regexp2, (int64_t)startIndex, status);
}

U_CAPI UBool U_EXPORT2
uregex_lookingAt64(URegularExpression *regexp2,
                   int64_t             startIndex,
                   UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    UBool result = FALSE;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return result;
    }
    if (startIndex == -1) {
        result = regexp->fMatcher->lookingAt(*status);
    } else {
        result = regexp->fMatcher->lookingAt(startIndex, *status);
    }
    return result;
}



//------------------------------------------------------------------------------
//
//    uregex_find
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_find(URegularExpression *regexp2,
            int32_t             startIndex,
            UErrorCode         *status)  {
    return uregex_find64( regexp2, (int64_t)startIndex, status);
}

U_CAPI UBool U_EXPORT2
uregex_find64(URegularExpression *regexp2,
              int64_t             startIndex,
              UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    UBool result = FALSE;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return result;
    }
    if (startIndex == -1) {
        regexp->fMatcher->resetPreserveRegion();
        result = regexp->fMatcher->find(*status);
    } else {
        result = regexp->fMatcher->find(startIndex, *status);
    }
    return result;
}


//------------------------------------------------------------------------------
//
//    uregex_findNext
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_findNext(URegularExpression *regexp2,
                UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return FALSE;
    }
    UBool result = regexp->fMatcher->find(*status);
    return result;
}

//------------------------------------------------------------------------------
//
//    uregex_groupCount
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_groupCount(URegularExpression *regexp2,
                  UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return 0;
    }
    int32_t  result = regexp->fMatcher->groupCount();
    return result;
}


//------------------------------------------------------------------------------
//
//    uregex_groupNumberFromName
//
//------------------------------------------------------------------------------
int32_t
uregex_groupNumberFromName(URegularExpression *regexp2,
                           const UChar        *groupName,
                           int32_t             nameLength,
                           UErrorCode          *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return 0;
    }
    int32_t  result = regexp->fPat->groupNumberFromName(UnicodeString(groupName, nameLength), *status);
    return result;
}

int32_t
uregex_groupNumberFromCName(URegularExpression *regexp2,
                            const char         *groupName,
                            int32_t             nameLength,
                            UErrorCode          *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return 0;
    }
    return regexp->fPat->groupNumberFromName(groupName, nameLength, *status);
}

//------------------------------------------------------------------------------
//
//    uregex_group
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_group(URegularExpression *regexp2,
             int32_t             groupNum,
             UChar              *dest,
             int32_t             destCapacity,
             UErrorCode          *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if (destCapacity < 0 || (destCapacity > 0 && dest == NULL)) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if (destCapacity == 0 || regexp->fText != NULL) {
        // If preflighting or if we already have the text as UChars,
        // this is a little cheaper than extracting from the UText

        //
        // Pick up the range of characters from the matcher
        //
        int32_t  startIx = regexp->fMatcher->start(groupNum, *status);
        int32_t  endIx   = regexp->fMatcher->end  (groupNum, *status);
        if (U_FAILURE(*status)) {
            return 0;
        }

        //
        // Trim length based on buffer capacity
        //
        int32_t fullLength = endIx - startIx;
        int32_t copyLength = fullLength;
        if (copyLength < destCapacity) {
            dest[copyLength] = 0;
        } else if (copyLength == destCapacity) {
            *status = U_STRING_NOT_TERMINATED_WARNING;
        } else {
            copyLength = destCapacity;
            *status = U_BUFFER_OVERFLOW_ERROR;
        }

        //
        // Copy capture group to user's buffer
        //
        if (copyLength > 0) {
            u_memcpy(dest, &regexp->fText[startIx], copyLength);
        }
        return fullLength;
    } else {
        int64_t  start = regexp->fMatcher->start64(groupNum, *status);
        int64_t  limit = regexp->fMatcher->end64(groupNum, *status);
        if (U_FAILURE(*status)) {
            return 0;
        }
        // Note edge cases:
        //   Group didn't match: start == end == -1. UText trims to 0, UText gives zero length result.
        //   Zero Length Match: start == end.
        int32_t length = utext_extract(regexp->fMatcher->inputText(), start, limit, dest, destCapacity, status);
        return length;
    }

}


//------------------------------------------------------------------------------
//
//    uregex_groupUText
//
//------------------------------------------------------------------------------
U_CAPI UText * U_EXPORT2
uregex_groupUText(URegularExpression *regexp2,
                  int32_t             groupNum,
                  UText              *dest,
                  int64_t            *groupLength,
                  UErrorCode         *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        UErrorCode emptyTextStatus = U_ZERO_ERROR;
        return (dest ? dest : utext_openUChars(NULL, NULL, 0, &emptyTextStatus));
    }

    return regexp->fMatcher->group(groupNum, dest, *groupLength, *status);
}

//------------------------------------------------------------------------------
//
//    uregex_start
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_start(URegularExpression *regexp2,
             int32_t             groupNum,
             UErrorCode          *status)  {
    return (int32_t)uregex_start64( regexp2, groupNum, status);
}

U_CAPI int64_t U_EXPORT2
uregex_start64(URegularExpression *regexp2,
               int32_t             groupNum,
               UErrorCode          *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    int32_t result = regexp->fMatcher->start(groupNum, *status);
    return result;
}

//------------------------------------------------------------------------------
//
//    uregex_end
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_end(URegularExpression   *regexp2,
           int32_t               groupNum,
           UErrorCode           *status)  {
    return (int32_t)uregex_end64( regexp2, groupNum, status);
}

U_CAPI int64_t U_EXPORT2
uregex_end64(URegularExpression   *regexp2,
             int32_t               groupNum,
             UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    int32_t result = regexp->fMatcher->end(groupNum, *status);
    return result;
}

//------------------------------------------------------------------------------
//
//    uregex_reset
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_reset(URegularExpression    *regexp2,
             int32_t               index,
             UErrorCode            *status)  {
    uregex_reset64( regexp2, (int64_t)index, status);
}

U_CAPI void U_EXPORT2
uregex_reset64(URegularExpression    *regexp2,
               int64_t               index,
               UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return;
    }
    regexp->fMatcher->reset(index, *status);
}


//------------------------------------------------------------------------------
//
//    uregex_setRegion
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setRegion(URegularExpression   *regexp2,
                 int32_t               regionStart,
                 int32_t               regionLimit,
                 UErrorCode           *status)  {
    uregex_setRegion64( regexp2, (int64_t)regionStart, (int64_t)regionLimit, status);
}

U_CAPI void U_EXPORT2
uregex_setRegion64(URegularExpression   *regexp2,
                   int64_t               regionStart,
                   int64_t               regionLimit,
                   UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return;
    }
    regexp->fMatcher->region(regionStart, regionLimit, *status);
}


//------------------------------------------------------------------------------
//
//    uregex_setRegionAndStart
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setRegionAndStart(URegularExpression   *regexp2,
                 int64_t               regionStart,
                 int64_t               regionLimit,
                 int64_t               startIndex,
                 UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return;
    }
    regexp->fMatcher->region(regionStart, regionLimit, startIndex, *status);
}

//------------------------------------------------------------------------------
//
//    uregex_regionStart
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_regionStart(const  URegularExpression   *regexp2,
                          UErrorCode           *status)  {
    return (int32_t)uregex_regionStart64(regexp2, status);
}

U_CAPI int64_t U_EXPORT2
uregex_regionStart64(const  URegularExpression   *regexp2,
                            UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    return regexp->fMatcher->regionStart();
}


//------------------------------------------------------------------------------
//
//    uregex_regionEnd
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_regionEnd(const  URegularExpression   *regexp2,
                        UErrorCode           *status)  {
    return (int32_t)uregex_regionEnd64(regexp2, status);
}

U_CAPI int64_t U_EXPORT2
uregex_regionEnd64(const  URegularExpression   *regexp2,
                          UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    return regexp->fMatcher->regionEnd();
}


//------------------------------------------------------------------------------
//
//    uregex_hasTransparentBounds
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_hasTransparentBounds(const  URegularExpression   *regexp2,
                                   UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return FALSE;
    }
    return regexp->fMatcher->hasTransparentBounds();
}


//------------------------------------------------------------------------------
//
//    uregex_useTransparentBounds
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_useTransparentBounds(URegularExpression    *regexp2,
                            UBool                  b,
                            UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return;
    }
    regexp->fMatcher->useTransparentBounds(b);
}


//------------------------------------------------------------------------------
//
//    uregex_hasAnchoringBounds
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_hasAnchoringBounds(const  URegularExpression   *regexp2,
                                 UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return FALSE;
    }
    return regexp->fMatcher->hasAnchoringBounds();
}


//------------------------------------------------------------------------------
//
//    uregex_useAnchoringBounds
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_useAnchoringBounds(URegularExpression    *regexp2,
                          UBool                  b,
                          UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status) == FALSE) {
        return;
    }
    regexp->fMatcher->useAnchoringBounds(b);
}


//------------------------------------------------------------------------------
//
//    uregex_hitEnd
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_hitEnd(const  URegularExpression   *regexp2,
                     UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return FALSE;
    }
    return regexp->fMatcher->hitEnd();
}


//------------------------------------------------------------------------------
//
//    uregex_requireEnd
//
//------------------------------------------------------------------------------
U_CAPI UBool U_EXPORT2
uregex_requireEnd(const  URegularExpression   *regexp2,
                         UErrorCode           *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return FALSE;
    }
    return regexp->fMatcher->requireEnd();
}


//------------------------------------------------------------------------------
//
//    uregex_setTimeLimit
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setTimeLimit(URegularExpression   *regexp2,
                    int32_t               limit,
                    UErrorCode           *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status)) {
        regexp->fMatcher->setTimeLimit(limit, *status);
    }
}



//------------------------------------------------------------------------------
//
//    uregex_getTimeLimit
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_getTimeLimit(const  URegularExpression   *regexp2,
                           UErrorCode           *status) {
    int32_t retVal = 0;
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status)) {
        retVal = regexp->fMatcher->getTimeLimit();
    }
    return retVal;
}



//------------------------------------------------------------------------------
//
//    uregex_setStackLimit
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setStackLimit(URegularExpression   *regexp2,
                     int32_t               limit,
                     UErrorCode           *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status)) {
        regexp->fMatcher->setStackLimit(limit, *status);
    }
}



//------------------------------------------------------------------------------
//
//    uregex_getStackLimit
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_getStackLimit(const  URegularExpression   *regexp2,
                            UErrorCode           *status) {
    int32_t retVal = 0;
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status)) {
        retVal = regexp->fMatcher->getStackLimit();
    }
    return retVal;
}


//------------------------------------------------------------------------------
//
//    uregex_setMatchCallback
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setMatchCallback(URegularExpression      *regexp2,
                        URegexMatchCallback     *callback,
                        const void              *context,
                        UErrorCode              *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status)) {
        regexp->fMatcher->setMatchCallback(callback, context, *status);
    }
}


//------------------------------------------------------------------------------
//
//    uregex_getMatchCallback
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_getMatchCallback(const URegularExpression    *regexp2,
                        URegexMatchCallback        **callback,
                        const void                 **context,
                        UErrorCode                  *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
     if (validateRE(regexp, FALSE, status)) {
         regexp->fMatcher->getMatchCallback(*callback, *context, *status);
     }
}


//------------------------------------------------------------------------------
//
//    uregex_setMatchProgressCallback
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_setFindProgressCallback(URegularExpression              *regexp2,
                                URegexFindProgressCallback      *callback,
                                const void                      *context,
                                UErrorCode                      *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, FALSE, status)) {
        regexp->fMatcher->setFindProgressCallback(callback, context, *status);
    }
}


//------------------------------------------------------------------------------
//
//    uregex_getMatchCallback
//
//------------------------------------------------------------------------------
U_CAPI void U_EXPORT2
uregex_getFindProgressCallback(const URegularExpression          *regexp2,
                                URegexFindProgressCallback        **callback,
                                const void                        **context,
                                UErrorCode                        *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
     if (validateRE(regexp, FALSE, status)) {
         regexp->fMatcher->getFindProgressCallback(*callback, *context, *status);
     }
}


//------------------------------------------------------------------------------
//
//    uregex_replaceAll
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_replaceAll(URegularExpression    *regexp2,
                  const UChar           *replacementText,
                  int32_t                replacementLength,
                  UChar                 *destBuf,
                  int32_t                destCapacity,
                  UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if (replacementText == NULL || replacementLength < -1 ||
        (destBuf == NULL && destCapacity > 0) ||
        destCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    int32_t   len = 0;

    uregex_reset(regexp2, 0, status);

    // Note: Seperate error code variables for findNext() and appendReplacement()
    //       are used so that destination buffer overflow errors
    //       in appendReplacement won't stop findNext() from working.
    //       appendReplacement() and appendTail() special case incoming buffer
    //       overflow errors, continuing to return the correct length.
    UErrorCode  findStatus = *status;
    while (uregex_findNext(regexp2, &findStatus)) {
        len += uregex_appendReplacement(regexp2, replacementText, replacementLength,
                                        &destBuf, &destCapacity, status);
    }
    len += uregex_appendTail(regexp2, &destBuf, &destCapacity, status);

    if (U_FAILURE(findStatus)) {
        // If anything went wrong with the findNext(), make that error trump
        //   whatever may have happened with the append() operations.
        //   Errors in findNext() are not expected.
        *status = findStatus;
    }

    return len;
}


//------------------------------------------------------------------------------
//
//    uregex_replaceAllUText
//
//------------------------------------------------------------------------------
U_CAPI UText * U_EXPORT2
uregex_replaceAllUText(URegularExpression    *regexp2,
                       UText                 *replacementText,
                       UText                 *dest,
                       UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if (replacementText == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    dest = regexp->fMatcher->replaceAll(replacementText, dest, *status);
    return dest;
}


//------------------------------------------------------------------------------
//
//    uregex_replaceFirst
//
//------------------------------------------------------------------------------
U_CAPI int32_t U_EXPORT2
uregex_replaceFirst(URegularExpression  *regexp2,
                    const UChar         *replacementText,
                    int32_t              replacementLength,
                    UChar               *destBuf,
                    int32_t              destCapacity,
                    UErrorCode          *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if (replacementText == NULL || replacementLength < -1 ||
        (destBuf == NULL && destCapacity > 0) ||
        destCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    int32_t   len = 0;
    UBool     findSucceeded;
    uregex_reset(regexp2, 0, status);
    findSucceeded = uregex_find(regexp2, 0, status);
    if (findSucceeded) {
        len = uregex_appendReplacement(regexp2, replacementText, replacementLength,
                                       &destBuf, &destCapacity, status);
    }
    len += uregex_appendTail(regexp2, &destBuf, &destCapacity, status);

    return len;
}


//------------------------------------------------------------------------------
//
//    uregex_replaceFirstUText
//
//------------------------------------------------------------------------------
U_CAPI UText * U_EXPORT2
uregex_replaceFirstUText(URegularExpression  *regexp2,
                         UText                 *replacementText,
                         UText                 *dest,
                         UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if (replacementText == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    dest = regexp->fMatcher->replaceFirst(replacementText, dest, *status);
    return dest;
}


//------------------------------------------------------------------------------
//
//    uregex_appendReplacement
//
//------------------------------------------------------------------------------

U_NAMESPACE_BEGIN
//
//  Dummy class, because these functions need to be friends of class RegexMatcher,
//               and stand-alone C functions don't work as friends
//
class RegexCImpl {
 public:
   inline static  int32_t appendReplacement(RegularExpression    *regexp,
                      const UChar           *replacementText,
                      int32_t                replacementLength,
                      UChar                **destBuf,
                      int32_t               *destCapacity,
                      UErrorCode            *status);

   inline static int32_t appendTail(RegularExpression    *regexp,
        UChar                **destBuf,
        int32_t               *destCapacity,
        UErrorCode            *status);

    inline static int32_t split(RegularExpression    *regexp,
        UChar                 *destBuf,
        int32_t                destCapacity,
        int32_t               *requiredCapacity,
        UChar                 *destFields[],
        int32_t                destFieldsCapacity,
        UErrorCode            *status);
};

U_NAMESPACE_END



static const UChar BACKSLASH  = 0x5c;
static const UChar DOLLARSIGN = 0x24;
static const UChar LEFTBRACKET = 0x7b;
static const UChar RIGHTBRACKET = 0x7d;

//
//  Move a character to an output buffer, with bounds checking on the index.
//      Index advances even if capacity is exceeded, for preflight size computations.
//      This little sequence is used a LOT.
//
static inline void appendToBuf(UChar c, int32_t *idx, UChar *buf, int32_t bufCapacity) {
    if (*idx < bufCapacity) {
        buf[*idx] = c;
    }
    (*idx)++;
}


//
//  appendReplacement, the actual implementation.
//
int32_t RegexCImpl::appendReplacement(RegularExpression    *regexp,
                                      const UChar           *replacementText,
                                      int32_t                replacementLength,
                                      UChar                **destBuf,
                                      int32_t               *destCapacity,
                                      UErrorCode            *status)  {

    // If we come in with a buffer overflow error, don't suppress the operation.
    //  A series of appendReplacements, appendTail need to correctly preflight
    //  the buffer size when an overflow happens somewhere in the middle.
    UBool pendingBufferOverflow = FALSE;
    if (*status == U_BUFFER_OVERFLOW_ERROR && destCapacity != NULL && *destCapacity == 0) {
        pendingBufferOverflow = TRUE;
        *status = U_ZERO_ERROR;
    }

    //
    // Validate all paramters
    //
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if (replacementText == NULL || replacementLength < -1 ||
        destCapacity == NULL || destBuf == NULL ||
        (*destBuf == NULL && *destCapacity > 0) ||
        *destCapacity < 0) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    RegexMatcher *m = regexp->fMatcher;
    if (m->fMatch == FALSE) {
        *status = U_REGEX_INVALID_STATE;
        return 0;
    }

    UChar    *dest             = *destBuf;
    int32_t   capacity         = *destCapacity;
    int32_t   destIdx          =  0;
    int32_t   i;

    // If it wasn't supplied by the caller,  get the length of the replacement text.
    //   TODO:  slightly smarter logic in the copy loop could watch for the NUL on
    //          the fly and avoid this step.
    if (replacementLength == -1) {
        replacementLength = u_strlen(replacementText);
    }

    // Copy input string from the end of previous match to start of current match
    if (regexp->fText != NULL) {
        int32_t matchStart;
        int32_t lastMatchEnd;
        if (UTEXT_USES_U16(m->fInputText)) {
            lastMatchEnd = (int32_t)m->fLastMatchEnd;
            matchStart = (int32_t)m->fMatchStart;
        } else {
            // !!!: Would like a better way to do this!
            UErrorCode tempStatus = U_ZERO_ERROR;
            lastMatchEnd = utext_extract(m->fInputText, 0, m->fLastMatchEnd, NULL, 0, &tempStatus);
            tempStatus = U_ZERO_ERROR;
            matchStart = lastMatchEnd + utext_extract(m->fInputText, m->fLastMatchEnd, m->fMatchStart, NULL, 0, &tempStatus);
        }
        for (i=lastMatchEnd; i<matchStart; i++) {
            appendToBuf(regexp->fText[i], &destIdx, dest, capacity);
        }
    } else {
        UErrorCode possibleOverflowError = U_ZERO_ERROR; // ignore
        destIdx += utext_extract(m->fInputText, m->fLastMatchEnd, m->fMatchStart,
                                 dest==NULL?NULL:&dest[destIdx], REMAINING_CAPACITY(destIdx, capacity),
                                 &possibleOverflowError);
    }
    U_ASSERT(destIdx >= 0);

    // scan the replacement text, looking for substitutions ($n) and \escapes.
    int32_t  replIdx = 0;
    while (replIdx < replacementLength && U_SUCCESS(*status)) {
        UChar  c = replacementText[replIdx];
        replIdx++;
        if (c != DOLLARSIGN && c != BACKSLASH) {
            // Common case, no substitution, no escaping,
            //  just copy the char to the dest buf.
            appendToBuf(c, &destIdx, dest, capacity);
            continue;
        }

        if (c == BACKSLASH) {
            // Backslash Escape.  Copy the following char out without further checks.
            //                    Note:  Surrogate pairs don't need any special handling
            //                           The second half wont be a '$' or a '\', and
            //                           will move to the dest normally on the next
            //                           loop iteration.
            if (replIdx >= replacementLength) {
                break;
            }
            c = replacementText[replIdx];

            if (c==0x55/*U*/ || c==0x75/*u*/) {
                // We have a \udddd or \Udddddddd escape sequence.
                UChar32 escapedChar =
                    u_unescapeAt(uregex_ucstr_unescape_charAt,
                       &replIdx,                   // Index is updated by unescapeAt
                       replacementLength,          // Length of replacement text
                       (void *)replacementText);

                if (escapedChar != (UChar32)0xFFFFFFFF) {
                    if (escapedChar <= 0xffff) {
                        appendToBuf((UChar)escapedChar, &destIdx, dest, capacity);
                    } else {
                        appendToBuf(U16_LEAD(escapedChar), &destIdx, dest, capacity);
                        appendToBuf(U16_TRAIL(escapedChar), &destIdx, dest, capacity);
                    }
                    continue;
                }
                // Note:  if the \u escape was invalid, just fall through and
                //        treat it as a plain \<anything> escape.
            }

            // Plain backslash escape.  Just put out the escaped character.
            appendToBuf(c, &destIdx, dest, capacity);

            replIdx++;
            continue;
        }

        // We've got a $.  Pick up the following capture group name or number.
        // For numbers, consume only digits that produce a valid capture group for the pattern.

        int32_t groupNum  = 0;
        U_ASSERT(c == DOLLARSIGN);
        UChar32 c32 = -1;
        if (replIdx < replacementLength) {
            U16_GET(replacementText, 0, replIdx, replacementLength, c32);
        }
        if (u_isdigit(c32)) {
            int32_t numDigits = 0;
            int32_t numCaptureGroups = m->fPattern->fGroupMap->size();
            for (;;) {
                if (replIdx >= replacementLength) {
                    break;
                }
                U16_GET(replacementText, 0, replIdx, replacementLength, c32);
                if (u_isdigit(c32) == FALSE) {
                    break;
                }

                int32_t digitVal = u_charDigitValue(c32);
                if (groupNum * 10 + digitVal <= numCaptureGroups) {
                    groupNum = groupNum * 10 + digitVal;
                    U16_FWD_1(replacementText, replIdx, replacementLength);
                    numDigits++;
                } else {
                    if (numDigits == 0) {
                        *status = U_INDEX_OUTOFBOUNDS_ERROR;
                    }
                    break;
                }
            }
        } else if (c32 == LEFTBRACKET) {
            // Scan for Named Capture Group, ${name}.
            UnicodeString groupName;
            U16_FWD_1(replacementText, replIdx, replacementLength);
            while (U_SUCCESS(*status) && c32 != RIGHTBRACKET) {
                if (replIdx >= replacementLength) {
                    *status = U_REGEX_INVALID_CAPTURE_GROUP_NAME;
                    break;
                }
                U16_NEXT(replacementText, replIdx, replacementLength, c32);
                if ((c32 >= 0x41 && c32 <= 0x5a) ||           // A..Z
                        (c32 >= 0x61 && c32 <= 0x7a) ||       // a..z
                        (c32 >= 0x31 && c32 <= 0x39)) {       // 0..9
                    groupName.append(c32);
                } else if (c32 == RIGHTBRACKET) {
                    groupNum = uhash_geti(regexp->fPat->fNamedCaptureMap, &groupName);
                    if (groupNum == 0) {
                        // Name not defined by pattern.
                        *status = U_REGEX_INVALID_CAPTURE_GROUP_NAME;
                    }
                } else {
                    // Character was something other than a name char or a closing '}'
                    *status = U_REGEX_INVALID_CAPTURE_GROUP_NAME;
                }
            }
        } else {
            // $ not followed by {name} or digits.
            *status = U_REGEX_INVALID_CAPTURE_GROUP_NAME;
        }


        // Finally, append the capture group data to the destination.
        if (U_SUCCESS(*status)) {
            destIdx += uregex_group((URegularExpression*)regexp, groupNum,
                                    dest==NULL?NULL:&dest[destIdx], REMAINING_CAPACITY(destIdx, capacity), status);
            if (*status == U_BUFFER_OVERFLOW_ERROR) {
                // Ignore buffer overflow when extracting the group.  We need to
                //   continue on to get full size of the untruncated result.  We will
                //   raise our own buffer overflow error at the end.
                *status = U_ZERO_ERROR;
            }
        }

        if (U_FAILURE(*status)) {
            // bad group number or name.
            break;
        }
    }

    //
    //  Nul Terminate the dest buffer if possible.
    //  Set the appropriate buffer overflow or not terminated error, if needed.
    //
    if (destIdx < capacity) {
        dest[destIdx] = 0;
    } else if (U_SUCCESS(*status)) {
        if (destIdx == *destCapacity) {
            *status = U_STRING_NOT_TERMINATED_WARNING;
        } else {
            *status = U_BUFFER_OVERFLOW_ERROR;
        }
    }

    //
    // Return an updated dest buffer and capacity to the caller.
    //
    if (destIdx > 0 &&  *destCapacity > 0) {
        if (destIdx < capacity) {
            *destBuf      += destIdx;
            *destCapacity -= destIdx;
        } else {
            *destBuf      += capacity;
            *destCapacity =  0;
        }
    }

    // If we came in with a buffer overflow, make sure we go out with one also.
    //   (A zero length match right at the end of the previous match could
    //    make this function succeed even though a previous call had overflowed the buf)
    if (pendingBufferOverflow && U_SUCCESS(*status)) {
        *status = U_BUFFER_OVERFLOW_ERROR;
    }

    return destIdx;
}

//
//   appendReplacement   the actual API function,
//
U_CAPI int32_t U_EXPORT2
uregex_appendReplacement(URegularExpression    *regexp2,
                         const UChar           *replacementText,
                         int32_t                replacementLength,
                         UChar                **destBuf,
                         int32_t               *destCapacity,
                         UErrorCode            *status) {

    RegularExpression *regexp = (RegularExpression*)regexp2;
    return RegexCImpl::appendReplacement(
        regexp, replacementText, replacementLength,destBuf, destCapacity, status);
}

//
//   uregex_appendReplacementUText...can just use the normal C++ method
//
U_CAPI void U_EXPORT2
uregex_appendReplacementUText(URegularExpression    *regexp2,
                              UText                 *replText,
                              UText                 *dest,
                              UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    regexp->fMatcher->appendReplacement(dest, replText, *status);
}


//------------------------------------------------------------------------------
//
//    uregex_appendTail
//
//------------------------------------------------------------------------------
int32_t RegexCImpl::appendTail(RegularExpression    *regexp,
                               UChar                **destBuf,
                               int32_t               *destCapacity,
                               UErrorCode            *status)
{

    // If we come in with a buffer overflow error, don't suppress the operation.
    //  A series of appendReplacements, appendTail need to correctly preflight
    //  the buffer size when an overflow happens somewhere in the middle.
    UBool pendingBufferOverflow = FALSE;
    if (*status == U_BUFFER_OVERFLOW_ERROR && destCapacity != NULL && *destCapacity == 0) {
        pendingBufferOverflow = TRUE;
        *status = U_ZERO_ERROR;
    }

    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }

    if (destCapacity == NULL || destBuf == NULL ||
        (*destBuf == NULL && *destCapacity > 0) ||
        *destCapacity < 0)
    {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    RegexMatcher *m = regexp->fMatcher;

    int32_t  destIdx     = 0;
    int32_t  destCap     = *destCapacity;
    UChar    *dest       = *destBuf;

    if (regexp->fText != NULL) {
        int32_t srcIdx;
        int64_t nativeIdx = (m->fMatch ? m->fMatchEnd : m->fLastMatchEnd);
        if (nativeIdx == -1) {
            srcIdx = 0;
        } else if (UTEXT_USES_U16(m->fInputText)) {
            srcIdx = (int32_t)nativeIdx;
        } else {
            UErrorCode newStatus = U_ZERO_ERROR;
            srcIdx = utext_extract(m->fInputText, 0, nativeIdx, NULL, 0, &newStatus);
        }

        for (;;) {
            U_ASSERT(destIdx >= 0);

            if (srcIdx == regexp->fTextLength) {
                break;
            }
            UChar c = regexp->fText[srcIdx];
            if (c == 0 && regexp->fTextLength == -1) {
                regexp->fTextLength = srcIdx;
                break;
            }

            if (destIdx < destCap) {
                dest[destIdx] = c;
            } else {
                // We've overflowed the dest buffer.
                //  If the total input string length is known, we can
                //    compute the total buffer size needed without scanning through the string.
                if (regexp->fTextLength > 0) {
                    destIdx += (regexp->fTextLength - srcIdx);
                    break;
                }
            }
            srcIdx++;
            destIdx++;
        }
    } else {
        int64_t  srcIdx;
        if (m->fMatch) {
            // The most recent call to find() succeeded.
            srcIdx = m->fMatchEnd;
        } else {
            // The last call to find() on this matcher failed().
            //   Look back to the end of the last find() that succeeded for src index.
            srcIdx = m->fLastMatchEnd;
            if (srcIdx == -1)  {
                // There has been no successful match with this matcher.
                //   We want to copy the whole string.
                srcIdx = 0;
            }
        }

        destIdx = utext_extract(m->fInputText, srcIdx, m->fInputLength, dest, destCap, status);
    }

    //
    //  NUL terminate the output string, if possible, otherwise issue the
    //   appropriate error or warning.
    //
    if (destIdx < destCap) {
        dest[destIdx] = 0;
    } else  if (destIdx == destCap) {
        *status = U_STRING_NOT_TERMINATED_WARNING;
    } else {
        *status = U_BUFFER_OVERFLOW_ERROR;
    }

    //
    // Update the user's buffer ptr and capacity vars to reflect the
    //   amount used.
    //
    if (destIdx < destCap) {
        *destBuf      += destIdx;
        *destCapacity -= destIdx;
    } else if (*destBuf != NULL) {
        *destBuf      += destCap;
        *destCapacity  = 0;
    }

    if (pendingBufferOverflow && U_SUCCESS(*status)) {
        *status = U_BUFFER_OVERFLOW_ERROR;
    }

    return destIdx;
}


//
//   appendTail   the actual API function
//
U_CAPI int32_t U_EXPORT2
uregex_appendTail(URegularExpression    *regexp2,
                  UChar                **destBuf,
                  int32_t               *destCapacity,
                  UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    return RegexCImpl::appendTail(regexp, destBuf, destCapacity, status);
}


//
//   uregex_appendTailUText...can just use the normal C++ method
//
U_CAPI UText * U_EXPORT2
uregex_appendTailUText(URegularExpression    *regexp2,
                       UText                 *dest,
                       UErrorCode            *status)  {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    return regexp->fMatcher->appendTail(dest, *status);
}


//------------------------------------------------------------------------------
//
//    copyString     Internal utility to copy a string to an output buffer,
//                   while managing buffer overflow and preflight size
//                   computation.  NUL termination is added to destination,
//                   and the NUL is counted in the output size.
//
//------------------------------------------------------------------------------
#if 0
static void copyString(UChar        *destBuffer,    //  Destination buffer.
                       int32_t       destCapacity,  //  Total capacity of dest buffer
                       int32_t      *destIndex,     //  Index into dest buffer.  Updated on return.
                                                    //    Update not clipped to destCapacity.
                       const UChar  *srcPtr,        //  Pointer to source string
                       int32_t       srcLen)        //  Source string len.
{
    int32_t  si;
    int32_t  di = *destIndex;
    UChar    c;

    for (si=0; si<srcLen;  si++) {
        c = srcPtr[si];
        if (di < destCapacity) {
            destBuffer[di] = c;
            di++;
        } else {
            di += srcLen - si;
            break;
        }
    }
    if (di<destCapacity) {
        destBuffer[di] = 0;
    }
    di++;
    *destIndex = di;
}
#endif

//------------------------------------------------------------------------------
//
//    uregex_split
//
//------------------------------------------------------------------------------
int32_t RegexCImpl::split(RegularExpression     *regexp,
                          UChar                 *destBuf,
                          int32_t                destCapacity,
                          int32_t               *requiredCapacity,
                          UChar                 *destFields[],
                          int32_t                destFieldsCapacity,
                          UErrorCode            *status) {
    //
    // Reset for the input text
    //
    regexp->fMatcher->reset();
    UText *inputText = regexp->fMatcher->fInputText;
    int64_t   nextOutputStringStart = 0;
    int64_t   inputLen = regexp->fMatcher->fInputLength;
    if (inputLen == 0) {
        return 0;
    }

    //
    // Loop through the input text, searching for the delimiter pattern
    //
    int32_t   i;             // Index of the field being processed.
    int32_t   destIdx = 0;   // Next available position in destBuf;
    int32_t   numCaptureGroups = regexp->fMatcher->groupCount();
    UErrorCode  tStatus = U_ZERO_ERROR;   // Want to ignore any buffer overflow errors so that the strings are still counted
    for (i=0; ; i++) {
        if (i>=destFieldsCapacity-1) {
            // There are one or zero output strings left.
            // Fill the last output string with whatever is left from the input, then exit the loop.
            //  ( i will be == destFieldsCapacity if we filled the output array while processing
            //    capture groups of the delimiter expression, in which case we will discard the
            //    last capture group saved in favor of the unprocessed remainder of the
            //    input string.)
            if (inputLen > nextOutputStringStart) {
                if (i != destFieldsCapacity-1) {
                    // No fields are left.  Recycle the last one for holding the trailing part of
                    //   the input string.
                    i = destFieldsCapacity-1;
                    destIdx = (int32_t)(destFields[i] - destFields[0]);
                }

                destFields[i] = &destBuf[destIdx];
                destIdx += 1 + utext_extract(inputText, nextOutputStringStart, inputLen,
                                             &destBuf[destIdx], REMAINING_CAPACITY(destIdx, destCapacity), status);
            }
            break;
        }

        if (regexp->fMatcher->find()) {
            // We found another delimiter.  Move everything from where we started looking
            //  up until the start of the delimiter into the next output string.
            destFields[i] = &destBuf[destIdx];

            destIdx += 1 + utext_extract(inputText, nextOutputStringStart, regexp->fMatcher->fMatchStart,
                                         &destBuf[destIdx], REMAINING_CAPACITY(destIdx, destCapacity), &tStatus);
            if (tStatus == U_BUFFER_OVERFLOW_ERROR) {
                tStatus = U_ZERO_ERROR;
            } else {
                *status = tStatus;
            }
            nextOutputStringStart = regexp->fMatcher->fMatchEnd;

            // If the delimiter pattern has capturing parentheses, the captured
            //  text goes out into the next n destination strings.
            int32_t groupNum;
            for (groupNum=1; groupNum<=numCaptureGroups; groupNum++) {
                // If we've run out of output string slots, bail out.
                if (i==destFieldsCapacity-1) {
                    break;
                }
                i++;

                // Set up to extract the capture group contents into the dest buffer.
                destFields[i] = &destBuf[destIdx];
                tStatus = U_ZERO_ERROR;
                int32_t t = uregex_group((URegularExpression*)regexp,
                                         groupNum,
                                         destFields[i],
                                         REMAINING_CAPACITY(destIdx, destCapacity),
                                         &tStatus);
                destIdx += t + 1;    // Record the space used in the output string buffer.
                                     //  +1 for the NUL that terminates the string.
                if (tStatus == U_BUFFER_OVERFLOW_ERROR) {
                    tStatus = U_ZERO_ERROR;
                } else {
                    *status = tStatus;
                }
            }

            if (nextOutputStringStart == inputLen) {
                // The delimiter was at the end of the string.
                // Output an empty string, and then we are done.
                if (destIdx < destCapacity) {
                    destBuf[destIdx] = 0;
                }
                if (i < destFieldsCapacity-1) {
                   ++i;
                }
                if (destIdx < destCapacity) {
                    destFields[i] = destBuf + destIdx;
                }
                ++destIdx;
                break;
            }

        }
        else
        {
            // We ran off the end of the input while looking for the next delimiter.
            // All the remaining text goes into the current output string.
            destFields[i] = &destBuf[destIdx];
            destIdx += 1 + utext_extract(inputText, nextOutputStringStart, inputLen,
                                         &destBuf[destIdx], REMAINING_CAPACITY(destIdx, destCapacity), status);
            break;
        }
    }

    // Zero out any unused portion of the destFields array
    int j;
    for (j=i+1; j<destFieldsCapacity; j++) {
        destFields[j] = NULL;
    }

    if (requiredCapacity != NULL) {
        *requiredCapacity = destIdx;
    }
    if (destIdx > destCapacity) {
        *status = U_BUFFER_OVERFLOW_ERROR;
    }
    return i+1;
}

//
//   uregex_split   The actual API function
//
U_CAPI int32_t U_EXPORT2
uregex_split(URegularExpression      *regexp2,
             UChar                   *destBuf,
             int32_t                  destCapacity,
             int32_t                 *requiredCapacity,
             UChar                   *destFields[],
             int32_t                  destFieldsCapacity,
             UErrorCode              *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    if (validateRE(regexp, TRUE, status) == FALSE) {
        return 0;
    }
    if ((destBuf == NULL && destCapacity > 0) ||
        destCapacity < 0 ||
        destFields == NULL ||
        destFieldsCapacity < 1 ) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    return RegexCImpl::split(regexp, destBuf, destCapacity, requiredCapacity, destFields, destFieldsCapacity, status);
}


//
//   uregex_splitUText...can just use the normal C++ method
//
U_CAPI int32_t U_EXPORT2
uregex_splitUText(URegularExpression    *regexp2,
                  UText                 *destFields[],
                  int32_t                destFieldsCapacity,
                  UErrorCode            *status) {
    RegularExpression *regexp = (RegularExpression*)regexp2;
    return regexp->fMatcher->split(regexp->fMatcher->inputText(), destFields, destFieldsCapacity, *status);
}


#endif   // !UCONFIG_NO_REGULAR_EXPRESSIONS
