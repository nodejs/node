// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ustr_titlecase_brkiter.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011may30
*   created by: Markus W. Scherer
*
*   Titlecasing functions that are based on BreakIterator
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/brkiter.h"
#include "unicode/casemap.h"
#include "unicode/localpointer.h"
#include "unicode/ubrk.h"
#include "unicode/ucasemap.h"
#include "cmemory.h"
#include "ucase.h"
#include "ucasemap_imp.h"

U_NAMESPACE_USE

/* functions available in the common library (for unistr_case.cpp) */

/* public API functions */

U_CAPI int32_t U_EXPORT2
u_strToTitle(UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             UBreakIterator *titleIter,
             const char *locale,
             UErrorCode *pErrorCode) {
    LocalPointer<BreakIterator> ownedIter;
    BreakIterator *iter;
    if(titleIter!=NULL) {
        iter=reinterpret_cast<BreakIterator *>(titleIter);
    } else {
        iter=BreakIterator::createWordInstance(Locale(locale), *pErrorCode);
        ownedIter.adoptInstead(iter);
    }
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    UnicodeString s(srcLength<0, src, srcLength);
    iter->setText(s);
    return ustrcase_mapWithOverlap(
        ustrcase_getCaseLocale(locale), 0, iter,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToTitle, *pErrorCode);
}

U_NAMESPACE_BEGIN

int32_t CaseMap::toTitle(
        const char *locale, uint32_t options, BreakIterator *iter,
        const UChar *src, int32_t srcLength,
        UChar *dest, int32_t destCapacity, Edits *edits,
        UErrorCode &errorCode) {
    LocalPointer<BreakIterator> ownedIter;
    if(iter==NULL) {
        iter=BreakIterator::createWordInstance(Locale(locale), errorCode);
        ownedIter.adoptInstead(iter);
    }
    if(U_FAILURE(errorCode)) {
        return 0;
    }
    UnicodeString s(srcLength<0, src, srcLength);
    iter->setText(s);
    return ustrcase_map(
        ustrcase_getCaseLocale(locale), options, iter,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToTitle, edits, errorCode);
}

U_NAMESPACE_END

U_CAPI int32_t U_EXPORT2
ucasemap_toTitle(UCaseMap *csm,
                 UChar *dest, int32_t destCapacity,
                 const UChar *src, int32_t srcLength,
                 UErrorCode *pErrorCode) {
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if (csm->iter == NULL) {
        csm->iter = BreakIterator::createWordInstance(Locale(csm->locale), *pErrorCode);
    }
    if (U_FAILURE(*pErrorCode)) {
        return 0;
    }
    UnicodeString s(srcLength<0, src, srcLength);
    csm->iter->setText(s);
    return ustrcase_map(
        csm->caseLocale, csm->options, csm->iter,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalToTitle, NULL, *pErrorCode);
}

#endif  // !UCONFIG_NO_BREAK_ITERATION
