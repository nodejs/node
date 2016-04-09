/*
*******************************************************************************
*
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucasemap.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2005may06
*   created by: Markus W. Scherer
*
*   Case mapping service object and functions using it.
*/

#include "unicode/utypes.h"
#include "unicode/brkiter.h"
#include "unicode/ubrk.h"
#include "unicode/uloc.h"
#include "unicode/ustring.h"
#include "unicode/ucasemap.h"
#if !UCONFIG_NO_BREAK_ITERATION
#include "unicode/utext.h"
#endif
#include "unicode/utf.h"
#include "unicode/utf8.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "cstring.h"
#include "ucase.h"
#include "ustr_imp.h"

U_NAMESPACE_USE

/* UCaseMap service object -------------------------------------------------- */

U_CAPI UCaseMap * U_EXPORT2
ucasemap_open(const char *locale, uint32_t options, UErrorCode *pErrorCode) {
    UCaseMap *csm;

    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }

    csm=(UCaseMap *)uprv_malloc(sizeof(UCaseMap));
    if(csm==NULL) {
        return NULL;
    }
    uprv_memset(csm, 0, sizeof(UCaseMap));

    csm->csp=ucase_getSingleton();
    ucasemap_setLocale(csm, locale, pErrorCode);
    if(U_FAILURE(*pErrorCode)) {
        uprv_free(csm);
        return NULL;
    }

    csm->options=options;
    return csm;
}

U_CAPI void U_EXPORT2
ucasemap_close(UCaseMap *csm) {
    if(csm!=NULL) {
#if !UCONFIG_NO_BREAK_ITERATION
        // Do not call ubrk_close() so that we do not depend on all of the BreakIterator code.
        delete reinterpret_cast<BreakIterator *>(csm->iter);
#endif
        uprv_free(csm);
    }
}

U_CAPI const char * U_EXPORT2
ucasemap_getLocale(const UCaseMap *csm) {
    return csm->locale;
}

U_CAPI uint32_t U_EXPORT2
ucasemap_getOptions(const UCaseMap *csm) {
    return csm->options;
}

U_CAPI void U_EXPORT2
ucasemap_setLocale(UCaseMap *csm, const char *locale, UErrorCode *pErrorCode) {
    int32_t length;

    if(U_FAILURE(*pErrorCode)) {
        return;
    }

    length=uloc_getName(locale, csm->locale, (int32_t)sizeof(csm->locale), pErrorCode);
    if(*pErrorCode==U_BUFFER_OVERFLOW_ERROR || length==sizeof(csm->locale)) {
        *pErrorCode=U_ZERO_ERROR;
        /* we only really need the language code for case mappings */
        length=uloc_getLanguage(locale, csm->locale, (int32_t)sizeof(csm->locale), pErrorCode);
    }
    if(length==sizeof(csm->locale)) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    csm->locCache=0;
    if(U_SUCCESS(*pErrorCode)) {
        ucase_getCaseLocale(csm->locale, &csm->locCache);
    } else {
        csm->locale[0]=0;
    }
}

U_CAPI void U_EXPORT2
ucasemap_setOptions(UCaseMap *csm, uint32_t options, UErrorCode * /*pErrorCode*/) {
    csm->options=options;
}

/* UTF-8 string case mappings ----------------------------------------------- */

/* TODO(markus): Move to a new, separate utf8case.c file. */

/* append a full case mapping result, see UCASE_MAX_STRING_LENGTH */
static inline int32_t
appendResult(uint8_t *dest, int32_t destIndex, int32_t destCapacity,
             int32_t result, const UChar *s) {
    UChar32 c;
    int32_t length, destLength;
    UErrorCode errorCode;

    /* decode the result */
    if(result<0) {
        /* (not) original code point */
        c=~result;
        length=-1;
    } else if(result<=UCASE_MAX_STRING_LENGTH) {
        c=U_SENTINEL;
        length=result;
    } else {
        c=result;
        length=-1;
    }

    if(destIndex<destCapacity) {
        /* append the result */
        if(length<0) {
            /* code point */
            UBool isError=FALSE;
            U8_APPEND(dest, destIndex, destCapacity, c, isError);
            if(isError) {
                /* overflow, nothing written */
                destIndex+=U8_LENGTH(c);
            }
        } else {
            /* string */
            errorCode=U_ZERO_ERROR;
            u_strToUTF8(
                (char *)(dest+destIndex), destCapacity-destIndex, &destLength,
                s, length,
                &errorCode);
            destIndex+=destLength;
            /* we might have an overflow, but we know the actual length */
        }
    } else {
        /* preflight */
        if(length<0) {
            destIndex+=U8_LENGTH(c);
        } else {
            errorCode=U_ZERO_ERROR;
            u_strToUTF8(
                NULL, 0, &destLength,
                s, length,
                &errorCode);
            destIndex+=destLength;
        }
    }
    return destIndex;
}

static UChar32 U_CALLCONV
utf8_caseContextIterator(void *context, int8_t dir) {
    UCaseContext *csc=(UCaseContext *)context;
    UChar32 c;

    if(dir<0) {
        /* reset for backward iteration */
        csc->index=csc->cpStart;
        csc->dir=dir;
    } else if(dir>0) {
        /* reset for forward iteration */
        csc->index=csc->cpLimit;
        csc->dir=dir;
    } else {
        /* continue current iteration direction */
        dir=csc->dir;
    }

    if(dir<0) {
        if(csc->start<csc->index) {
            U8_PREV((const uint8_t *)csc->p, csc->start, csc->index, c);
            return c;
        }
    } else {
        if(csc->index<csc->limit) {
            U8_NEXT((const uint8_t *)csc->p, csc->index, csc->limit, c);
            return c;
        }
    }
    return U_SENTINEL;
}

/*
 * Case-maps [srcStart..srcLimit[ but takes
 * context [0..srcLength[ into account.
 */
static int32_t
_caseMap(const UCaseMap *csm, UCaseMapFull *map,
         uint8_t *dest, int32_t destCapacity,
         const uint8_t *src, UCaseContext *csc,
         int32_t srcStart, int32_t srcLimit,
         UErrorCode *pErrorCode) {
    const UChar *s = NULL;
    UChar32 c, c2 = 0;
    int32_t srcIndex, destIndex;
    int32_t locCache;

    locCache=csm->locCache;

    /* case mapping loop */
    srcIndex=srcStart;
    destIndex=0;
    while(srcIndex<srcLimit) {
        csc->cpStart=srcIndex;
        U8_NEXT(src, srcIndex, srcLimit, c);
        csc->cpLimit=srcIndex;
        if(c<0) {
            int32_t i=csc->cpStart;
            while(destIndex<destCapacity && i<srcIndex) {
                dest[destIndex++]=src[i++];
            }
            continue;
        }
        c=map(csm->csp, c, utf8_caseContextIterator, csc, &s, csm->locale, &locCache);
        if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0x7f : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0x7f)) {
            /* fast path version of appendResult() for ASCII results */
            dest[destIndex++]=(uint8_t)c2;
        } else {
            destIndex=appendResult(dest, destIndex, destCapacity, c, s);
        }
    }

    if(destIndex>destCapacity) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    return destIndex;
}

#if !UCONFIG_NO_BREAK_ITERATION

U_CFUNC int32_t U_CALLCONV
ucasemap_internalUTF8ToTitle(const UCaseMap *csm,
         uint8_t *dest, int32_t destCapacity,
         const uint8_t *src, int32_t srcLength,
         UErrorCode *pErrorCode) {
    const UChar *s;
    UChar32 c;
    int32_t prev, titleStart, titleLimit, idx, destIndex, length;
    UBool isFirstIndex;

    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }

    // Use the C++ abstract base class to minimize dependencies.
    // TODO: Change UCaseMap.iter to store a BreakIterator directly.
    BreakIterator *bi=reinterpret_cast<BreakIterator *>(csm->iter);

    /* set up local variables */
    int32_t locCache=csm->locCache;
    UCaseContext csc=UCASECONTEXT_INITIALIZER;
    csc.p=(void *)src;
    csc.limit=srcLength;
    destIndex=0;
    prev=0;
    isFirstIndex=TRUE;

    /* titlecasing loop */
    while(prev<srcLength) {
        /* find next index where to titlecase */
        if(isFirstIndex) {
            isFirstIndex=FALSE;
            idx=bi->first();
        } else {
            idx=bi->next();
        }
        if(idx==UBRK_DONE || idx>srcLength) {
            idx=srcLength;
        }

        /*
         * Unicode 4 & 5 section 3.13 Default Case Operations:
         *
         * R3  toTitlecase(X): Find the word boundaries based on Unicode Standard Annex
         * #29, "Text Boundaries." Between each pair of word boundaries, find the first
         * cased character F. If F exists, map F to default_title(F); then map each
         * subsequent character C to default_lower(C).
         *
         * In this implementation, segment [prev..index[ into 3 parts:
         * a) uncased characters (copy as-is) [prev..titleStart[
         * b) first case letter (titlecase)         [titleStart..titleLimit[
         * c) subsequent characters (lowercase)                 [titleLimit..index[
         */
        if(prev<idx) {
            /* find and copy uncased characters [prev..titleStart[ */
            titleStart=titleLimit=prev;
            U8_NEXT(src, titleLimit, idx, c);
            if((csm->options&U_TITLECASE_NO_BREAK_ADJUSTMENT)==0 && UCASE_NONE==ucase_getType(csm->csp, c)) {
                /* Adjust the titlecasing index (titleStart) to the next cased character. */
                for(;;) {
                    titleStart=titleLimit;
                    if(titleLimit==idx) {
                        /*
                         * only uncased characters in [prev..index[
                         * stop with titleStart==titleLimit==index
                         */
                        break;
                    }
                    U8_NEXT(src, titleLimit, idx, c);
                    if(UCASE_NONE!=ucase_getType(csm->csp, c)) {
                        break; /* cased letter at [titleStart..titleLimit[ */
                    }
                }
                length=titleStart-prev;
                if(length>0) {
                    if((destIndex+length)<=destCapacity) {
                        uprv_memcpy(dest+destIndex, src+prev, length);
                    }
                    destIndex+=length;
                }
            }

            if(titleStart<titleLimit) {
                /* titlecase c which is from [titleStart..titleLimit[ */
                csc.cpStart=titleStart;
                csc.cpLimit=titleLimit;
                c=ucase_toFullTitle(csm->csp, c, utf8_caseContextIterator, &csc, &s, csm->locale, &locCache);
                destIndex=appendResult(dest, destIndex, destCapacity, c, s);

                /* Special case Dutch IJ titlecasing */
                if ( titleStart+1 < idx &&
                     ucase_getCaseLocale(csm->locale, &locCache) == UCASE_LOC_DUTCH &&
                     ( src[titleStart] == 0x0049 || src[titleStart] == 0x0069 ) &&
                     ( src[titleStart+1] == 0x004A || src[titleStart+1] == 0x006A )) {
                            c=0x004A;
                            destIndex=appendResult(dest, destIndex, destCapacity, c, s);
                            titleLimit++;
                }
                /* lowercase [titleLimit..index[ */
                if(titleLimit<idx) {
                    if((csm->options&U_TITLECASE_NO_LOWERCASE)==0) {
                        /* Normal operation: Lowercase the rest of the word. */
                        destIndex+=
                            _caseMap(
                                csm, ucase_toFullLower,
                                dest+destIndex, destCapacity-destIndex,
                                src, &csc,
                                titleLimit, idx,
                                pErrorCode);
                    } else {
                        /* Optionally just copy the rest of the word unchanged. */
                        length=idx-titleLimit;
                        if((destIndex+length)<=destCapacity) {
                            uprv_memcpy(dest+destIndex, src+titleLimit, length);
                        }
                        destIndex+=length;
                    }
                }
            }
        }

        prev=idx;
    }

    if(destIndex>destCapacity) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    return destIndex;
}

#endif

static int32_t U_CALLCONV
ucasemap_internalUTF8ToLower(const UCaseMap *csm,
                             uint8_t *dest, int32_t destCapacity,
                             const uint8_t *src, int32_t srcLength,
                             UErrorCode *pErrorCode) {
    UCaseContext csc=UCASECONTEXT_INITIALIZER;
    csc.p=(void *)src;
    csc.limit=srcLength;
    return _caseMap(
        csm, ucase_toFullLower,
        dest, destCapacity,
        src, &csc, 0, srcLength,
        pErrorCode);
}

static int32_t U_CALLCONV
ucasemap_internalUTF8ToUpper(const UCaseMap *csm,
                             uint8_t *dest, int32_t destCapacity,
                             const uint8_t *src, int32_t srcLength,
                             UErrorCode *pErrorCode) {
    UCaseContext csc=UCASECONTEXT_INITIALIZER;
    csc.p=(void *)src;
    csc.limit=srcLength;
    return _caseMap(
        csm, ucase_toFullUpper,
        dest, destCapacity,
        src, &csc, 0, srcLength,
        pErrorCode);
}

static int32_t
utf8_foldCase(const UCaseProps *csp,
              uint8_t *dest, int32_t destCapacity,
              const uint8_t *src, int32_t srcLength,
              uint32_t options,
              UErrorCode *pErrorCode) {
    int32_t srcIndex, destIndex;

    const UChar *s;
    UChar32 c, c2;
    int32_t start;

    /* case mapping loop */
    srcIndex=destIndex=0;
    while(srcIndex<srcLength) {
        start=srcIndex;
        U8_NEXT(src, srcIndex, srcLength, c);
        if(c<0) {
            while(destIndex<destCapacity && start<srcIndex) {
                dest[destIndex++]=src[start++];
            }
            continue;
        }
        c=ucase_toFullFolding(csp, c, &s, options);
        if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0x7f : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0x7f)) {
            /* fast path version of appendResult() for ASCII results */
            dest[destIndex++]=(uint8_t)c2;
        } else {
            destIndex=appendResult(dest, destIndex, destCapacity, c, s);
        }
    }

    if(destIndex>destCapacity) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    return destIndex;
}

static int32_t U_CALLCONV
ucasemap_internalUTF8Fold(const UCaseMap *csm,
                          uint8_t *dest, int32_t destCapacity,
                          const uint8_t *src, int32_t srcLength,
                          UErrorCode *pErrorCode) {
    return utf8_foldCase(csm->csp, dest, destCapacity, src, srcLength, csm->options, pErrorCode);
}

U_CFUNC int32_t
ucasemap_mapUTF8(const UCaseMap *csm,
                 uint8_t *dest, int32_t destCapacity,
                 const uint8_t *src, int32_t srcLength,
                 UTF8CaseMapper *stringCaseMapper,
                 UErrorCode *pErrorCode) {
    int32_t destLength;

    /* check argument values */
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if( destCapacity<0 ||
        (dest==NULL && destCapacity>0) ||
        src==NULL ||
        srcLength<-1
    ) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    /* get the string length */
    if(srcLength==-1) {
        srcLength=(int32_t)uprv_strlen((const char *)src);
    }

    /* check for overlapping source and destination */
    if( dest!=NULL &&
        ((src>=dest && src<(dest+destCapacity)) ||
         (dest>=src && dest<(src+srcLength)))
    ) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    destLength=stringCaseMapper(csm, dest, destCapacity, src, srcLength, pErrorCode);
    return u_terminateChars((char *)dest, destCapacity, destLength, pErrorCode);
}

/* public API functions */

U_CAPI int32_t U_EXPORT2
ucasemap_utf8ToLower(const UCaseMap *csm,
                     char *dest, int32_t destCapacity,
                     const char *src, int32_t srcLength,
                     UErrorCode *pErrorCode) {
    return ucasemap_mapUTF8(csm,
                   (uint8_t *)dest, destCapacity,
                   (const uint8_t *)src, srcLength,
                   ucasemap_internalUTF8ToLower, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
ucasemap_utf8ToUpper(const UCaseMap *csm,
                     char *dest, int32_t destCapacity,
                     const char *src, int32_t srcLength,
                     UErrorCode *pErrorCode) {
    return ucasemap_mapUTF8(csm,
                   (uint8_t *)dest, destCapacity,
                   (const uint8_t *)src, srcLength,
                   ucasemap_internalUTF8ToUpper, pErrorCode);
}

U_CAPI int32_t U_EXPORT2
ucasemap_utf8FoldCase(const UCaseMap *csm,
                      char *dest, int32_t destCapacity,
                      const char *src, int32_t srcLength,
                      UErrorCode *pErrorCode) {
    return ucasemap_mapUTF8(csm,
                   (uint8_t *)dest, destCapacity,
                   (const uint8_t *)src, srcLength,
                   ucasemap_internalUTF8Fold, pErrorCode);
}
