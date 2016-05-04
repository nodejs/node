/*
*******************************************************************************
*
*   Copyright (C) 2001-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ustrcase.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002feb20
*   created by: Markus W. Scherer
*
*   Implementation file for string casing C API functions.
*   Uses functions from uchar.c for basic functionality that requires access
*   to the Unicode Character Database (uprops.dat).
*/

#include "unicode/utypes.h"
#include "unicode/brkiter.h"
#include "unicode/ustring.h"
#include "unicode/ucasemap.h"
#include "unicode/ubrk.h"
#include "unicode/utf.h"
#include "unicode/utf16.h"
#include "cmemory.h"
#include "ucase.h"
#include "ustr_imp.h"
#include "uassert.h"

U_NAMESPACE_USE

/* string casing ------------------------------------------------------------ */

/* Appends a full case mapping result, see UCASE_MAX_STRING_LENGTH. */
static inline int32_t
appendResult(UChar *dest, int32_t destIndex, int32_t destCapacity,
             int32_t result, const UChar *s) {
    UChar32 c;
    int32_t length;

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
            U16_APPEND(dest, destIndex, destCapacity, c, isError);
            if(isError) {
                /* overflow, nothing written */
                destIndex+=U16_LENGTH(c);
            }
        } else {
            /* string */
            if((destIndex+length)<=destCapacity) {
                while(length>0) {
                    dest[destIndex++]=*s++;
                    --length;
                }
            } else {
                /* overflow */
                destIndex+=length;
            }
        }
    } else {
        /* preflight */
        if(length<0) {
            destIndex+=U16_LENGTH(c);
        } else {
            destIndex+=length;
        }
    }
    return destIndex;
}

static UChar32 U_CALLCONV
utf16_caseContextIterator(void *context, int8_t dir) {
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
            U16_PREV((const UChar *)csc->p, csc->start, csc->index, c);
            return c;
        }
    } else {
        if(csc->index<csc->limit) {
            U16_NEXT((const UChar *)csc->p, csc->index, csc->limit, c);
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
         UChar *dest, int32_t destCapacity,
         const UChar *src, UCaseContext *csc,
         int32_t srcStart, int32_t srcLimit,
         UErrorCode *pErrorCode) {
    const UChar *s;
    UChar32 c, c2 = 0;
    int32_t srcIndex, destIndex;
    int32_t locCache;

    locCache=csm->locCache;

    /* case mapping loop */
    srcIndex=srcStart;
    destIndex=0;
    while(srcIndex<srcLimit) {
        csc->cpStart=srcIndex;
        U16_NEXT(src, srcIndex, srcLimit, c);
        csc->cpLimit=srcIndex;
        c=map(csm->csp, c, utf16_caseContextIterator, csc, &s, csm->locale, &locCache);
        if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0xffff : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0xffff)) {
            /* fast path version of appendResult() for BMP results */
            dest[destIndex++]=(UChar)c2;
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
ustrcase_internalToTitle(const UCaseMap *csm,
                         UChar *dest, int32_t destCapacity,
                         const UChar *src, int32_t srcLength,
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
            U16_NEXT(src, titleLimit, idx, c);
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
                    U16_NEXT(src, titleLimit, idx, c);
                    if(UCASE_NONE!=ucase_getType(csm->csp, c)) {
                        break; /* cased letter at [titleStart..titleLimit[ */
                    }
                }
                length=titleStart-prev;
                if(length>0) {
                    if((destIndex+length)<=destCapacity) {
                        uprv_memcpy(dest+destIndex, src+prev, length*U_SIZEOF_UCHAR);
                    }
                    destIndex+=length;
                }
            }

            if(titleStart<titleLimit) {
                /* titlecase c which is from [titleStart..titleLimit[ */
                csc.cpStart=titleStart;
                csc.cpLimit=titleLimit;
                c=ucase_toFullTitle(csm->csp, c, utf16_caseContextIterator, &csc, &s, csm->locale, &locCache);
                destIndex=appendResult(dest, destIndex, destCapacity, c, s); 

                /* Special case Dutch IJ titlecasing */
                if ( titleStart+1 < idx && 
                     ucase_getCaseLocale(csm->locale,&locCache) == UCASE_LOC_DUTCH &&
                     ( src[titleStart] == (UChar32) 0x0049 || src[titleStart] == (UChar32) 0x0069 ) &&
                     ( src[titleStart+1] == (UChar32) 0x004A || src[titleStart+1] == (UChar32) 0x006A )) { 
                            c=(UChar32) 0x004A;
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
                            uprv_memcpy(dest+destIndex, src+titleLimit, length*U_SIZEOF_UCHAR);
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

#endif  // !UCONFIG_NO_BREAK_ITERATION

/* functions available in the common library (for unistr_case.cpp) */

U_CFUNC int32_t U_CALLCONV
ustrcase_internalToLower(const UCaseMap *csm,
                         UChar *dest, int32_t destCapacity,
                         const UChar *src, int32_t srcLength,
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

U_CFUNC int32_t U_CALLCONV
ustrcase_internalToUpper(const UCaseMap *csm,
                         UChar *dest, int32_t destCapacity,
                         const UChar *src, int32_t srcLength,
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
ustr_foldCase(const UCaseProps *csp,
              UChar *dest, int32_t destCapacity,
              const UChar *src, int32_t srcLength,
              uint32_t options,
              UErrorCode *pErrorCode) {
    int32_t srcIndex, destIndex;

    const UChar *s;
    UChar32 c, c2 = 0;

    /* case mapping loop */
    srcIndex=destIndex=0;
    while(srcIndex<srcLength) {
        U16_NEXT(src, srcIndex, srcLength, c);
        c=ucase_toFullFolding(csp, c, &s, options);
        if((destIndex<destCapacity) && (c<0 ? (c2=~c)<=0xffff : UCASE_MAX_STRING_LENGTH<c && (c2=c)<=0xffff)) {
            /* fast path version of appendResult() for BMP results */
            dest[destIndex++]=(UChar)c2;
        } else {
            destIndex=appendResult(dest, destIndex, destCapacity, c, s);
        }
    }

    if(destIndex>destCapacity) {
        *pErrorCode=U_BUFFER_OVERFLOW_ERROR;
    }
    return destIndex;
}

U_CFUNC int32_t U_CALLCONV
ustrcase_internalFold(const UCaseMap *csm,
                      UChar *dest, int32_t destCapacity,
                      const UChar *src, int32_t srcLength,
                      UErrorCode *pErrorCode) {
    return ustr_foldCase(csm->csp, dest, destCapacity, src, srcLength, csm->options, pErrorCode);
}

U_CFUNC int32_t
ustrcase_map(const UCaseMap *csm,
             UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             UStringCaseMapper *stringCaseMapper,
             UErrorCode *pErrorCode) {
    UChar buffer[300];
    UChar *temp;

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
        srcLength=u_strlen(src);
    }

    /* check for overlapping source and destination */
    if( dest!=NULL &&
        ((src>=dest && src<(dest+destCapacity)) ||
         (dest>=src && dest<(src+srcLength)))
    ) {
        /* overlap: provide a temporary destination buffer and later copy the result */
        if(destCapacity<=UPRV_LENGTHOF(buffer)) {
            /* the stack buffer is large enough */
            temp=buffer;
        } else {
            /* allocate a buffer */
            temp=(UChar *)uprv_malloc(destCapacity*U_SIZEOF_UCHAR);
            if(temp==NULL) {
                *pErrorCode=U_MEMORY_ALLOCATION_ERROR;
                return 0;
            }
        }
    } else {
        temp=dest;
    }

    destLength=stringCaseMapper(csm, temp, destCapacity, src, srcLength, pErrorCode);
    if(temp!=dest) {
        /* copy the result string to the destination buffer */
        if(destLength>0) {
            int32_t copyLength= destLength<=destCapacity ? destLength : destCapacity;
            if(copyLength>0) {
                uprv_memmove(dest, temp, copyLength*U_SIZEOF_UCHAR);
            }
        }
        if(temp!=buffer) {
            uprv_free(temp);
        }
    }

    return u_terminateUChars(dest, destCapacity, destLength, pErrorCode);
}

/* public API functions */

U_CAPI int32_t U_EXPORT2
u_strFoldCase(UChar *dest, int32_t destCapacity,
              const UChar *src, int32_t srcLength,
              uint32_t options,
              UErrorCode *pErrorCode) {
    UCaseMap csm=UCASEMAP_INITIALIZER;
    csm.csp=ucase_getSingleton();
    csm.options=options;
    return ustrcase_map(
        &csm,
        dest, destCapacity,
        src, srcLength,
        ustrcase_internalFold, pErrorCode);
}

/* case-insensitive string comparisons -------------------------------------- */

/*
 * This function is a copy of unorm_cmpEquivFold() minus the parts for
 * canonical equivalence.
 * Keep the functions in sync, and see there for how this works.
 * The duplication is for modularization:
 * It makes caseless (but not canonical caseless) matches independent of
 * the normalization code.
 */

/* stack element for previous-level source/decomposition pointers */
struct CmpEquivLevel {
    const UChar *start, *s, *limit;
};
typedef struct CmpEquivLevel CmpEquivLevel;

/**
 * Internal implementation code comparing string with case fold.
 * This function is called from u_strcmpFold() and u_caseInsensitivePrefixMatch().
 *
 * @param s1            input string 1
 * @param length1       length of string 1, or -1 (NULL terminated)
 * @param s2            input string 2
 * @param length2       length of string 2, or -1 (NULL terminated)
 * @param options       compare options
 * @param matchLen1     (output) length of partial prefix match in s1
 * @param matchLen2     (output) length of partial prefix match in s2
 * @param pErrorCode    receives error status
 * @return The result of comparison
 */
static int32_t _cmpFold(
            const UChar *s1, int32_t length1,
            const UChar *s2, int32_t length2,
            uint32_t options,
            int32_t *matchLen1, int32_t *matchLen2,
            UErrorCode *pErrorCode) {
    int32_t cmpRes = 0;

    const UCaseProps *csp;

    /* current-level start/limit - s1/s2 as current */
    const UChar *start1, *start2, *limit1, *limit2;

    /* points to the original start address */
    const UChar *org1, *org2;

    /* points to the end of match + 1 */
    const UChar *m1, *m2;

    /* case folding variables */
    const UChar *p;
    int32_t length;

    /* stacks of previous-level start/current/limit */
    CmpEquivLevel stack1[2], stack2[2];

    /* case folding buffers, only use current-level start/limit */
    UChar fold1[UCASE_MAX_STRING_LENGTH+1], fold2[UCASE_MAX_STRING_LENGTH+1];

    /* track which is the current level per string */
    int32_t level1, level2;

    /* current code units, and code points for lookups */
    UChar32 c1, c2, cp1, cp2;

    /* no argument error checking because this itself is not an API */

    /*
     * assume that at least the option U_COMPARE_IGNORE_CASE is set
     * otherwise this function would have to behave exactly as uprv_strCompare()
     */
    csp=ucase_getSingleton();
    if(U_FAILURE(*pErrorCode)) {
        return 0;
    }

    /* initialize */
    if(matchLen1) {
        U_ASSERT(matchLen2 !=NULL);
        *matchLen1=0;
        *matchLen2=0;
    }

    start1=m1=org1=s1;
    if(length1==-1) {
        limit1=NULL;
    } else {
        limit1=s1+length1;
    }

    start2=m2=org2=s2;
    if(length2==-1) {
        limit2=NULL;
    } else {
        limit2=s2+length2;
    }

    level1=level2=0;
    c1=c2=-1;

    /* comparison loop */
    for(;;) {
        /*
         * here a code unit value of -1 means "get another code unit"
         * below it will mean "this source is finished"
         */

        if(c1<0) {
            /* get next code unit from string 1, post-increment */
            for(;;) {
                if(s1==limit1 || ((c1=*s1)==0 && (limit1==NULL || (options&_STRNCMP_STYLE)))) {
                    if(level1==0) {
                        c1=-1;
                        break;
                    }
                } else {
                    ++s1;
                    break;
                }

                /* reached end of level buffer, pop one level */
                do {
                    --level1;
                    start1=stack1[level1].start;    /*Not uninitialized*/
                } while(start1==NULL);
                s1=stack1[level1].s;                /*Not uninitialized*/
                limit1=stack1[level1].limit;        /*Not uninitialized*/
            }
        }

        if(c2<0) {
            /* get next code unit from string 2, post-increment */
            for(;;) {
                if(s2==limit2 || ((c2=*s2)==0 && (limit2==NULL || (options&_STRNCMP_STYLE)))) {
                    if(level2==0) {
                        c2=-1;
                        break;
                    }
                } else {
                    ++s2;
                    break;
                }

                /* reached end of level buffer, pop one level */
                do {
                    --level2;
                    start2=stack2[level2].start;    /*Not uninitialized*/
                } while(start2==NULL);
                s2=stack2[level2].s;                /*Not uninitialized*/
                limit2=stack2[level2].limit;        /*Not uninitialized*/
            }
        }

        /*
         * compare c1 and c2
         * either variable c1, c2 is -1 only if the corresponding string is finished
         */
        if(c1==c2) {
            const UChar *next1, *next2;

            if(c1<0) {
                cmpRes=0;   /* c1==c2==-1 indicating end of strings */
                break;
            }

            /*
             * Note: Move the match positions in both strings at the same time
             *      only when corresponding code point(s) in the original strings
             *      are fully consumed. For example, when comparing s1="Fust" and
             *      s2="Fu\u00dfball", s2[2] is folded into "ss", and s1[2] matches
             *      the first code point in the case-folded data. But the second "s"
             *      has no matching code point in s1, so this implementation returns
             *      2 as the prefix match length ("Fu").
             */
            next1=next2=NULL;
            if(level1==0) {
                next1=s1;
            } else if(s1==limit1) {
                /* Note: This implementation only use a single level of stack.
                 *      If this code needs to be changed to use multiple levels
                 *      of stacks, the code above should check if the current
                 *      code is at the end of all stacks.
                 */
                U_ASSERT(level1==1);

                /* is s1 at the end of the current stack? */
                next1=stack1[0].s;
            }

            if (next1!=NULL) {
                if(level2==0) {
                    next2=s2;
                } else if(s2==limit2) {
                    U_ASSERT(level2==1);

                    /* is s2 at the end of the current stack? */
                    next2=stack2[0].s;
                }
                if(next2!=NULL) {
                    m1=next1;
                    m2=next2;
                }
            }
            c1=c2=-1;       /* make us fetch new code units */
            continue;
        } else if(c1<0) {
            cmpRes=-1;      /* string 1 ends before string 2 */
            break;
        } else if(c2<0) {
            cmpRes=1;       /* string 2 ends before string 1 */
            break;
        }
        /* c1!=c2 && c1>=0 && c2>=0 */

        /* get complete code points for c1, c2 for lookups if either is a surrogate */
        cp1=c1;
        if(U_IS_SURROGATE(c1)) {
            UChar c;

            if(U_IS_SURROGATE_LEAD(c1)) {
                if(s1!=limit1 && U16_IS_TRAIL(c=*s1)) {
                    /* advance ++s1; only below if cp1 decomposes/case-folds */
                    cp1=U16_GET_SUPPLEMENTARY(c1, c);
                }
            } else /* isTrail(c1) */ {
                if(start1<=(s1-2) && U16_IS_LEAD(c=*(s1-2))) {
                    cp1=U16_GET_SUPPLEMENTARY(c, c1);
                }
            }
        }

        cp2=c2;
        if(U_IS_SURROGATE(c2)) {
            UChar c;

            if(U_IS_SURROGATE_LEAD(c2)) {
                if(s2!=limit2 && U16_IS_TRAIL(c=*s2)) {
                    /* advance ++s2; only below if cp2 decomposes/case-folds */
                    cp2=U16_GET_SUPPLEMENTARY(c2, c);
                }
            } else /* isTrail(c2) */ {
                if(start2<=(s2-2) && U16_IS_LEAD(c=*(s2-2))) {
                    cp2=U16_GET_SUPPLEMENTARY(c, c2);
                }
            }
        }

        /*
         * go down one level for each string
         * continue with the main loop as soon as there is a real change
         */

        if( level1==0 &&
            (length=ucase_toFullFolding(csp, (UChar32)cp1, &p, options))>=0
        ) {
            /* cp1 case-folds to the code point "length" or to p[length] */
            if(U_IS_SURROGATE(c1)) {
                if(U_IS_SURROGATE_LEAD(c1)) {
                    /* advance beyond source surrogate pair if it case-folds */
                    ++s1;
                } else /* isTrail(c1) */ {
                    /*
                     * we got a supplementary code point when hitting its trail surrogate,
                     * therefore the lead surrogate must have been the same as in the other string;
                     * compare this decomposition with the lead surrogate in the other string
                     * remember that this simulates bulk text replacement:
                     * the decomposition would replace the entire code point
                     */
                    --s2;
                    --m2;
                    c2=*(s2-1);
                }
            }

            /* push current level pointers */
            stack1[0].start=start1;
            stack1[0].s=s1;
            stack1[0].limit=limit1;
            ++level1;

            /* copy the folding result to fold1[] */
            if(length<=UCASE_MAX_STRING_LENGTH) {
                u_memcpy(fold1, p, length);
            } else {
                int32_t i=0;
                U16_APPEND_UNSAFE(fold1, i, length);
                length=i;
            }

            /* set next level pointers to case folding */
            start1=s1=fold1;
            limit1=fold1+length;

            /* get ready to read from decomposition, continue with loop */
            c1=-1;
            continue;
        }

        if( level2==0 &&
            (length=ucase_toFullFolding(csp, (UChar32)cp2, &p, options))>=0
        ) {
            /* cp2 case-folds to the code point "length" or to p[length] */
            if(U_IS_SURROGATE(c2)) {
                if(U_IS_SURROGATE_LEAD(c2)) {
                    /* advance beyond source surrogate pair if it case-folds */
                    ++s2;
                } else /* isTrail(c2) */ {
                    /*
                     * we got a supplementary code point when hitting its trail surrogate,
                     * therefore the lead surrogate must have been the same as in the other string;
                     * compare this decomposition with the lead surrogate in the other string
                     * remember that this simulates bulk text replacement:
                     * the decomposition would replace the entire code point
                     */
                    --s1;
                    --m2;
                    c1=*(s1-1);
                }
            }

            /* push current level pointers */
            stack2[0].start=start2;
            stack2[0].s=s2;
            stack2[0].limit=limit2;
            ++level2;

            /* copy the folding result to fold2[] */
            if(length<=UCASE_MAX_STRING_LENGTH) {
                u_memcpy(fold2, p, length);
            } else {
                int32_t i=0;
                U16_APPEND_UNSAFE(fold2, i, length);
                length=i;
            }

            /* set next level pointers to case folding */
            start2=s2=fold2;
            limit2=fold2+length;

            /* get ready to read from decomposition, continue with loop */
            c2=-1;
            continue;
        }

        /*
         * no decomposition/case folding, max level for both sides:
         * return difference result
         *
         * code point order comparison must not just return cp1-cp2
         * because when single surrogates are present then the surrogate pairs
         * that formed cp1 and cp2 may be from different string indexes
         *
         * example: { d800 d800 dc01 } vs. { d800 dc00 }, compare at second code units
         * c1=d800 cp1=10001 c2=dc00 cp2=10000
         * cp1-cp2>0 but c1-c2<0 and in fact in UTF-32 it is { d800 10001 } < { 10000 }
         *
         * therefore, use same fix-up as in ustring.c/uprv_strCompare()
         * except: uprv_strCompare() fetches c=*s while this functions fetches c=*s++
         * so we have slightly different pointer/start/limit comparisons here
         */

        if(c1>=0xd800 && c2>=0xd800 && (options&U_COMPARE_CODE_POINT_ORDER)) {
            /* subtract 0x2800 from BMP code points to make them smaller than supplementary ones */
            if(
                (c1<=0xdbff && s1!=limit1 && U16_IS_TRAIL(*s1)) ||
                (U16_IS_TRAIL(c1) && start1!=(s1-1) && U16_IS_LEAD(*(s1-2)))
            ) {
                /* part of a surrogate pair, leave >=d800 */
            } else {
                /* BMP code point - may be surrogate code point - make <d800 */
                c1-=0x2800;
            }

            if(
                (c2<=0xdbff && s2!=limit2 && U16_IS_TRAIL(*s2)) ||
                (U16_IS_TRAIL(c2) && start2!=(s2-1) && U16_IS_LEAD(*(s2-2)))
            ) {
                /* part of a surrogate pair, leave >=d800 */
            } else {
                /* BMP code point - may be surrogate code point - make <d800 */
                c2-=0x2800;
            }
        }

        cmpRes=c1-c2;
        break;
    }

    if(matchLen1) {
        *matchLen1=m1-org1;
        *matchLen2=m2-org2;
    }
    return cmpRes;
}

/* internal function */
U_CFUNC int32_t
u_strcmpFold(const UChar *s1, int32_t length1,
             const UChar *s2, int32_t length2,
             uint32_t options,
             UErrorCode *pErrorCode) {
    return _cmpFold(s1, length1, s2, length2, options, NULL, NULL, pErrorCode);
}

/* public API functions */

U_CAPI int32_t U_EXPORT2
u_strCaseCompare(const UChar *s1, int32_t length1,
                 const UChar *s2, int32_t length2,
                 uint32_t options,
                 UErrorCode *pErrorCode) {
    /* argument checking */
    if(pErrorCode==0 || U_FAILURE(*pErrorCode)) {
        return 0;
    }
    if(s1==NULL || length1<-1 || s2==NULL || length2<-1) {
        *pErrorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    return u_strcmpFold(s1, length1, s2, length2,
                        options|U_COMPARE_IGNORE_CASE,
                        pErrorCode);
}

U_CAPI int32_t U_EXPORT2
u_strcasecmp(const UChar *s1, const UChar *s2, uint32_t options) {
    UErrorCode errorCode=U_ZERO_ERROR;
    return u_strcmpFold(s1, -1, s2, -1,
                        options|U_COMPARE_IGNORE_CASE,
                        &errorCode);
}

U_CAPI int32_t U_EXPORT2
u_memcasecmp(const UChar *s1, const UChar *s2, int32_t length, uint32_t options) {
    UErrorCode errorCode=U_ZERO_ERROR;
    return u_strcmpFold(s1, length, s2, length,
                        options|U_COMPARE_IGNORE_CASE,
                        &errorCode);
}

U_CAPI int32_t U_EXPORT2
u_strncasecmp(const UChar *s1, const UChar *s2, int32_t n, uint32_t options) {
    UErrorCode errorCode=U_ZERO_ERROR;
    return u_strcmpFold(s1, n, s2, n,
                        options|(U_COMPARE_IGNORE_CASE|_STRNCMP_STYLE),
                        &errorCode);
}

/* internal API - detect length of shared prefix */
U_CAPI void
u_caseInsensitivePrefixMatch(const UChar *s1, int32_t length1,
                             const UChar *s2, int32_t length2,
                             uint32_t options,
                             int32_t *matchLen1, int32_t *matchLen2,
                             UErrorCode *pErrorCode) {
    _cmpFold(s1, length1, s2, length2, options,
        matchLen1, matchLen2, pErrorCode);
}
