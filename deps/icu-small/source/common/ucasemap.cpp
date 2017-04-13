// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucasemap.cpp
*   encoding:   UTF-8
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
#include "unicode/casemap.h"
#include "unicode/edits.h"
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
#include "uassert.h"
#include "ucase.h"
#include "ucasemap_imp.h"
#include "ustr_imp.h"

U_NAMESPACE_BEGIN

namespace {

// TODO: share with UTF-16? inline in ucasemap_imp.h?
int32_t checkOverflowAndEditsError(int32_t destIndex, int32_t destCapacity,
                                   Edits *edits, UErrorCode &errorCode) {
    if (U_SUCCESS(errorCode)) {
        if (destIndex > destCapacity) {
            errorCode = U_BUFFER_OVERFLOW_ERROR;
        } else if (edits != NULL) {
            edits->copyErrorTo(errorCode);
        }
    }
    return destIndex;
}

}  // namespace

U_NAMESPACE_END

U_NAMESPACE_USE

/* UCaseMap service object -------------------------------------------------- */

UCaseMap::UCaseMap(const char *localeID, uint32_t opts, UErrorCode *pErrorCode) :
#if !UCONFIG_NO_BREAK_ITERATION
        iter(NULL),
#endif
        caseLocale(UCASE_LOC_UNKNOWN), options(opts) {
    ucasemap_setLocale(this, localeID, pErrorCode);
}

UCaseMap::~UCaseMap() {
#if !UCONFIG_NO_BREAK_ITERATION
    delete iter;
#endif
}

U_CAPI UCaseMap * U_EXPORT2
ucasemap_open(const char *locale, uint32_t options, UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return NULL;
    }
    UCaseMap *csm = new UCaseMap(locale, options, pErrorCode);
    if(csm==NULL) {
        *pErrorCode = U_MEMORY_ALLOCATION_ERROR;
        return NULL;
    } else if (U_FAILURE(*pErrorCode)) {
        delete csm;
        return NULL;
    }
    return csm;
}

U_CAPI void U_EXPORT2
ucasemap_close(UCaseMap *csm) {
    delete csm;
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
    if(U_FAILURE(*pErrorCode)) {
        return;
    }
    if (locale != NULL && *locale == 0) {
        csm->locale[0] = 0;
        csm->caseLocale = UCASE_LOC_ROOT;
        return;
    }

    int32_t length=uloc_getName(locale, csm->locale, (int32_t)sizeof(csm->locale), pErrorCode);
    if(*pErrorCode==U_BUFFER_OVERFLOW_ERROR || length==sizeof(csm->locale)) {
        *pErrorCode=U_ZERO_ERROR;
        /* we only really need the language code for case mappings */
        length=uloc_getLanguage(locale, csm->locale, (int32_t)sizeof(csm->locale), pErrorCode);
    }
    if(length==sizeof(csm->locale)) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    if(U_SUCCESS(*pErrorCode)) {
        csm->caseLocale=UCASE_LOC_UNKNOWN;
        csm->caseLocale = ucase_getCaseLocale(csm->locale);
    } else {
        csm->locale[0]=0;
        csm->caseLocale = UCASE_LOC_ROOT;
    }
}

U_CAPI void U_EXPORT2
ucasemap_setOptions(UCaseMap *csm, uint32_t options, UErrorCode *pErrorCode) {
    if(U_FAILURE(*pErrorCode)) {
        return;
    }
    csm->options=options;
}

/* UTF-8 string case mappings ----------------------------------------------- */

/* TODO(markus): Move to a new, separate utf8case.cpp file. */

/* append a full case mapping result, see UCASE_MAX_STRING_LENGTH */
static inline int32_t
appendResult(uint8_t *dest, int32_t destIndex, int32_t destCapacity,
             int32_t result, const UChar *s,
             int32_t cpLength, uint32_t options, icu::Edits *edits) {
    UChar32 c;
    int32_t length;
    UErrorCode errorCode;

    /* decode the result */
    if(result<0) {
        /* (not) original code point */
        if(edits!=NULL) {
            edits->addUnchanged(cpLength);
            if(options & UCASEMAP_OMIT_UNCHANGED_TEXT) {
                return destIndex;
            }
        }
        c=~result;
        if(destIndex<destCapacity && c<=0x7f) {  // ASCII slightly-fastpath
            dest[destIndex++]=(uint8_t)c;
            return destIndex;
        }
        length=cpLength;
    } else {
        if(result<=UCASE_MAX_STRING_LENGTH) {
            // string: "result" is the UTF-16 length
            errorCode=U_ZERO_ERROR;
            if(destIndex<destCapacity) {
                u_strToUTF8((char *)(dest+destIndex), destCapacity-destIndex, &length,
                            s, result, &errorCode);
            } else {
                u_strToUTF8(NULL, 0, &length, s, result, &errorCode);
            }
            if(U_FAILURE(errorCode) && errorCode != U_BUFFER_OVERFLOW_ERROR) {
                return -1;
            }
            if(length>(INT32_MAX-destIndex)) {
                return -1;  // integer overflow
            }
            if(edits!=NULL) {
                edits->addReplace(cpLength, length);
            }
            // We might have an overflow, but we know the actual length.
            return destIndex+length;
        } else if(destIndex<destCapacity && result<=0x7f) {  // ASCII slightly-fastpath
            dest[destIndex++]=(uint8_t)result;
            if(edits!=NULL) {
                edits->addReplace(cpLength, 1);
            }
            return destIndex;
        } else {
            c=result;
            length=U8_LENGTH(c);
            if(edits!=NULL) {
                edits->addReplace(cpLength, length);
            }
        }
    }
    // c>=0 single code point
    if(length>(INT32_MAX-destIndex)) {
        return -1;  // integer overflow
    }

    if(destIndex<destCapacity) {
        /* append the result */
        UBool isError=FALSE;
        U8_APPEND(dest, destIndex, destCapacity, c, isError);
        if(isError) {
            /* overflow, nothing written */
            destIndex+=length;
        }
    } else {
        /* preflight */
        destIndex+=length;
    }
    return destIndex;
}

static inline int32_t
appendASCII(uint8_t *dest, int32_t destIndex, int32_t destCapacity, uint8_t c) {
    if(destIndex<destCapacity) {
        dest[destIndex]=c;
    } else if(destIndex==INT32_MAX) {
        return -1;  // integer overflow
    }
    return destIndex+1;
}

// See unicode/utf8.h U8_APPEND_UNSAFE().
static inline uint8_t getTwoByteLead(UChar32 c) { return (uint8_t)((c >> 6) | 0xc0); }
static inline uint8_t getTwoByteTrail(UChar32 c) { return (uint8_t)((c & 0x3f) | 0x80); }

static inline int32_t
appendTwoBytes(uint8_t *dest, int32_t destIndex, int32_t destCapacity, UChar32 c) {
    U_ASSERT(0x370 <= c && c <= 0x3ff);  // 2-byte UTF-8, main Greek block
    if(2>(INT32_MAX-destIndex)) {
        return -1;  // integer overflow
    }
    int32_t limit=destIndex+2;
    if(limit<=destCapacity) {
        dest+=destIndex;
        dest[0]=getTwoByteLead(c);
        dest[1]=getTwoByteTrail(c);
    }
    return limit;
}

static inline int32_t
appendTwoBytes(uint8_t *dest, int32_t destIndex, int32_t destCapacity, const char *s) {
    if(2>(INT32_MAX-destIndex)) {
        return -1;  // integer overflow
    }
    int32_t limit=destIndex+2;
    if(limit<=destCapacity) {
        dest+=destIndex;
        dest[0]=(uint8_t)s[0];
        dest[1]=(uint8_t)s[1];
    }
    return limit;
}

static inline int32_t
appendUnchanged(uint8_t *dest, int32_t destIndex, int32_t destCapacity,
                const uint8_t *s, int32_t length, uint32_t options, icu::Edits *edits) {
    if(length>0) {
        if(edits!=NULL) {
            edits->addUnchanged(length);
            if(options & UCASEMAP_OMIT_UNCHANGED_TEXT) {
                return destIndex;
            }
        }
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
_caseMap(int32_t caseLocale, uint32_t options, UCaseMapFull *map,
         uint8_t *dest, int32_t destCapacity,
         const uint8_t *src, UCaseContext *csc,
         int32_t srcStart, int32_t srcLimit,
         icu::Edits *edits,
         UErrorCode &errorCode) {
    /* case mapping loop */
    int32_t srcIndex=srcStart;
    int32_t destIndex=0;
    while(srcIndex<srcLimit) {
        int32_t cpStart;
        csc->cpStart=cpStart=srcIndex;
        UChar32 c;
        U8_NEXT(src, srcIndex, srcLimit, c);
        csc->cpLimit=srcIndex;
        if(c<0) {
            // Malformed UTF-8.
            destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                      src+cpStart, srcIndex-cpStart, options, edits);
            if(destIndex<0) {
                errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
            continue;
        }
        const UChar *s;
        c=map(c, utf8_caseContextIterator, csc, &s, caseLocale);
        destIndex = appendResult(dest, destIndex, destCapacity, c, s,
                                 srcIndex - cpStart, options, edits);
        if (destIndex < 0) {
            errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }
    }

    return destIndex;
}

#if !UCONFIG_NO_BREAK_ITERATION

U_CFUNC int32_t U_CALLCONV
ucasemap_internalUTF8ToTitle(
        int32_t caseLocale, uint32_t options, BreakIterator *iter,
        uint8_t *dest, int32_t destCapacity,
        const uint8_t *src, int32_t srcLength,
        icu::Edits *edits,
        UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return 0;
    }

    /* set up local variables */
    UCaseContext csc=UCASECONTEXT_INITIALIZER;
    csc.p=(void *)src;
    csc.limit=srcLength;
    int32_t destIndex=0;
    int32_t prev=0;
    UBool isFirstIndex=TRUE;

    /* titlecasing loop */
    while(prev<srcLength) {
        /* find next index where to titlecase */
        int32_t index;
        if(isFirstIndex) {
            isFirstIndex=FALSE;
            index=iter->first();
        } else {
            index=iter->next();
        }
        if(index==UBRK_DONE || index>srcLength) {
            index=srcLength;
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
        if(prev<index) {
            /* find and copy uncased characters [prev..titleStart[ */
            int32_t titleStart=prev;
            int32_t titleLimit=prev;
            UChar32 c;
            U8_NEXT(src, titleLimit, index, c);
            if((options&U_TITLECASE_NO_BREAK_ADJUSTMENT)==0 && UCASE_NONE==ucase_getType(c)) {
                /* Adjust the titlecasing index (titleStart) to the next cased character. */
                for(;;) {
                    titleStart=titleLimit;
                    if(titleLimit==index) {
                        /*
                         * only uncased characters in [prev..index[
                         * stop with titleStart==titleLimit==index
                         */
                        break;
                    }
                    U8_NEXT(src, titleLimit, index, c);
                    if(UCASE_NONE!=ucase_getType(c)) {
                        break; /* cased letter at [titleStart..titleLimit[ */
                    }
                }
                destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                          src+prev, titleStart-prev, options, edits);
                if(destIndex<0) {
                    errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                    return 0;
                }
            }

            if(titleStart<titleLimit) {
                /* titlecase c which is from [titleStart..titleLimit[ */
                if(c>=0) {
                    csc.cpStart=titleStart;
                    csc.cpLimit=titleLimit;
                    const UChar *s;
                    c=ucase_toFullTitle(c, utf8_caseContextIterator, &csc, &s, caseLocale);
                    destIndex=appendResult(dest, destIndex, destCapacity, c, s,
                                           titleLimit-titleStart, options, edits);
                } else {
                    // Malformed UTF-8.
                    destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                              src+titleStart, titleLimit-titleStart, options, edits);
                }
                if(destIndex<0) {
                    errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                    return 0;
                }

                /* Special case Dutch IJ titlecasing */
                if (titleStart+1 < index &&
                        caseLocale == UCASE_LOC_DUTCH &&
                        (src[titleStart] == 0x0049 || src[titleStart] == 0x0069)) {
                    if (src[titleStart+1] == 0x006A) {
                        destIndex=appendASCII(dest, destIndex, destCapacity, 0x004A);
                        if(destIndex<0) {
                            errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                            return 0;
                        }
                        if(edits!=NULL) {
                            edits->addReplace(1, 1);
                        }
                        titleLimit++;
                    } else if (src[titleStart+1] == 0x004A) {
                        // Keep the capital J from getting lowercased.
                        destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                                  src+titleStart+1, 1, options, edits);
                        if(destIndex<0) {
                            errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                            return 0;
                        }
                        titleLimit++;
                    }
                }

                /* lowercase [titleLimit..index[ */
                if(titleLimit<index) {
                    if((options&U_TITLECASE_NO_LOWERCASE)==0) {
                        /* Normal operation: Lowercase the rest of the word. */
                        destIndex+=
                            _caseMap(
                                caseLocale, options, ucase_toFullLower,
                                dest+destIndex, destCapacity-destIndex,
                                src, &csc,
                                titleLimit, index,
                                edits, errorCode);
                        if(errorCode==U_BUFFER_OVERFLOW_ERROR) {
                            errorCode=U_ZERO_ERROR;
                        }
                        if(U_FAILURE(errorCode)) {
                            return destIndex;
                        }
                    } else {
                        /* Optionally just copy the rest of the word unchanged. */
                        destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                                  src+titleLimit, index-titleLimit, options, edits);
                        if(destIndex<0) {
                            errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                            return 0;
                        }
                    }
                }
            }
        }

        prev=index;
    }

    return checkOverflowAndEditsError(destIndex, destCapacity, edits, errorCode);
}

#endif

U_NAMESPACE_BEGIN
namespace GreekUpper {

UBool isFollowedByCasedLetter(const uint8_t *s, int32_t i, int32_t length) {
    while (i < length) {
        UChar32 c;
        U8_NEXT(s, i, length, c);
        int32_t type = ucase_getTypeOrIgnorable(c);
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
int32_t toUpper(uint32_t options,
                uint8_t *dest, int32_t destCapacity,
                const uint8_t *src, int32_t srcLength,
                Edits *edits,
                UErrorCode &errorCode) {
    int32_t destIndex=0;
    uint32_t state = 0;
    for (int32_t i = 0; i < srcLength;) {
        int32_t nextIndex = i;
        UChar32 c;
        U8_NEXT(src, nextIndex, srcLength, c);
        uint32_t nextState = 0;
        int32_t type = ucase_getTypeOrIgnorable(c);
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
                    !isFollowedByCasedLetter(src, nextIndex, srcLength)) {
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

            UBool change = TRUE;
            if (edits != NULL) {
                // Find out first whether we are changing the text.
                U_ASSERT(0x370 <= upper && upper <= 0x3ff);  // 2-byte UTF-8, main Greek block
                change = (i + 2) > nextIndex ||
                        src[i] != getTwoByteLead(upper) || src[i + 1] != getTwoByteTrail(upper) ||
                        numYpogegrammeni > 0;
                int32_t i2 = i + 2;
                if ((data & HAS_EITHER_DIALYTIKA) != 0) {
                    change |= (i2 + 2) > nextIndex ||
                            src[i2] != (uint8_t)u8"\u0308"[0] ||
                            src[i2 + 1] != (uint8_t)u8"\u0308"[1];
                    i2 += 2;
                }
                if (addTonos) {
                    change |= (i2 + 2) > nextIndex ||
                            src[i2] != (uint8_t)u8"\u0301"[0] ||
                            src[i2 + 1] != (uint8_t)u8"\u0301"[1];
                    i2 += 2;
                }
                int32_t oldLength = nextIndex - i;
                int32_t newLength = (i2 - i) + numYpogegrammeni * 2;  // 2 bytes per U+0399
                change |= oldLength != newLength;
                if (change) {
                    if (edits != NULL) {
                        edits->addReplace(oldLength, newLength);
                    }
                } else {
                    if (edits != NULL) {
                        edits->addUnchanged(oldLength);
                    }
                    // Write unchanged text?
                    change = (options & UCASEMAP_OMIT_UNCHANGED_TEXT) == 0;
                }
            }

            if (change) {
                destIndex=appendTwoBytes(dest, destIndex, destCapacity, upper);
                if (destIndex >= 0 && (data & HAS_EITHER_DIALYTIKA) != 0) {
                    destIndex=appendTwoBytes(dest, destIndex, destCapacity, u8"\u0308");  // restore or add a dialytika
                }
                if (destIndex >= 0 && addTonos) {
                    destIndex=appendTwoBytes(dest, destIndex, destCapacity, u8"\u0301");
                }
                while (destIndex >= 0 && numYpogegrammeni > 0) {
                    destIndex=appendTwoBytes(dest, destIndex, destCapacity, u8"\u0399");
                    --numYpogegrammeni;
                }
                if(destIndex<0) {
                    errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                    return 0;
                }
            }
        } else if(c>=0) {
            const UChar *s;
            c=ucase_toFullUpper(c, NULL, NULL, &s, UCASE_LOC_GREEK);
            destIndex = appendResult(dest, destIndex, destCapacity, c, s,
                                     nextIndex - i, options, edits);
            if (destIndex < 0) {
                errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        } else {
            // Malformed UTF-8.
            destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                      src+i, nextIndex-i, options, edits);
            if(destIndex<0) {
                errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
        }
        i = nextIndex;
        state = nextState;
    }

    return destIndex;
}

}  // namespace GreekUpper
U_NAMESPACE_END

static int32_t U_CALLCONV
ucasemap_internalUTF8ToLower(int32_t caseLocale, uint32_t options, UCASEMAP_BREAK_ITERATOR_UNUSED
                             uint8_t *dest, int32_t destCapacity,
                             const uint8_t *src, int32_t srcLength,
                             icu::Edits *edits,
                             UErrorCode &errorCode) {
    UCaseContext csc=UCASECONTEXT_INITIALIZER;
    csc.p=(void *)src;
    csc.limit=srcLength;
    int32_t destIndex = _caseMap(
        caseLocale, options, ucase_toFullLower,
        dest, destCapacity,
        src, &csc, 0, srcLength,
        edits, errorCode);
    return checkOverflowAndEditsError(destIndex, destCapacity, edits, errorCode);
}

static int32_t U_CALLCONV
ucasemap_internalUTF8ToUpper(int32_t caseLocale, uint32_t options, UCASEMAP_BREAK_ITERATOR_UNUSED
                             uint8_t *dest, int32_t destCapacity,
                             const uint8_t *src, int32_t srcLength,
                             icu::Edits *edits,
                             UErrorCode &errorCode) {
    int32_t destIndex;
    if (caseLocale == UCASE_LOC_GREEK) {
        destIndex = GreekUpper::toUpper(options, dest, destCapacity,
                                        src, srcLength, edits, errorCode);
    } else {
        UCaseContext csc=UCASECONTEXT_INITIALIZER;
        csc.p=(void *)src;
        csc.limit=srcLength;
        destIndex = _caseMap(
            caseLocale, options, ucase_toFullUpper,
            dest, destCapacity,
            src, &csc, 0, srcLength,
            edits, errorCode);
    }
    return checkOverflowAndEditsError(destIndex, destCapacity, edits, errorCode);
}

static int32_t U_CALLCONV
ucasemap_internalUTF8Fold(int32_t /* caseLocale */, uint32_t options, UCASEMAP_BREAK_ITERATOR_UNUSED
                          uint8_t *dest, int32_t destCapacity,
                          const uint8_t *src, int32_t srcLength,
                          icu::Edits *edits,
                          UErrorCode &errorCode) {
    /* case mapping loop */
    int32_t srcIndex = 0;
    int32_t destIndex = 0;
    while (srcIndex < srcLength) {
        int32_t cpStart = srcIndex;
        UChar32 c;
        U8_NEXT(src, srcIndex, srcLength, c);
        if(c<0) {
            // Malformed UTF-8.
            destIndex=appendUnchanged(dest, destIndex, destCapacity,
                                      src+cpStart, srcIndex-cpStart, options, edits);
            if(destIndex<0) {
                errorCode=U_INDEX_OUTOFBOUNDS_ERROR;
                return 0;
            }
            continue;
        }
        const UChar *s;
        c = ucase_toFullFolding(c, &s, options);
        destIndex = appendResult(dest, destIndex, destCapacity, c, s,
                                 srcIndex - cpStart, options, edits);
        if (destIndex < 0) {
            errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return 0;
        }
    }

    return checkOverflowAndEditsError(destIndex, destCapacity, edits, errorCode);
}

U_CFUNC int32_t
ucasemap_mapUTF8(int32_t caseLocale, uint32_t options, UCASEMAP_BREAK_ITERATOR_PARAM
                 uint8_t *dest, int32_t destCapacity,
                 const uint8_t *src, int32_t srcLength,
                 UTF8CaseMapper *stringCaseMapper,
                 icu::Edits *edits,
                 UErrorCode &errorCode) {
    int32_t destLength;

    /* check argument values */
    if(U_FAILURE(errorCode)) {
        return 0;
    }
    if( destCapacity<0 ||
        (dest==NULL && destCapacity>0) ||
        src==NULL ||
        srcLength<-1
    ) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
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
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    if(edits!=NULL) {
        edits->reset();
    }
    destLength=stringCaseMapper(caseLocale, options, UCASEMAP_BREAK_ITERATOR
                                dest, destCapacity, src, srcLength, edits, errorCode);
    return u_terminateChars((char *)dest, destCapacity, destLength, &errorCode);
}

/* public API functions */

U_CAPI int32_t U_EXPORT2
ucasemap_utf8ToLower(const UCaseMap *csm,
                     char *dest, int32_t destCapacity,
                     const char *src, int32_t srcLength,
                     UErrorCode *pErrorCode) {
    return ucasemap_mapUTF8(
        csm->caseLocale, csm->options, UCASEMAP_BREAK_ITERATOR_NULL
        (uint8_t *)dest, destCapacity,
        (const uint8_t *)src, srcLength,
        ucasemap_internalUTF8ToLower, NULL, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
ucasemap_utf8ToUpper(const UCaseMap *csm,
                     char *dest, int32_t destCapacity,
                     const char *src, int32_t srcLength,
                     UErrorCode *pErrorCode) {
    return ucasemap_mapUTF8(
        csm->caseLocale, csm->options, UCASEMAP_BREAK_ITERATOR_NULL
        (uint8_t *)dest, destCapacity,
        (const uint8_t *)src, srcLength,
        ucasemap_internalUTF8ToUpper, NULL, *pErrorCode);
}

U_CAPI int32_t U_EXPORT2
ucasemap_utf8FoldCase(const UCaseMap *csm,
                      char *dest, int32_t destCapacity,
                      const char *src, int32_t srcLength,
                      UErrorCode *pErrorCode) {
    return ucasemap_mapUTF8(
        UCASE_LOC_ROOT, csm->options, UCASEMAP_BREAK_ITERATOR_NULL
        (uint8_t *)dest, destCapacity,
        (const uint8_t *)src, srcLength,
        ucasemap_internalUTF8Fold, NULL, *pErrorCode);
}

U_NAMESPACE_BEGIN

int32_t CaseMap::utf8ToLower(
        const char *locale, uint32_t options,
        const char *src, int32_t srcLength,
        char *dest, int32_t destCapacity, Edits *edits,
        UErrorCode &errorCode) {
    return ucasemap_mapUTF8(
        ustrcase_getCaseLocale(locale), options, UCASEMAP_BREAK_ITERATOR_NULL
        (uint8_t *)dest, destCapacity,
        (const uint8_t *)src, srcLength,
        ucasemap_internalUTF8ToLower, edits, errorCode);
}

int32_t CaseMap::utf8ToUpper(
        const char *locale, uint32_t options,
        const char *src, int32_t srcLength,
        char *dest, int32_t destCapacity, Edits *edits,
        UErrorCode &errorCode) {
    return ucasemap_mapUTF8(
        ustrcase_getCaseLocale(locale), options, UCASEMAP_BREAK_ITERATOR_NULL
        (uint8_t *)dest, destCapacity,
        (const uint8_t *)src, srcLength,
        ucasemap_internalUTF8ToUpper, edits, errorCode);
}

int32_t CaseMap::utf8Fold(
        uint32_t options,
        const char *src, int32_t srcLength,
        char *dest, int32_t destCapacity, Edits *edits,
        UErrorCode &errorCode) {
    return ucasemap_mapUTF8(
        UCASE_LOC_ROOT, options, UCASEMAP_BREAK_ITERATOR_NULL
        (uint8_t *)dest, destCapacity,
        (const uint8_t *)src, srcLength,
        ucasemap_internalUTF8Fold, edits, errorCode);
}

U_NAMESPACE_END
