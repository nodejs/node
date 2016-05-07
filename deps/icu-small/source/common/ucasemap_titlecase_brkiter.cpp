/*
*******************************************************************************
*   Copyright (C) 2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ucasemap_titlecase_brkiter.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2011jun02
*   created by: Markus W. Scherer
*
*   Titlecasing functions that are based on BreakIterator
*   were moved here to break dependency cycles among parts of the common library.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/brkiter.h"
#include "unicode/ubrk.h"
#include "unicode/ucasemap.h"
#include "cmemory.h"
#include "ucase.h"
#include "ustr_imp.h"

U_NAMESPACE_USE

U_CAPI const UBreakIterator * U_EXPORT2
ucasemap_getBreakIterator(const UCaseMap *csm) {
    return csm->iter;
}

U_CAPI void U_EXPORT2
ucasemap_setBreakIterator(UCaseMap *csm, UBreakIterator *iterToAdopt, UErrorCode * /*pErrorCode*/) {
    // Do not call ubrk_close() so that we do not depend on all of the BreakIterator code.
    delete reinterpret_cast<BreakIterator *>(csm->iter);
    csm->iter=iterToAdopt;
}

U_CAPI int32_t U_EXPORT2
ucasemap_utf8ToTitle(UCaseMap *csm,
                     char *dest, int32_t destCapacity,
                     const char *src, int32_t srcLength,
                     UErrorCode *pErrorCode) {
    UText utext=UTEXT_INITIALIZER;
    utext_openUTF8(&utext, (const char *)src, srcLength, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(csm->iter==NULL) {
        csm->iter=ubrk_open(UBRK_WORD, csm->locale,
                            NULL, 0,
                            pErrorCode);
    }
    ubrk_setUText(csm->iter, &utext, pErrorCode);
    int32_t length=ucasemap_mapUTF8(csm,
                   (uint8_t *)dest, destCapacity,
                   (const uint8_t *)src, srcLength,
                   ucasemap_internalUTF8ToTitle, pErrorCode);
    utext_close(&utext);
    return length;
}

#endif  // !UCONFIG_NO_BREAK_ITERATION
