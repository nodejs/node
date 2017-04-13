// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 2001-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File ustrtrns.cpp
*
* Modification History:
*
*   Date        Name        Description
*   9/10/2001    Ram    Creation.
******************************************************************************
*/

/*******************************************************************************
 *
 * u_strTo* and u_strFrom* APIs
 * WCS functions moved to ustr_wcs.c for better modularization
 *
 *******************************************************************************
 */


#include "unicode/putil.h"
#include "unicode/ustring.h"
#include "unicode/utf.h"
#include "unicode/utf8.h"
#include "unicode/utf16.h"
#include "cstring.h"
#include "cmemory.h"
#include "ustr_imp.h"
#include "uassert.h"

U_CAPI UChar* U_EXPORT2
u_strFromUTF32WithSub(UChar *dest,
               int32_t destCapacity,
               int32_t *pDestLength,
               const UChar32 *src,
               int32_t srcLength,
               UChar32 subchar, int32_t *pNumSubstitutions,
               UErrorCode *pErrorCode) {
    const UChar32 *srcLimit;
    UChar32 ch;
    UChar *destLimit;
    UChar *pDest;
    int32_t reqLength;
    int32_t numSubstitutions;

    /* args check */
    if(U_FAILURE(*pErrorCode)){
        return NULL;
    }
    if( (src==NULL && srcLength!=0) || srcLength < -1 ||
        (destCapacity<0) || (dest == NULL && destCapacity > 0) ||
        subchar > 0x10ffff || U_IS_SURROGATE(subchar)
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(pNumSubstitutions != NULL) {
        *pNumSubstitutions = 0;
    }

    pDest = dest;
    destLimit = (dest!=NULL)?(dest + destCapacity):NULL;
    reqLength = 0;
    numSubstitutions = 0;

    if(srcLength < 0) {
        /* simple loop for conversion of a NUL-terminated BMP string */
        while((ch=*src) != 0 &&
              ((uint32_t)ch < 0xd800 || (0xe000 <= ch && ch <= 0xffff))) {
            ++src;
            if(pDest < destLimit) {
                *pDest++ = (UChar)ch;
            } else {
                ++reqLength;
            }
        }
        srcLimit = src;
        if(ch != 0) {
            /* "complicated" case, find the end of the remaining string */
            while(*++srcLimit != 0) {}
        }
    } else {
      srcLimit = (src!=NULL)?(src + srcLength):NULL;
    }

    /* convert with length */
    while(src < srcLimit) {
        ch = *src++;
        do {
            /* usually "loops" once; twice only for writing subchar */
            if((uint32_t)ch < 0xd800 || (0xe000 <= ch && ch <= 0xffff)) {
                if(pDest < destLimit) {
                    *pDest++ = (UChar)ch;
                } else {
                    ++reqLength;
                }
                break;
            } else if(0x10000 <= ch && ch <= 0x10ffff) {
                if(pDest!=NULL && ((pDest + 2) <= destLimit)) {
                    *pDest++ = U16_LEAD(ch);
                    *pDest++ = U16_TRAIL(ch);
                } else {
                    reqLength += 2;
                }
                break;
            } else if((ch = subchar) < 0) {
                /* surrogate code point, or not a Unicode code point at all */
                *pErrorCode = U_INVALID_CHAR_FOUND;
                return NULL;
            } else {
                ++numSubstitutions;
            }
        } while(TRUE);
    }

    reqLength += (int32_t)(pDest - dest);
    if(pDestLength) {
        *pDestLength = reqLength;
    }
    if(pNumSubstitutions != NULL) {
        *pNumSubstitutions = numSubstitutions;
    }

    /* Terminate the buffer */
    u_terminateUChars(dest, destCapacity, reqLength, pErrorCode);

    return dest;
}

U_CAPI UChar* U_EXPORT2
u_strFromUTF32(UChar *dest,
               int32_t destCapacity,
               int32_t *pDestLength,
               const UChar32 *src,
               int32_t srcLength,
               UErrorCode *pErrorCode) {
    return u_strFromUTF32WithSub(
            dest, destCapacity, pDestLength,
            src, srcLength,
            U_SENTINEL, NULL,
            pErrorCode);
}

U_CAPI UChar32* U_EXPORT2
u_strToUTF32WithSub(UChar32 *dest,
             int32_t destCapacity,
             int32_t *pDestLength,
             const UChar *src,
             int32_t srcLength,
             UChar32 subchar, int32_t *pNumSubstitutions,
             UErrorCode *pErrorCode) {
    const UChar *srcLimit;
    UChar32 ch;
    UChar ch2;
    UChar32 *destLimit;
    UChar32 *pDest;
    int32_t reqLength;
    int32_t numSubstitutions;

    /* args check */
    if(U_FAILURE(*pErrorCode)){
        return NULL;
    }
    if( (src==NULL && srcLength!=0) || srcLength < -1 ||
        (destCapacity<0) || (dest == NULL && destCapacity > 0) ||
        subchar > 0x10ffff || U_IS_SURROGATE(subchar)
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(pNumSubstitutions != NULL) {
        *pNumSubstitutions = 0;
    }

    pDest = dest;
    destLimit = (dest!=NULL)?(dest + destCapacity):NULL;
    reqLength = 0;
    numSubstitutions = 0;

    if(srcLength < 0) {
        /* simple loop for conversion of a NUL-terminated BMP string */
        while((ch=*src) != 0 && !U16_IS_SURROGATE(ch)) {
            ++src;
            if(pDest < destLimit) {
                *pDest++ = ch;
            } else {
                ++reqLength;
            }
        }
        srcLimit = src;
        if(ch != 0) {
            /* "complicated" case, find the end of the remaining string */
            while(*++srcLimit != 0) {}
        }
    } else {
        srcLimit = (src!=NULL)?(src + srcLength):NULL;
    }

    /* convert with length */
    while(src < srcLimit) {
        ch = *src++;
        if(!U16_IS_SURROGATE(ch)) {
            /* write or count ch below */
        } else if(U16_IS_SURROGATE_LEAD(ch) && src < srcLimit && U16_IS_TRAIL(ch2 = *src)) {
            ++src;
            ch = U16_GET_SUPPLEMENTARY(ch, ch2);
        } else if((ch = subchar) < 0) {
            /* unpaired surrogate */
            *pErrorCode = U_INVALID_CHAR_FOUND;
            return NULL;
        } else {
            ++numSubstitutions;
        }
        if(pDest < destLimit) {
            *pDest++ = ch;
        } else {
            ++reqLength;
        }
    }

    reqLength += (int32_t)(pDest - dest);
    if(pDestLength) {
        *pDestLength = reqLength;
    }
    if(pNumSubstitutions != NULL) {
        *pNumSubstitutions = numSubstitutions;
    }

    /* Terminate the buffer */
    u_terminateUChar32s(dest, destCapacity, reqLength, pErrorCode);

    return dest;
}

U_CAPI UChar32* U_EXPORT2
u_strToUTF32(UChar32 *dest,
             int32_t destCapacity,
             int32_t *pDestLength,
             const UChar *src,
             int32_t srcLength,
             UErrorCode *pErrorCode) {
    return u_strToUTF32WithSub(
            dest, destCapacity, pDestLength,
            src, srcLength,
            U_SENTINEL, NULL,
            pErrorCode);
}

/* for utf8_nextCharSafeBodyTerminated() */
static const UChar32
utf8_minLegal[4]={ 0, 0x80, 0x800, 0x10000 };

/*
 * Version of utf8_nextCharSafeBody() with the following differences:
 * - checks for NUL termination instead of length
 * - works with pointers instead of indexes
 * - always strict (strict==-1)
 *
 * *ps points to after the lead byte and will be moved to after the last trail byte.
 * c is the lead byte.
 * @return the code point, or U_SENTINEL
 */
static UChar32
utf8_nextCharSafeBodyTerminated(const uint8_t **ps, UChar32 c) {
    const uint8_t *s=*ps;
    uint8_t trail, illegal=0;
    uint8_t count=U8_COUNT_TRAIL_BYTES(c);
    U_ASSERT(count<6);
    U8_MASK_LEAD_BYTE((c), count);
    /* count==0 for illegally leading trail bytes and the illegal bytes 0xfe and 0xff */
    switch(count) {
    /* each branch falls through to the next one */
    case 5:
    case 4:
        /* count>=4 is always illegal: no more than 3 trail bytes in Unicode's UTF-8 */
        illegal=1;
        break;
    case 3:
        trail=(uint8_t)(*s++ - 0x80);
        c=(c<<6)|trail;
        if(trail>0x3f || c>=0x110) {
            /* not a trail byte, or code point>0x10ffff (outside Unicode) */
            illegal=1;
            break;
        }
        U_FALLTHROUGH;
    case 2:
        trail=(uint8_t)(*s++ - 0x80);
        if(trail>0x3f) {
            /* not a trail byte */
            illegal=1;
            break;
        }
        c=(c<<6)|trail;
        U_FALLTHROUGH;
    case 1:
        trail=(uint8_t)(*s++ - 0x80);
        if(trail>0x3f) {
            /* not a trail byte */
            illegal=1;
        }
        c=(c<<6)|trail;
        break;
    case 0:
        return U_SENTINEL;
    /* no default branch to optimize switch()  - all values are covered */
    }

    /* correct sequence - all trail bytes have (b7..b6)==(10)? */
    /* illegal is also set if count>=4 */
    if(illegal || c<utf8_minLegal[count] || U_IS_SURROGATE(c)) {
        /* error handling */
        /* don't go beyond this sequence */
        s=*ps;
        while(count>0 && U8_IS_TRAIL(*s)) {
            ++s;
            --count;
        }
        c=U_SENTINEL;
    }
    *ps=s;
    return c;
}

/*
 * Version of utf8_nextCharSafeBody() with the following differences:
 * - works with pointers instead of indexes
 * - always strict (strict==-1)
 *
 * *ps points to after the lead byte and will be moved to after the last trail byte.
 * c is the lead byte.
 * @return the code point, or U_SENTINEL
 */
static UChar32
utf8_nextCharSafeBodyPointer(const uint8_t **ps, const uint8_t *limit, UChar32 c) {
    const uint8_t *s=*ps;
    uint8_t trail, illegal=0;
    uint8_t count=U8_COUNT_TRAIL_BYTES(c);
    if((limit-s)>=count) {
        U8_MASK_LEAD_BYTE((c), count);
        /* count==0 for illegally leading trail bytes and the illegal bytes 0xfe and 0xff */
        switch(count) {
        /* each branch falls through to the next one */
        case 5:
        case 4:
            /* count>=4 is always illegal: no more than 3 trail bytes in Unicode's UTF-8 */
            illegal=1;
            break;
        case 3:
            trail=*s++;
            c=(c<<6)|(trail&0x3f);
            if(c<0x110) {
                illegal|=(trail&0xc0)^0x80;
            } else {
                /* code point>0x10ffff, outside Unicode */
                illegal=1;
                break;
            }
            U_FALLTHROUGH;
        case 2:
            trail=*s++;
            c=(c<<6)|(trail&0x3f);
            illegal|=(trail&0xc0)^0x80;
            U_FALLTHROUGH;
        case 1:
            trail=*s++;
            c=(c<<6)|(trail&0x3f);
            illegal|=(trail&0xc0)^0x80;
            break;
        case 0:
            return U_SENTINEL;
        /* no default branch to optimize switch()  - all values are covered */
        }
    } else {
        illegal=1; /* too few bytes left */
    }

    /* correct sequence - all trail bytes have (b7..b6)==(10)? */
    /* illegal is also set if count>=4 */
    U_ASSERT(illegal || count<UPRV_LENGTHOF(utf8_minLegal));
    if(illegal || c<utf8_minLegal[count] || U_IS_SURROGATE(c)) {
        /* error handling */
        /* don't go beyond this sequence */
        s=*ps;
        while(count>0 && s<limit && U8_IS_TRAIL(*s)) {
            ++s;
            --count;
        }
        c=U_SENTINEL;
    }
    *ps=s;
    return c;
}

U_CAPI UChar* U_EXPORT2
u_strFromUTF8WithSub(UChar *dest,
              int32_t destCapacity,
              int32_t *pDestLength,
              const char* src,
              int32_t srcLength,
              UChar32 subchar, int32_t *pNumSubstitutions,
              UErrorCode *pErrorCode){
    UChar *pDest = dest;
    UChar *pDestLimit = dest+destCapacity;
    UChar32 ch;
    int32_t reqLength = 0;
    const uint8_t* pSrc = (const uint8_t*) src;
    uint8_t t1, t2; /* trail bytes */
    int32_t numSubstitutions;

    /* args check */
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)){
        return NULL;
    }

    if( (src==NULL && srcLength!=0) || srcLength < -1 ||
        (destCapacity<0) || (dest == NULL && destCapacity > 0) ||
        subchar > 0x10ffff || U_IS_SURROGATE(subchar)
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(pNumSubstitutions!=NULL) {
        *pNumSubstitutions=0;
    }
    numSubstitutions=0;

    /*
     * Inline processing of UTF-8 byte sequences:
     *
     * Byte sequences for the most common characters are handled inline in
     * the conversion loops. In order to reduce the path lengths for those
     * characters, the tests are arranged in a kind of binary search.
     * ASCII (<=0x7f) is checked first, followed by the dividing point
     * between 2- and 3-byte sequences (0xe0).
     * The 3-byte branch is tested first to speed up CJK text.
     * The compiler should combine the subtractions for the two tests for 0xe0.
     * Each branch then tests for the other end of its range.
     */

    if(srcLength < 0){
        /*
         * Transform a NUL-terminated string.
         * The code explicitly checks for NULs only in the lead byte position.
         * A NUL byte in the trail byte position fails the trail byte range check anyway.
         */
        while(((ch = *pSrc) != 0) && (pDest < pDestLimit)) {
            if(ch <= 0x7f){
                *pDest++=(UChar)ch;
                ++pSrc;
            } else {
                if(ch > 0xe0) {
                    if( /* handle U+1000..U+CFFF inline */
                        ch <= 0xec &&
                        (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f &&
                        (t2 = (uint8_t)(pSrc[2] - 0x80)) <= 0x3f
                    ) {
                        /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                        *pDest++ = (UChar)((ch << 12) | (t1 << 6) | t2);
                        pSrc += 3;
                        continue;
                    }
                } else if(ch < 0xe0) {
                    if( /* handle U+0080..U+07FF inline */
                        ch >= 0xc2 &&
                        (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f
                    ) {
                        *pDest++ = (UChar)(((ch & 0x1f) << 6) | t1);
                        pSrc += 2;
                        continue;
                    }
                }

                /* function call for "complicated" and error cases */
                ++pSrc; /* continue after the lead byte */
                ch=utf8_nextCharSafeBodyTerminated(&pSrc, ch);
                if(ch<0 && (++numSubstitutions, ch = subchar) < 0) {
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                } else if(ch<=0xFFFF) {
                    *(pDest++)=(UChar)ch;
                } else {
                    *(pDest++)=U16_LEAD(ch);
                    if(pDest<pDestLimit) {
                        *(pDest++)=U16_TRAIL(ch);
                    } else {
                        reqLength++;
                        break;
                    }
                }
            }
        }

        /* Pre-flight the rest of the string. */
        while((ch = *pSrc) != 0) {
            if(ch <= 0x7f){
                ++reqLength;
                ++pSrc;
            } else {
                if(ch > 0xe0) {
                    if( /* handle U+1000..U+CFFF inline */
                        ch <= 0xec &&
                        (uint8_t)(pSrc[1] - 0x80) <= 0x3f &&
                        (uint8_t)(pSrc[2] - 0x80) <= 0x3f
                    ) {
                        ++reqLength;
                        pSrc += 3;
                        continue;
                    }
                } else if(ch < 0xe0) {
                    if( /* handle U+0080..U+07FF inline */
                        ch >= 0xc2 &&
                        (uint8_t)(pSrc[1] - 0x80) <= 0x3f
                    ) {
                        ++reqLength;
                        pSrc += 2;
                        continue;
                    }
                }

                /* function call for "complicated" and error cases */
                ++pSrc; /* continue after the lead byte */
                ch=utf8_nextCharSafeBodyTerminated(&pSrc, ch);
                if(ch<0 && (++numSubstitutions, ch = subchar) < 0) {
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                }
                reqLength += U16_LENGTH(ch);
            }
        }
    } else /* srcLength >= 0 */ {
        const uint8_t *pSrcLimit = pSrc + srcLength;
        int32_t count;

        /* Faster loop without ongoing checking for pSrcLimit and pDestLimit. */
        for(;;) {
            /*
             * Each iteration of the inner loop progresses by at most 3 UTF-8
             * bytes and one UChar, for most characters.
             * For supplementary code points (4 & 2), which are rare,
             * there is an additional adjustment.
             */
            count = (int32_t)(pDestLimit - pDest);
            srcLength = (int32_t)((pSrcLimit - pSrc) / 3);
            if(count > srcLength) {
                count = srcLength; /* min(remaining dest, remaining src/3) */
            }
            if(count < 3) {
                /*
                 * Too much overhead if we get near the end of the string,
                 * continue with the next loop.
                 */
                break;
            }

            do {
                ch = *pSrc;
                if(ch <= 0x7f){
                    *pDest++=(UChar)ch;
                    ++pSrc;
                } else {
                    if(ch > 0xe0) {
                        if( /* handle U+1000..U+CFFF inline */
                            ch <= 0xec &&
                            (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f &&
                            (t2 = (uint8_t)(pSrc[2] - 0x80)) <= 0x3f
                        ) {
                            /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                            *pDest++ = (UChar)((ch << 12) | (t1 << 6) | t2);
                            pSrc += 3;
                            continue;
                        }
                    } else if(ch < 0xe0) {
                        if( /* handle U+0080..U+07FF inline */
                            ch >= 0xc2 &&
                            (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f
                        ) {
                            *pDest++ = (UChar)(((ch & 0x1f) << 6) | t1);
                            pSrc += 2;
                            continue;
                        }
                    }

                    if(ch >= 0xf0 || subchar > 0xffff) {
                        /*
                         * We may read up to six bytes and write up to two UChars,
                         * which we didn't account for with computing count,
                         * so we adjust it here.
                         */
                        if(--count == 0) {
                            break;
                        }
                    }

                    /* function call for "complicated" and error cases */
                    ++pSrc; /* continue after the lead byte */
                    ch=utf8_nextCharSafeBodyPointer(&pSrc, pSrcLimit, ch);
                    if(ch<0 && (++numSubstitutions, ch = subchar) < 0){
                        *pErrorCode = U_INVALID_CHAR_FOUND;
                        return NULL;
                    }else if(ch<=0xFFFF){
                        *(pDest++)=(UChar)ch;
                    }else{
                        *(pDest++)=U16_LEAD(ch);
                        *(pDest++)=U16_TRAIL(ch);
                    }
                }
            } while(--count > 0);
        }

        while((pSrc<pSrcLimit) && (pDest<pDestLimit)) {
            ch = *pSrc;
            if(ch <= 0x7f){
                *pDest++=(UChar)ch;
                ++pSrc;
            } else {
                if(ch > 0xe0) {
                    if( /* handle U+1000..U+CFFF inline */
                        ch <= 0xec &&
                        ((pSrcLimit - pSrc) >= 3) &&
                        (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f &&
                        (t2 = (uint8_t)(pSrc[2] - 0x80)) <= 0x3f
                    ) {
                        /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                        *pDest++ = (UChar)((ch << 12) | (t1 << 6) | t2);
                        pSrc += 3;
                        continue;
                    }
                } else if(ch < 0xe0) {
                    if( /* handle U+0080..U+07FF inline */
                        ch >= 0xc2 &&
                        ((pSrcLimit - pSrc) >= 2) &&
                        (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f
                    ) {
                        *pDest++ = (UChar)(((ch & 0x1f) << 6) | t1);
                        pSrc += 2;
                        continue;
                    }
                }

                /* function call for "complicated" and error cases */
                ++pSrc; /* continue after the lead byte */
                ch=utf8_nextCharSafeBodyPointer(&pSrc, pSrcLimit, ch);
                if(ch<0 && (++numSubstitutions, ch = subchar) < 0){
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                }else if(ch<=0xFFFF){
                    *(pDest++)=(UChar)ch;
                }else{
                    *(pDest++)=U16_LEAD(ch);
                    if(pDest<pDestLimit){
                        *(pDest++)=U16_TRAIL(ch);
                    }else{
                        reqLength++;
                        break;
                    }
                }
            }
        }
        /* do not fill the dest buffer just count the UChars needed */
        while(pSrc < pSrcLimit){
            ch = *pSrc;
            if(ch <= 0x7f){
                reqLength++;
                ++pSrc;
            } else {
                if(ch > 0xe0) {
                    if( /* handle U+1000..U+CFFF inline */
                        ch <= 0xec &&
                        ((pSrcLimit - pSrc) >= 3) &&
                        (uint8_t)(pSrc[1] - 0x80) <= 0x3f &&
                        (uint8_t)(pSrc[2] - 0x80) <= 0x3f
                    ) {
                        reqLength++;
                        pSrc += 3;
                        continue;
                    }
                } else if(ch < 0xe0) {
                    if( /* handle U+0080..U+07FF inline */
                        ch >= 0xc2 &&
                        ((pSrcLimit - pSrc) >= 2) &&
                        (uint8_t)(pSrc[1] - 0x80) <= 0x3f
                    ) {
                        reqLength++;
                        pSrc += 2;
                        continue;
                    }
                }

                /* function call for "complicated" and error cases */
                ++pSrc; /* continue after the lead byte */
                ch=utf8_nextCharSafeBodyPointer(&pSrc, pSrcLimit, ch);
                if(ch<0 && (++numSubstitutions, ch = subchar) < 0){
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                }
                reqLength+=U16_LENGTH(ch);
            }
        }
    }

    reqLength+=(int32_t)(pDest - dest);

    if(pNumSubstitutions!=NULL) {
        *pNumSubstitutions=numSubstitutions;
    }

    if(pDestLength){
        *pDestLength = reqLength;
    }

    /* Terminate the buffer */
    u_terminateUChars(dest,destCapacity,reqLength,pErrorCode);

    return dest;
}

U_CAPI UChar* U_EXPORT2
u_strFromUTF8(UChar *dest,
              int32_t destCapacity,
              int32_t *pDestLength,
              const char* src,
              int32_t srcLength,
              UErrorCode *pErrorCode){
    return u_strFromUTF8WithSub(
            dest, destCapacity, pDestLength,
            src, srcLength,
            U_SENTINEL, NULL,
            pErrorCode);
}

U_CAPI UChar * U_EXPORT2
u_strFromUTF8Lenient(UChar *dest,
                     int32_t destCapacity,
                     int32_t *pDestLength,
                     const char *src,
                     int32_t srcLength,
                     UErrorCode *pErrorCode) {
    UChar *pDest = dest;
    UChar32 ch;
    int32_t reqLength = 0;
    uint8_t* pSrc = (uint8_t*) src;

    /* args check */
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)){
        return NULL;
    }

    if( (src==NULL && srcLength!=0) || srcLength < -1 ||
        (destCapacity<0) || (dest == NULL && destCapacity > 0)
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(srcLength < 0) {
        /* Transform a NUL-terminated string. */
        UChar *pDestLimit = (dest!=NULL)?(dest+destCapacity):NULL;
        uint8_t t1, t2, t3; /* trail bytes */

        while(((ch = *pSrc) != 0) && (pDest < pDestLimit)) {
            if(ch < 0xc0) {
                /*
                 * ASCII, or a trail byte in lead position which is treated like
                 * a single-byte sequence for better character boundary
                 * resynchronization after illegal sequences.
                 */
                *pDest++=(UChar)ch;
                ++pSrc;
                continue;
            } else if(ch < 0xe0) { /* U+0080..U+07FF */
                if((t1 = pSrc[1]) != 0) {
                    /* 0x3080 = (0xc0 << 6) + 0x80 */
                    *pDest++ = (UChar)((ch << 6) + t1 - 0x3080);
                    pSrc += 2;
                    continue;
                }
            } else if(ch < 0xf0) { /* U+0800..U+FFFF */
                if((t1 = pSrc[1]) != 0 && (t2 = pSrc[2]) != 0) {
                    /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                    /* 0x2080 = (0x80 << 6) + 0x80 */
                    *pDest++ = (UChar)((ch << 12) + (t1 << 6) + t2 - 0x2080);
                    pSrc += 3;
                    continue;
                }
            } else /* f0..f4 */ { /* U+10000..U+10FFFF */
                if((t1 = pSrc[1]) != 0 && (t2 = pSrc[2]) != 0 && (t3 = pSrc[3]) != 0) {
                    pSrc += 4;
                    /* 0x3c82080 = (0xf0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
                    ch = (ch << 18) + (t1 << 12) + (t2 << 6) + t3 - 0x3c82080;
                    *(pDest++) = U16_LEAD(ch);
                    if(pDest < pDestLimit) {
                        *(pDest++) = U16_TRAIL(ch);
                    } else {
                        reqLength = 1;
                        break;
                    }
                    continue;
                }
            }

            /* truncated character at the end */
            *pDest++ = 0xfffd;
            while(*++pSrc != 0) {}
            break;
        }

        /* Pre-flight the rest of the string. */
        while((ch = *pSrc) != 0) {
            if(ch < 0xc0) {
                /*
                 * ASCII, or a trail byte in lead position which is treated like
                 * a single-byte sequence for better character boundary
                 * resynchronization after illegal sequences.
                 */
                ++reqLength;
                ++pSrc;
                continue;
            } else if(ch < 0xe0) { /* U+0080..U+07FF */
                if(pSrc[1] != 0) {
                    ++reqLength;
                    pSrc += 2;
                    continue;
                }
            } else if(ch < 0xf0) { /* U+0800..U+FFFF */
                if(pSrc[1] != 0 && pSrc[2] != 0) {
                    ++reqLength;
                    pSrc += 3;
                    continue;
                }
            } else /* f0..f4 */ { /* U+10000..U+10FFFF */
                if(pSrc[1] != 0 && pSrc[2] != 0 && pSrc[3] != 0) {
                    reqLength += 2;
                    pSrc += 4;
                    continue;
                }
            }

            /* truncated character at the end */
            ++reqLength;
            break;
        }
    } else /* srcLength >= 0 */ {
      const uint8_t *pSrcLimit = (pSrc!=NULL)?(pSrc + srcLength):NULL;

        /*
         * This function requires that if srcLength is given, then it must be
         * destCapatity >= srcLength so that we need not check for
         * destination buffer overflow in the loop.
         */
        if(destCapacity < srcLength) {
            if(pDestLength != NULL) {
                *pDestLength = srcLength; /* this likely overestimates the true destLength! */
            }
            *pErrorCode = U_BUFFER_OVERFLOW_ERROR;
            return NULL;
        }

        if((pSrcLimit - pSrc) >= 4) {
            pSrcLimit -= 3; /* temporarily reduce pSrcLimit */

            /* in this loop, we can always access at least 4 bytes, up to pSrc+3 */
            do {
                ch = *pSrc++;
                if(ch < 0xc0) {
                    /*
                     * ASCII, or a trail byte in lead position which is treated like
                     * a single-byte sequence for better character boundary
                     * resynchronization after illegal sequences.
                     */
                    *pDest++=(UChar)ch;
                } else if(ch < 0xe0) { /* U+0080..U+07FF */
                    /* 0x3080 = (0xc0 << 6) + 0x80 */
                    *pDest++ = (UChar)((ch << 6) + *pSrc++ - 0x3080);
                } else if(ch < 0xf0) { /* U+0800..U+FFFF */
                    /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                    /* 0x2080 = (0x80 << 6) + 0x80 */
                    ch = (ch << 12) + (*pSrc++ << 6);
                    *pDest++ = (UChar)(ch + *pSrc++ - 0x2080);
                } else /* f0..f4 */ { /* U+10000..U+10FFFF */
                    /* 0x3c82080 = (0xf0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
                    ch = (ch << 18) + (*pSrc++ << 12);
                    ch += *pSrc++ << 6;
                    ch += *pSrc++ - 0x3c82080;
                    *(pDest++) = U16_LEAD(ch);
                    *(pDest++) = U16_TRAIL(ch);
                }
            } while(pSrc < pSrcLimit);

            pSrcLimit += 3; /* restore original pSrcLimit */
        }

        while(pSrc < pSrcLimit) {
            ch = *pSrc++;
            if(ch < 0xc0) {
                /*
                 * ASCII, or a trail byte in lead position which is treated like
                 * a single-byte sequence for better character boundary
                 * resynchronization after illegal sequences.
                 */
                *pDest++=(UChar)ch;
                continue;
            } else if(ch < 0xe0) { /* U+0080..U+07FF */
                if(pSrc < pSrcLimit) {
                    /* 0x3080 = (0xc0 << 6) + 0x80 */
                    *pDest++ = (UChar)((ch << 6) + *pSrc++ - 0x3080);
                    continue;
                }
            } else if(ch < 0xf0) { /* U+0800..U+FFFF */
                if((pSrcLimit - pSrc) >= 2) {
                    /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                    /* 0x2080 = (0x80 << 6) + 0x80 */
                    ch = (ch << 12) + (*pSrc++ << 6);
                    *pDest++ = (UChar)(ch + *pSrc++ - 0x2080);
                    pSrc += 3;
                    continue;
                }
            } else /* f0..f4 */ { /* U+10000..U+10FFFF */
                if((pSrcLimit - pSrc) >= 3) {
                    /* 0x3c82080 = (0xf0 << 18) + (0x80 << 12) + (0x80 << 6) + 0x80 */
                    ch = (ch << 18) + (*pSrc++ << 12);
                    ch += *pSrc++ << 6;
                    ch += *pSrc++ - 0x3c82080;
                    *(pDest++) = U16_LEAD(ch);
                    *(pDest++) = U16_TRAIL(ch);
                    pSrc += 4;
                    continue;
                }
            }

            /* truncated character at the end */
            *pDest++ = 0xfffd;
            break;
        }
    }

    reqLength+=(int32_t)(pDest - dest);

    if(pDestLength){
        *pDestLength = reqLength;
    }

    /* Terminate the buffer */
    u_terminateUChars(dest,destCapacity,reqLength,pErrorCode);

    return dest;
}

static inline uint8_t *
_appendUTF8(uint8_t *pDest, UChar32 c) {
    /* it is 0<=c<=0x10ffff and not a surrogate if called by a validating function */
    if((c)<=0x7f) {
        *pDest++=(uint8_t)c;
    } else if(c<=0x7ff) {
        *pDest++=(uint8_t)((c>>6)|0xc0);
        *pDest++=(uint8_t)((c&0x3f)|0x80);
    } else if(c<=0xffff) {
        *pDest++=(uint8_t)((c>>12)|0xe0);
        *pDest++=(uint8_t)(((c>>6)&0x3f)|0x80);
        *pDest++=(uint8_t)(((c)&0x3f)|0x80);
    } else /* if((uint32_t)(c)<=0x10ffff) */ {
        *pDest++=(uint8_t)(((c)>>18)|0xf0);
        *pDest++=(uint8_t)((((c)>>12)&0x3f)|0x80);
        *pDest++=(uint8_t)((((c)>>6)&0x3f)|0x80);
        *pDest++=(uint8_t)(((c)&0x3f)|0x80);
    }
    return pDest;
}


U_CAPI char* U_EXPORT2
u_strToUTF8WithSub(char *dest,
            int32_t destCapacity,
            int32_t *pDestLength,
            const UChar *pSrc,
            int32_t srcLength,
            UChar32 subchar, int32_t *pNumSubstitutions,
            UErrorCode *pErrorCode){
    int32_t reqLength=0;
    uint32_t ch=0,ch2=0;
    uint8_t *pDest = (uint8_t *)dest;
    uint8_t *pDestLimit = (pDest!=NULL)?(pDest + destCapacity):NULL;
    int32_t numSubstitutions;

    /* args check */
    if(pErrorCode==NULL || U_FAILURE(*pErrorCode)){
        return NULL;
    }

    if( (pSrc==NULL && srcLength!=0) || srcLength < -1 ||
        (destCapacity<0) || (dest == NULL && destCapacity > 0) ||
        subchar > 0x10ffff || U_IS_SURROGATE(subchar)
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(pNumSubstitutions!=NULL) {
        *pNumSubstitutions=0;
    }
    numSubstitutions=0;

    if(srcLength==-1) {
        while((ch=*pSrc)!=0) {
            ++pSrc;
            if(ch <= 0x7f) {
                if(pDest<pDestLimit) {
                    *pDest++ = (uint8_t)ch;
                } else {
                    reqLength = 1;
                    break;
                }
            } else if(ch <= 0x7ff) {
                if((pDestLimit - pDest) >= 2) {
                    *pDest++=(uint8_t)((ch>>6)|0xc0);
                    *pDest++=(uint8_t)((ch&0x3f)|0x80);
                } else {
                    reqLength = 2;
                    break;
                }
            } else if(ch <= 0xd7ff || ch >= 0xe000) {
                if((pDestLimit - pDest) >= 3) {
                    *pDest++=(uint8_t)((ch>>12)|0xe0);
                    *pDest++=(uint8_t)(((ch>>6)&0x3f)|0x80);
                    *pDest++=(uint8_t)((ch&0x3f)|0x80);
                } else {
                    reqLength = 3;
                    break;
                }
            } else /* ch is a surrogate */ {
                int32_t length;

                /*need not check for NUL because NUL fails U16_IS_TRAIL() anyway*/
                if(U16_IS_SURROGATE_LEAD(ch) && U16_IS_TRAIL(ch2=*pSrc)) {
                    ++pSrc;
                    ch=U16_GET_SUPPLEMENTARY(ch, ch2);
                } else if(subchar>=0) {
                    ch=subchar;
                    ++numSubstitutions;
                } else {
                    /* Unicode 3.2 forbids surrogate code points in UTF-8 */
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                }

                length = U8_LENGTH(ch);
                if((pDestLimit - pDest) >= length) {
                    /* convert and append*/
                    pDest=_appendUTF8(pDest, ch);
                } else {
                    reqLength = length;
                    break;
                }
            }
        }
        while((ch=*pSrc++)!=0) {
            if(ch<=0x7f) {
                ++reqLength;
            } else if(ch<=0x7ff) {
                reqLength+=2;
            } else if(!U16_IS_SURROGATE(ch)) {
                reqLength+=3;
            } else if(U16_IS_SURROGATE_LEAD(ch) && U16_IS_TRAIL(ch2=*pSrc)) {
                ++pSrc;
                reqLength+=4;
            } else if(subchar>=0) {
                reqLength+=U8_LENGTH(subchar);
                ++numSubstitutions;
            } else {
                /* Unicode 3.2 forbids surrogate code points in UTF-8 */
                *pErrorCode = U_INVALID_CHAR_FOUND;
                return NULL;
            }
        }
    } else {
        const UChar *pSrcLimit = (pSrc!=NULL)?(pSrc+srcLength):NULL;
        int32_t count;

        /* Faster loop without ongoing checking for pSrcLimit and pDestLimit. */
        for(;;) {
            /*
             * Each iteration of the inner loop progresses by at most 3 UTF-8
             * bytes and one UChar, for most characters.
             * For supplementary code points (4 & 2), which are rare,
             * there is an additional adjustment.
             */
            count = (int32_t)((pDestLimit - pDest) / 3);
            srcLength = (int32_t)(pSrcLimit - pSrc);
            if(count > srcLength) {
                count = srcLength; /* min(remaining dest/3, remaining src) */
            }
            if(count < 3) {
                /*
                 * Too much overhead if we get near the end of the string,
                 * continue with the next loop.
                 */
                break;
            }
            do {
                ch=*pSrc++;
                if(ch <= 0x7f) {
                    *pDest++ = (uint8_t)ch;
                } else if(ch <= 0x7ff) {
                    *pDest++=(uint8_t)((ch>>6)|0xc0);
                    *pDest++=(uint8_t)((ch&0x3f)|0x80);
                } else if(ch <= 0xd7ff || ch >= 0xe000) {
                    *pDest++=(uint8_t)((ch>>12)|0xe0);
                    *pDest++=(uint8_t)(((ch>>6)&0x3f)|0x80);
                    *pDest++=(uint8_t)((ch&0x3f)|0x80);
                } else /* ch is a surrogate */ {
                    /*
                     * We will read two UChars and probably output four bytes,
                     * which we didn't account for with computing count,
                     * so we adjust it here.
                     */
                    if(--count == 0) {
                        --pSrc; /* undo ch=*pSrc++ for the lead surrogate */
                        break;  /* recompute count */
                    }

                    if(U16_IS_SURROGATE_LEAD(ch) && U16_IS_TRAIL(ch2=*pSrc)) {
                        ++pSrc;
                        ch=U16_GET_SUPPLEMENTARY(ch, ch2);

                        /* writing 4 bytes per 2 UChars is ok */
                        *pDest++=(uint8_t)((ch>>18)|0xf0);
                        *pDest++=(uint8_t)(((ch>>12)&0x3f)|0x80);
                        *pDest++=(uint8_t)(((ch>>6)&0x3f)|0x80);
                        *pDest++=(uint8_t)((ch&0x3f)|0x80);
                    } else  {
                        /* Unicode 3.2 forbids surrogate code points in UTF-8 */
                        if(subchar>=0) {
                            ch=subchar;
                            ++numSubstitutions;
                        } else {
                            *pErrorCode = U_INVALID_CHAR_FOUND;
                            return NULL;
                        }

                        /* convert and append*/
                        pDest=_appendUTF8(pDest, ch);
                    }
                }
            } while(--count > 0);
        }

        while(pSrc<pSrcLimit) {
            ch=*pSrc++;
            if(ch <= 0x7f) {
                if(pDest<pDestLimit) {
                    *pDest++ = (uint8_t)ch;
                } else {
                    reqLength = 1;
                    break;
                }
            } else if(ch <= 0x7ff) {
                if((pDestLimit - pDest) >= 2) {
                    *pDest++=(uint8_t)((ch>>6)|0xc0);
                    *pDest++=(uint8_t)((ch&0x3f)|0x80);
                } else {
                    reqLength = 2;
                    break;
                }
            } else if(ch <= 0xd7ff || ch >= 0xe000) {
                if((pDestLimit - pDest) >= 3) {
                    *pDest++=(uint8_t)((ch>>12)|0xe0);
                    *pDest++=(uint8_t)(((ch>>6)&0x3f)|0x80);
                    *pDest++=(uint8_t)((ch&0x3f)|0x80);
                } else {
                    reqLength = 3;
                    break;
                }
            } else /* ch is a surrogate */ {
                int32_t length;

                if(U16_IS_SURROGATE_LEAD(ch) && pSrc<pSrcLimit && U16_IS_TRAIL(ch2=*pSrc)) {
                    ++pSrc;
                    ch=U16_GET_SUPPLEMENTARY(ch, ch2);
                } else if(subchar>=0) {
                    ch=subchar;
                    ++numSubstitutions;
                } else {
                    /* Unicode 3.2 forbids surrogate code points in UTF-8 */
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                }

                length = U8_LENGTH(ch);
                if((pDestLimit - pDest) >= length) {
                    /* convert and append*/
                    pDest=_appendUTF8(pDest, ch);
                } else {
                    reqLength = length;
                    break;
                }
            }
        }
        while(pSrc<pSrcLimit) {
            ch=*pSrc++;
            if(ch<=0x7f) {
                ++reqLength;
            } else if(ch<=0x7ff) {
                reqLength+=2;
            } else if(!U16_IS_SURROGATE(ch)) {
                reqLength+=3;
            } else if(U16_IS_SURROGATE_LEAD(ch) && pSrc<pSrcLimit && U16_IS_TRAIL(ch2=*pSrc)) {
                ++pSrc;
                reqLength+=4;
            } else if(subchar>=0) {
                reqLength+=U8_LENGTH(subchar);
                ++numSubstitutions;
            } else {
                /* Unicode 3.2 forbids surrogate code points in UTF-8 */
                *pErrorCode = U_INVALID_CHAR_FOUND;
                return NULL;
            }
        }
    }

    reqLength+=(int32_t)(pDest - (uint8_t *)dest);

    if(pNumSubstitutions!=NULL) {
        *pNumSubstitutions=numSubstitutions;
    }

    if(pDestLength){
        *pDestLength = reqLength;
    }

    /* Terminate the buffer */
    u_terminateChars(dest, destCapacity, reqLength, pErrorCode);
    return dest;
}

U_CAPI char* U_EXPORT2
u_strToUTF8(char *dest,
            int32_t destCapacity,
            int32_t *pDestLength,
            const UChar *pSrc,
            int32_t srcLength,
            UErrorCode *pErrorCode){
    return u_strToUTF8WithSub(
            dest, destCapacity, pDestLength,
            pSrc, srcLength,
            U_SENTINEL, NULL,
            pErrorCode);
}

U_CAPI UChar* U_EXPORT2
u_strFromJavaModifiedUTF8WithSub(
        UChar *dest,
        int32_t destCapacity,
        int32_t *pDestLength,
        const char *src,
        int32_t srcLength,
        UChar32 subchar, int32_t *pNumSubstitutions,
        UErrorCode *pErrorCode) {
    UChar *pDest = dest;
    UChar *pDestLimit = dest+destCapacity;
    UChar32 ch;
    int32_t reqLength = 0;
    const uint8_t* pSrc = (const uint8_t*) src;
    const uint8_t *pSrcLimit;
    int32_t count;
    uint8_t t1, t2; /* trail bytes */
    int32_t numSubstitutions;

    /* args check */
    if(U_FAILURE(*pErrorCode)){
        return NULL;
    }
    if( (src==NULL && srcLength!=0) || srcLength < -1 ||
        (dest==NULL && destCapacity!=0) || destCapacity<0 ||
        subchar > 0x10ffff || U_IS_SURROGATE(subchar)
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(pNumSubstitutions!=NULL) {
        *pNumSubstitutions=0;
    }
    numSubstitutions=0;

    if(srcLength < 0) {
        /*
         * Transform a NUL-terminated ASCII string.
         * Handle non-ASCII strings with slower code.
         */
        while(((ch = *pSrc) != 0) && ch <= 0x7f && (pDest < pDestLimit)) {
            *pDest++=(UChar)ch;
            ++pSrc;
        }
        if(ch == 0) {
            reqLength=(int32_t)(pDest - dest);
            if(pDestLength) {
                *pDestLength = reqLength;
            }

            /* Terminate the buffer */
            u_terminateUChars(dest, destCapacity, reqLength, pErrorCode);
            return dest;
        }
        srcLength = uprv_strlen((const char *)pSrc);
    }

    /* Faster loop without ongoing checking for pSrcLimit and pDestLimit. */
    pSrcLimit = (pSrc == NULL) ? NULL : pSrc + srcLength;
    for(;;) {
        count = (int32_t)(pDestLimit - pDest);
        srcLength = (int32_t)(pSrcLimit - pSrc);
        if(count >= srcLength && srcLength > 0 && *pSrc <= 0x7f) {
            /* fast ASCII loop */
            const uint8_t *prevSrc = pSrc;
            int32_t delta;
            while(pSrc < pSrcLimit && (ch = *pSrc) <= 0x7f) {
                *pDest++=(UChar)ch;
                ++pSrc;
            }
            delta = (int32_t)(pSrc - prevSrc);
            count -= delta;
            srcLength -= delta;
        }
        /*
         * Each iteration of the inner loop progresses by at most 3 UTF-8
         * bytes and one UChar.
         */
        srcLength /= 3;
        if(count > srcLength) {
            count = srcLength; /* min(remaining dest, remaining src/3) */
        }
        if(count < 3) {
            /*
             * Too much overhead if we get near the end of the string,
             * continue with the next loop.
             */
            break;
        }
        do {
            ch = *pSrc;
            if(ch <= 0x7f){
                *pDest++=(UChar)ch;
                ++pSrc;
            } else {
                if(ch >= 0xe0) {
                    if( /* handle U+0000..U+FFFF inline */
                        ch <= 0xef &&
                        (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f &&
                        (t2 = (uint8_t)(pSrc[2] - 0x80)) <= 0x3f
                    ) {
                        /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                        *pDest++ = (UChar)((ch << 12) | (t1 << 6) | t2);
                        pSrc += 3;
                        continue;
                    }
                } else {
                    if( /* handle U+0000..U+07FF inline */
                        ch >= 0xc0 &&
                        (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f
                    ) {
                        *pDest++ = (UChar)(((ch & 0x1f) << 6) | t1);
                        pSrc += 2;
                        continue;
                    }
                }

                if(subchar < 0) {
                    *pErrorCode = U_INVALID_CHAR_FOUND;
                    return NULL;
                } else if(subchar > 0xffff && --count == 0) {
                    /*
                     * We need to write two UChars, adjusted count for that,
                     * and ran out of space.
                     */
                    break;
                } else {
                    /* function call for error cases */
                    ++pSrc; /* continue after the lead byte */
                    utf8_nextCharSafeBodyPointer(&pSrc, pSrcLimit, ch);
                    ++numSubstitutions;
                    if(subchar<=0xFFFF) {
                        *(pDest++)=(UChar)subchar;
                    } else {
                        *(pDest++)=U16_LEAD(subchar);
                        *(pDest++)=U16_TRAIL(subchar);
                    }
                }
            }
        } while(--count > 0);
    }

    while((pSrc<pSrcLimit) && (pDest<pDestLimit)) {
        ch = *pSrc;
        if(ch <= 0x7f){
            *pDest++=(UChar)ch;
            ++pSrc;
        } else {
            if(ch >= 0xe0) {
                if( /* handle U+0000..U+FFFF inline */
                    ch <= 0xef &&
                    ((pSrcLimit - pSrc) >= 3) &&
                    (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f &&
                    (t2 = (uint8_t)(pSrc[2] - 0x80)) <= 0x3f
                ) {
                    /* no need for (ch & 0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */
                    *pDest++ = (UChar)((ch << 12) | (t1 << 6) | t2);
                    pSrc += 3;
                    continue;
                }
            } else {
                if( /* handle U+0000..U+07FF inline */
                    ch >= 0xc0 &&
                    ((pSrcLimit - pSrc) >= 2) &&
                    (t1 = (uint8_t)(pSrc[1] - 0x80)) <= 0x3f
                ) {
                    *pDest++ = (UChar)(((ch & 0x1f) << 6) | t1);
                    pSrc += 2;
                    continue;
                }
            }

            if(subchar < 0) {
                *pErrorCode = U_INVALID_CHAR_FOUND;
                return NULL;
            } else {
                /* function call for error cases */
                ++pSrc; /* continue after the lead byte */
                utf8_nextCharSafeBodyPointer(&pSrc, pSrcLimit, ch);
                ++numSubstitutions;
                if(subchar<=0xFFFF) {
                    *(pDest++)=(UChar)subchar;
                } else {
                    *(pDest++)=U16_LEAD(subchar);
                    if(pDest<pDestLimit) {
                        *(pDest++)=U16_TRAIL(subchar);
                    } else {
                        reqLength++;
                        break;
                    }
                }
            }
        }
    }

    /* do not fill the dest buffer just count the UChars needed */
    while(pSrc < pSrcLimit){
        ch = *pSrc;
        if(ch <= 0x7f) {
            reqLength++;
            ++pSrc;
        } else {
            if(ch >= 0xe0) {
                if( /* handle U+0000..U+FFFF inline */
                    ch <= 0xef &&
                    ((pSrcLimit - pSrc) >= 3) &&
                    (uint8_t)(pSrc[1] - 0x80) <= 0x3f &&
                    (uint8_t)(pSrc[2] - 0x80) <= 0x3f
                ) {
                    reqLength++;
                    pSrc += 3;
                    continue;
                }
            } else {
                if( /* handle U+0000..U+07FF inline */
                    ch >= 0xc0 &&
                    ((pSrcLimit - pSrc) >= 2) &&
                    (uint8_t)(pSrc[1] - 0x80) <= 0x3f
                ) {
                    reqLength++;
                    pSrc += 2;
                    continue;
                }
            }

            if(subchar < 0) {
                *pErrorCode = U_INVALID_CHAR_FOUND;
                return NULL;
            } else {
                /* function call for error cases */
                ++pSrc; /* continue after the lead byte */
                utf8_nextCharSafeBodyPointer(&pSrc, pSrcLimit, ch);
                ++numSubstitutions;
                reqLength+=U16_LENGTH(ch);
            }
        }
    }

    if(pNumSubstitutions!=NULL) {
        *pNumSubstitutions=numSubstitutions;
    }

    reqLength+=(int32_t)(pDest - dest);
    if(pDestLength) {
        *pDestLength = reqLength;
    }

    /* Terminate the buffer */
    u_terminateUChars(dest, destCapacity, reqLength, pErrorCode);
    return dest;
}

U_CAPI char* U_EXPORT2
u_strToJavaModifiedUTF8(
        char *dest,
        int32_t destCapacity,
        int32_t *pDestLength,
        const UChar *src,
        int32_t srcLength,
        UErrorCode *pErrorCode) {
    int32_t reqLength=0;
    uint32_t ch=0;
    uint8_t *pDest = (uint8_t *)dest;
    uint8_t *pDestLimit = pDest + destCapacity;
    const UChar *pSrcLimit;
    int32_t count;

    /* args check */
    if(U_FAILURE(*pErrorCode)){
        return NULL;
    }
    if( (src==NULL && srcLength!=0) || srcLength < -1 ||
        (dest==NULL && destCapacity!=0) || destCapacity<0
    ) {
        *pErrorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    if(srcLength==-1) {
        /* Convert NUL-terminated ASCII, then find the string length. */
        while((ch=*src)<=0x7f && ch != 0 && pDest<pDestLimit) {
            *pDest++ = (uint8_t)ch;
            ++src;
        }
        if(ch == 0) {
            reqLength=(int32_t)(pDest - (uint8_t *)dest);
            if(pDestLength) {
                *pDestLength = reqLength;
            }

            /* Terminate the buffer */
            u_terminateChars(dest, destCapacity, reqLength, pErrorCode);
            return dest;
        }
        srcLength = u_strlen(src);
    }

    /* Faster loop without ongoing checking for pSrcLimit and pDestLimit. */
    pSrcLimit = (src!=NULL)?(src+srcLength):NULL;
    for(;;) {
        count = (int32_t)(pDestLimit - pDest);
        srcLength = (int32_t)(pSrcLimit - src);
        if(count >= srcLength && srcLength > 0 && *src <= 0x7f) {
            /* fast ASCII loop */
            const UChar *prevSrc = src;
            int32_t delta;
            while(src < pSrcLimit && (ch = *src) <= 0x7f && ch != 0) {
                *pDest++=(uint8_t)ch;
                ++src;
            }
            delta = (int32_t)(src - prevSrc);
            count -= delta;
            srcLength -= delta;
        }
        /*
         * Each iteration of the inner loop progresses by at most 3 UTF-8
         * bytes and one UChar.
         */
        count /= 3;
        if(count > srcLength) {
            count = srcLength; /* min(remaining dest/3, remaining src) */
        }
        if(count < 3) {
            /*
             * Too much overhead if we get near the end of the string,
             * continue with the next loop.
             */
            break;
        }
        do {
            ch=*src++;
            if(ch <= 0x7f && ch != 0) {
                *pDest++ = (uint8_t)ch;
            } else if(ch <= 0x7ff) {
                *pDest++=(uint8_t)((ch>>6)|0xc0);
                *pDest++=(uint8_t)((ch&0x3f)|0x80);
            } else {
                *pDest++=(uint8_t)((ch>>12)|0xe0);
                *pDest++=(uint8_t)(((ch>>6)&0x3f)|0x80);
                *pDest++=(uint8_t)((ch&0x3f)|0x80);
            }
        } while(--count > 0);
    }

    while(src<pSrcLimit) {
        ch=*src++;
        if(ch <= 0x7f && ch != 0) {
            if(pDest<pDestLimit) {
                *pDest++ = (uint8_t)ch;
            } else {
                reqLength = 1;
                break;
            }
        } else if(ch <= 0x7ff) {
            if((pDestLimit - pDest) >= 2) {
                *pDest++=(uint8_t)((ch>>6)|0xc0);
                *pDest++=(uint8_t)((ch&0x3f)|0x80);
            } else {
                reqLength = 2;
                break;
            }
        } else {
            if((pDestLimit - pDest) >= 3) {
                *pDest++=(uint8_t)((ch>>12)|0xe0);
                *pDest++=(uint8_t)(((ch>>6)&0x3f)|0x80);
                *pDest++=(uint8_t)((ch&0x3f)|0x80);
            } else {
                reqLength = 3;
                break;
            }
        }
    }
    while(src<pSrcLimit) {
        ch=*src++;
        if(ch <= 0x7f && ch != 0) {
            ++reqLength;
        } else if(ch<=0x7ff) {
            reqLength+=2;
        } else {
            reqLength+=3;
        }
    }

    reqLength+=(int32_t)(pDest - (uint8_t *)dest);
    if(pDestLength){
        *pDestLength = reqLength;
    }

    /* Terminate the buffer */
    u_terminateChars(dest, destCapacity, reqLength, pErrorCode);
    return dest;
}
