// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
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
    int32_t length;
    UErrorCode errorCode;

    /* decode the result */
    if(result<0) {
        /* (not) original code point */
        c=~result;
        length=U8_LENGTH(c);
    } else if(result<=UCASE_MAX_STRING_LENGTH) {
        c=U_SENTINEL;
        length=result;
    } else {
        c=result;
        length=U8_LENGTH(c);
    }
    if(length>(INT32_MAX-destIndex)) {
        return -1;  // integer overflow
    }

    if(destIndex<destCapacity) {
        /* append the result */
        if(c>=0) {
            /* code point */
            UBool isError=FALSE;
            U8_APPEND(dest, destIndex, destCapacity, c, isError);
            if(isError) {
                /* overflow, nothing written */
                destIndex+=length;
            }
        } else {
            /* string */
            int32_t destLength;
            errorCode=U_ZERO_ERROR;
            u_strToUTF8(
                (char *)(dest+destIndex), destCapacity-destIndex, &destLength,
                s, length,
                &errorCode);
            if(U_FAILURE(errorCode) && errorCode != U_BUFFER_OVERFLOW_ERROR) {
                return -1;
            }
            if(destLength>(INT32_MAX-destIndex)) {
                return -1;  // integer overflow
            }
            destIndex+=destLength;
            /* we might have an overflow, but we know the actual length */
        }
    } else {
        /* preflight */
        if(c>=0) {
            destIndex+=length;
        } else {
            int32_t destLength;
            errorCode=U_ZERO_ERROR;
            u_strToUTF8(
                NULL, 0, &destLength,
                s, length,
                &errorCode);
            if(U_FAILURE(errorCode) && errorCode != U_BUFFER_OVERFLOW_ERROR) {
                return -1;
            }
            if(destLength>(INT32_MAX-destIndex)) {
                return -1;  // integer overflow
            }
            destIndex+=destLength;
        }
    }
    return destIndex;
}

static inline int32_t
appendUChar(uint8_t *dest, int32_t destIndex, int32_t destCapacity, UChar c) {
    int32_t length=U8_LENGTH(c);
    if(length>(INT32_MAX-destIndex)) {
        return -1;  // integer overflow
    }
    int32_t limit=destIndex+length;
    if(limit<destCapacity) {
        U8_APPEND_UNSAFE(dest, destIndex, c);
    }
    return limit;
}

static inline int32_t
appendString(uint8_t *dest, int32_t destIndex, int32_t destCapacity,
             const uint8_t *s, int32_t length) {
    if(length>0) {
        if(length>(INT32_MAX-destIndex)) {
            return -1;  // integer overflow
        }
        if((destIndex+length)<=destCapacity) {
            uprv_memcpy(dest+destIndex, s, length);
        }
        destIndex+=length;
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
            // Malformed UTF-8.
            destIndex=appendString(dest, destIndex, destCapacity, src+csc->cpStart, srcIndex-csc->cpStart);
            if(destIndex<0) {
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
            continue;
        }
        c=map(csm->csp, c, utf8_caseContextIterator, csc, &s, csm->locale, &locCache);
        if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0x7f : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0x7f)) {
            /* fast path version of appendResult() for ASCII results */
            dest[destIndex++]=(uint8_t)c2;
        } else {
            destIndex=appendResult(dest, destIndex, destCapacity, c, s);
            if(destIndex<0) {
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
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
    int32_t prev, titleStart, titleLimit, idx, destIndex;
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
                destIndex=appendString(dest, destIndex, destCapacity, src+prev, titleStart-prev);
                if(destIndex<0) {
                    *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                    return 0;
                }
            }

            if(titleStart<titleLimit) {
                /* titlecase c which is from [titleStart..titleLimit[ */
                if(c>=0) {
                    csc.cpStart=titleStart;
                    csc.cpLimit=titleLimit;
                    c=ucase_toFullTitle(csm->csp, c, utf8_caseContextIterator, &csc, &s, csm->locale, &locCache);
                    destIndex=appendResult(dest, destIndex, destCapacity, c, s);
                } else {
                    // Malformed UTF-8.
                    destIndex=appendString(dest, destIndex, destCapacity, src+titleStart, titleLimit-titleStart);
                }
                if(destIndex<0) {
                    *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                    return 0;
                }

                /* Special case Dutch IJ titlecasing */
                if (titleStart+1 < idx &&
                        ucase_getCaseLocale(csm->locale, &locCache) == UCASE_LOC_DUTCH &&
                        (src[titleStart] == 0x0049 || src[titleStart] == 0x0069) &&
                        (src[titleStart+1] == 0x004A || src[titleStart+1] == 0x006A)) {
                    destIndex=appendUChar(dest, destIndex, destCapacity, 0x004A);
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
                        if(U_FAILURE(*pErrorCode)) {
                            return destIndex;
                        }
                    } else {
                        /* Optionally just copy the rest of the word unchanged. */
                        destIndex=appendString(dest, destIndex, destCapacity, src+titleLimit, idx-titleLimit);
                        if(destIndex<0) {
                            *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                            return 0;
                        }
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

U_NAMESPACE_BEGIN
namespace GreekUpper {

UBool isFollowedByCasedLetter(const UCaseProps *csp, const uint8_t *s, int32_t i, int32_t length) {
    while (i < length) {
        UChar32 c;
        U8_NEXT(s, i, length, c);
        int32_t type = ucase_getTypeOrIgnorable(csp, c);
        if ((type & UCASE_IGNORABLE) != 0) {
            // Case-ignorable, continue with the loop.
        } else if (type != UCASE_NONE) {
            return TRUE;  // Followed by cased letter.
        } else {
            return FALSE;  // Uncased and not case-ignorable.
        }
    }
    return FALSE;  // Not followed by cased letter.
}

// Keep this consistent with the UTF-16 version in ustrcase.cpp and the Java version in CaseMap.java.
int32_t toUpper(const UCaseMap *csm,
                uint8_t *dest, int32_t destCapacity,
                const uint8_t *src, int32_t srcLength,
                UErrorCode *pErrorCode) {
    int32_t locCache = UCASE_LOC_GREEK;
    int32_t destIndex=0;
    uint32_t state = 0;
    for (int32_t i = 0; i < srcLength;) {
        int32_t nextIndex = i;
        UChar32 c;
        U8_NEXT(src, nextIndex, srcLength, c);
        uint32_t nextState = 0;
        int32_t type = ucase_getTypeOrIgnorable(csm->csp, c);
        if ((type & UCASE_IGNORABLE) != 0) {
            // c is case-ignorable
            nextState |= (state & AFTER_CASED);
        } else if (type != UCASE_NONE) {
            // c is cased
            nextState |= AFTER_CASED;
        }
        uint32_t data = getLetterData(c);
        if (data > 0) {
            uint32_t upper = data & UPPER_MASK;
            // Add a dialytika to this iota or ypsilon vowel
            // if we removed a tonos from the previous vowel,
            // and that previous vowel did not also have (or gain) a dialytika.
            // Adding one only to the final vowel in a longer sequence
            // (which does not occur in normal writing) would require lookahead.
            // Set the same flag as for preserving an existing dialytika.
            if ((data & HAS_VOWEL) != 0 && (state & AFTER_VOWEL_WITH_ACCENT) != 0 &&
                    (upper == 0x399 || upper == 0x3A5)) {
                data |= HAS_DIALYTIKA;
            }
            int32_t numYpogegrammeni = 0;  // Map each one to a trailing, spacing, capital iota.
            if ((data & HAS_YPOGEGRAMMENI) != 0) {
                numYpogegrammeni = 1;
            }
            // Skip combining diacritics after this Greek letter.
            int32_t nextNextIndex = nextIndex;
            while (nextIndex < srcLength) {
                UChar32 c2;
                U8_NEXT(src, nextNextIndex, srcLength, c2);
                uint32_t diacriticData = getDiacriticData(c2);
                if (diacriticData != 0) {
                    data |= diacriticData;
                    if ((diacriticData & HAS_YPOGEGRAMMENI) != 0) {
                        ++numYpogegrammeni;
                    }
                    nextIndex = nextNextIndex;
                } else {
                    break;  // not a Greek diacritic
                }
            }
            if ((data & HAS_VOWEL_AND_ACCENT_AND_DIALYTIKA) == HAS_VOWEL_AND_ACCENT) {
                nextState |= AFTER_VOWEL_WITH_ACCENT;
            }
            // Map according to Greek rules.
            UBool addTonos = FALSE;
            if (upper == 0x397 &&
                    (data & HAS_ACCENT) != 0 &&
                    numYpogegrammeni == 0 &&
                    (state & AFTER_CASED) == 0 &&
                    !isFollowedByCasedLetter(csm->csp, src, nextIndex, srcLength)) {
                // Keep disjunctive "or" with (only) a tonos.
                // We use the same "word boundary" conditions as for the Final_Sigma test.
                if (i == nextIndex) {
                    upper = 0x389;  // Preserve the precomposed form.
                } else {
                    addTonos = TRUE;
                }
            } else if ((data & HAS_DIALYTIKA) != 0) {
                // Preserve a vowel with dialytika in precomposed form if it exists.
                if (upper == 0x399) {
                    upper = 0x3AA;
                    data &= ~HAS_EITHER_DIALYTIKA;
                } else if (upper == 0x3A5) {
                    upper = 0x3AB;
                    data &= ~HAS_EITHER_DIALYTIKA;
                }
            }
            destIndex=appendUChar(dest, destIndex, destCapacity, (UChar)upper);
            if (destIndex >= 0 && (data & HAS_EITHER_DIALYTIKA) != 0) {
                destIndex=appendUChar(dest, destIndex, destCapacity, 0x308);  // restore or add a dialytika
            }
            if (destIndex >= 0 && addTonos) {
                destIndex=appendUChar(dest, destIndex, destCapacity, 0x301);
            }
            while (destIndex >= 0 && numYpogegrammeni > 0) {
                destIndex=appendUChar(dest, destIndex, destCapacity, 0x399);
                --numYpogegrammeni;
            }
            if(destIndex<0) {
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        } else if(c>=0) {
            const UChar *s;
            UChar32 c2 = 0;
            c=ucase_toFullUpper(csm->csp, c, NULL, NULL, &s, csm->locale, &locCache);
            if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0x7f : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0x7f)) {
                /* fast path version of appendResult() for ASCII results */
                dest[destIndex++]=(uint8_t)c2;
            } else {
                destIndex=appendResult(dest, destIndex, destCapacity, c, s);
                if(destIndex<0) {
                    *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                    return 0;
                }
            }
        } else {
            // Malformed UTF-8.
            destIndex=appendString(dest, destIndex, destCapacity, src+i, nextIndex-i);
            if(destIndex<0) {
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        }
        i = nextIndex;
        state = nextState;
    }

    if(destIndex>destCapacity) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    return destIndex;
}

}  // namespace GreekUpper
U_NAMESPACE_END

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
    int32_t locCache = csm->locCache;
    if (ucase_getCaseLocale(csm->locale, &locCache) == UCASE_LOC_GREEK) {
        return GreekUpper::toUpper(csm, dest, destCapacity, src, srcLength, pErrorCode);
    }
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
            // Malformed UTF-8.
            destIndex=appendString(dest, destIndex, destCapacity, src+start, srcIndex-start);
            if(destIndex<0) {
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
            continue;
        }
        c=ucase_toFullFolding(csp, c, &s, options);
        if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0x7f : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0x7f)) {
            /* fast path version of appendResult() for ASCII results */
            dest[destIndex++]=(uint8_t)c2;
        } else {
            destIndex=appendResult(dest, destIndex, destCapacity, c, s);
            if(destIndex<0) {
                *pErrorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
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
