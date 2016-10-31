// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1999-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utf8.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999sep13
*   created by: Markus W. Scherer
*/

/**
 * \file
 * \brief C API: 8-bit Unicode handling macros
 * 
 * This file defines macros to deal with 8-bit Unicode (UTF-8) code units (bytes) and strings.
 *
 * For more information see utf.h and the ICU User Guide Strings chapter
 * (http://userguide.icu-project.org/strings).
 *
 * <em>Usage:</em>
 * ICU coding guidelines for if() statements should be followed when using these macros.
 * Compound statements (curly braces {}) must be used  for if-else-while... 
 * bodies and all macro statements should be terminated with semicolon.
 */

#ifndef __UTF8_H__
#define __UTF8_H__

#include "unicode/umachine.h"
#ifndef __UTF_H__
#   include "unicode/utf.h"
#endif

/* internal definitions ----------------------------------------------------- */

/**
 * \var utf8_countTrailBytes
 * Internal array with numbers of trail bytes for any given byte used in
 * lead byte position.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is called by public macros in this file and thus must remain stable,
 * and should not be hidden when other internal functions are hidden (otherwise
 * public macros would fail to compile).
 * @internal
 */
#ifdef U_UTF8_IMPL
U_EXPORT const uint8_t 
#elif defined(U_STATIC_IMPLEMENTATION) || defined(U_COMMON_IMPLEMENTATION)
U_CFUNC const uint8_t
#else
U_CFUNC U_IMPORT const uint8_t /* U_IMPORT2? */ /*U_IMPORT*/ 
#endif
utf8_countTrailBytes[256];

/**
 * Counts the trail bytes for a UTF-8 lead byte.
 * Returns 0 for 0..0xbf as well as for 0xfe and 0xff.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is called by public macros in this file and thus must remain stable.
 *
 * Note: Beginning with ICU 50, the implementation uses a multi-condition expression
 * which was shown in 2012 (on x86-64) to compile to fast, branch-free code.
 * leadByte is evaluated multiple times.
 *
 * The pre-ICU 50 implementation used the exported array utf8_countTrailBytes:
 * #define U8_COUNT_TRAIL_BYTES(leadByte) (utf8_countTrailBytes[leadByte])
 * leadByte was evaluated exactly once.
 *
 * @param leadByte The first byte of a UTF-8 sequence. Must be 0..0xff.
 * @internal
 */
#define U8_COUNT_TRAIL_BYTES(leadByte) \
    ((uint8_t)(leadByte)<0xf0 ? \
        ((uint8_t)(leadByte)>=0xc0)+((uint8_t)(leadByte)>=0xe0) : \
        (uint8_t)(leadByte)<0xfe ? 3+((uint8_t)(leadByte)>=0xf8)+((uint8_t)(leadByte)>=0xfc) : 0)

/**
 * Counts the trail bytes for a UTF-8 lead byte of a valid UTF-8 sequence.
 * The maximum supported lead byte is 0xf4 corresponding to U+10FFFF.
 * leadByte might be evaluated multiple times.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is called by public macros in this file and thus must remain stable.
 *
 * @param leadByte The first byte of a UTF-8 sequence. Must be 0..0xff.
 * @internal
 */
#define U8_COUNT_TRAIL_BYTES_UNSAFE(leadByte) \
    (((leadByte)>=0xc0)+((leadByte)>=0xe0)+((leadByte)>=0xf0))

/**
 * Mask a UTF-8 lead byte, leave only the lower bits that form part of the code point value.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is called by public macros in this file and thus must remain stable.
 * @internal
 */
#define U8_MASK_LEAD_BYTE(leadByte, countTrailBytes) ((leadByte)&=(1<<(6-(countTrailBytes)))-1)

/**
 * Function for handling "next code point" with error-checking.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is U_STABLE (not U_INTERNAL) since it is called by public macros in this
 * file and thus must remain stable, and should not be hidden when other internal
 * functions are hidden (otherwise public macros would fail to compile).
 * @internal
 */
U_STABLE UChar32 U_EXPORT2
utf8_nextCharSafeBody(const uint8_t *s, int32_t *pi, int32_t length, UChar32 c, UBool strict);

/**
 * Function for handling "append code point" with error-checking.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is U_STABLE (not U_INTERNAL) since it is called by public macros in this
 * file and thus must remain stable, and should not be hidden when other internal
 * functions are hidden (otherwise public macros would fail to compile).
 * @internal
 */
U_STABLE int32_t U_EXPORT2
utf8_appendCharSafeBody(uint8_t *s, int32_t i, int32_t length, UChar32 c, UBool *pIsError);

/**
 * Function for handling "previous code point" with error-checking.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is U_STABLE (not U_INTERNAL) since it is called by public macros in this
 * file and thus must remain stable, and should not be hidden when other internal
 * functions are hidden (otherwise public macros would fail to compile).
 * @internal
 */
U_STABLE UChar32 U_EXPORT2
utf8_prevCharSafeBody(const uint8_t *s, int32_t start, int32_t *pi, UChar32 c, UBool strict);

/**
 * Function for handling "skip backward one code point" with error-checking.
 *
 * This is internal since it is not meant to be called directly by external clients;
 * however it is U_STABLE (not U_INTERNAL) since it is called by public macros in this
 * file and thus must remain stable, and should not be hidden when other internal
 * functions are hidden (otherwise public macros would fail to compile).
 * @internal
 */
U_STABLE int32_t U_EXPORT2
utf8_back1SafeBody(const uint8_t *s, int32_t start, int32_t i);

/* single-code point definitions -------------------------------------------- */

/**
 * Does this code unit (byte) encode a code point by itself (US-ASCII 0..0x7f)?
 * @param c 8-bit code unit (byte)
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U8_IS_SINGLE(c) (((c)&0x80)==0)

/**
 * Is this code unit (byte) a UTF-8 lead byte?
 * @param c 8-bit code unit (byte)
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U8_IS_LEAD(c) ((uint8_t)((c)-0xc0)<0x3e)

/**
 * Is this code unit (byte) a UTF-8 trail byte?
 * @param c 8-bit code unit (byte)
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U8_IS_TRAIL(c) (((c)&0xc0)==0x80)

/**
 * How many code units (bytes) are used for the UTF-8 encoding
 * of this Unicode code point?
 * @param c 32-bit code point
 * @return 1..4, or 0 if c is a surrogate or not a Unicode code point
 * @stable ICU 2.4
 */
#define U8_LENGTH(c) \
    ((uint32_t)(c)<=0x7f ? 1 : \
        ((uint32_t)(c)<=0x7ff ? 2 : \
            ((uint32_t)(c)<=0xd7ff ? 3 : \
                ((uint32_t)(c)<=0xdfff || (uint32_t)(c)>0x10ffff ? 0 : \
                    ((uint32_t)(c)<=0xffff ? 3 : 4)\
                ) \
            ) \
        ) \
    )

/**
 * The maximum number of UTF-8 code units (bytes) per Unicode code point (U+0000..U+10ffff).
 * @return 4
 * @stable ICU 2.4
 */
#define U8_MAX_LENGTH 4

/**
 * Get a code point from a string at a random-access offset,
 * without changing the offset.
 * The offset may point to either the lead byte or one of the trail bytes
 * for a code point, in which case the macro will read all of the bytes
 * for the code point.
 * The result is undefined if the offset points to an illegal UTF-8
 * byte sequence.
 * Iteration through a string is more efficient with U8_NEXT_UNSAFE or U8_NEXT.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @param c output UChar32 variable
 * @see U8_GET
 * @stable ICU 2.4
 */
#define U8_GET_UNSAFE(s, i, c) { \
    int32_t _u8_get_unsafe_index=(int32_t)(i); \
    U8_SET_CP_START_UNSAFE(s, _u8_get_unsafe_index); \
    U8_NEXT_UNSAFE(s, _u8_get_unsafe_index, c); \
}

/**
 * Get a code point from a string at a random-access offset,
 * without changing the offset.
 * The offset may point to either the lead byte or one of the trail bytes
 * for a code point, in which case the macro will read all of the bytes
 * for the code point.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * If the offset points to an illegal UTF-8 byte sequence, then
 * c is set to a negative value.
 * Iteration through a string is more efficient with U8_NEXT_UNSAFE or U8_NEXT.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset
 * @param i int32_t string offset, must be start<=i<length
 * @param length int32_t string length
 * @param c output UChar32 variable, set to <0 in case of an error
 * @see U8_GET_UNSAFE
 * @stable ICU 2.4
 */
#define U8_GET(s, start, i, length, c) { \
    int32_t _u8_get_index=(i); \
    U8_SET_CP_START(s, start, _u8_get_index); \
    U8_NEXT(s, _u8_get_index, length, c); \
}

/**
 * Get a code point from a string at a random-access offset,
 * without changing the offset.
 * The offset may point to either the lead byte or one of the trail bytes
 * for a code point, in which case the macro will read all of the bytes
 * for the code point.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * If the offset points to an illegal UTF-8 byte sequence, then
 * c is set to U+FFFD.
 * Iteration through a string is more efficient with U8_NEXT_UNSAFE or U8_NEXT_OR_FFFD.
 *
 * This macro does not distinguish between a real U+FFFD in the text
 * and U+FFFD returned for an ill-formed sequence.
 * Use U8_GET() if that distinction is important.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset
 * @param i int32_t string offset, must be start<=i<length
 * @param length int32_t string length
 * @param c output UChar32 variable, set to U+FFFD in case of an error
 * @see U8_GET
 * @stable ICU 51
 */
#define U8_GET_OR_FFFD(s, start, i, length, c) { \
    int32_t _u8_get_index=(i); \
    U8_SET_CP_START(s, start, _u8_get_index); \
    U8_NEXT_OR_FFFD(s, _u8_get_index, length, c); \
}

/* definitions with forward iteration --------------------------------------- */

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * The offset may point to the lead byte of a multi-byte sequence,
 * in which case the macro will read the whole sequence.
 * The result is undefined if the offset points to a trail byte
 * or an illegal UTF-8 sequence.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @param c output UChar32 variable
 * @see U8_NEXT
 * @stable ICU 2.4
 */
#define U8_NEXT_UNSAFE(s, i, c) { \
    (c)=(uint8_t)(s)[(i)++]; \
    if((c)>=0x80) { \
        if((c)<0xe0) { \
            (c)=(((c)&0x1f)<<6)|((s)[(i)++]&0x3f); \
        } else if((c)<0xf0) { \
            /* no need for (c&0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */ \
            (c)=(UChar)(((c)<<12)|(((s)[i]&0x3f)<<6)|((s)[(i)+1]&0x3f)); \
            (i)+=2; \
        } else { \
            (c)=(((c)&7)<<18)|(((s)[i]&0x3f)<<12)|(((s)[(i)+1]&0x3f)<<6)|((s)[(i)+2]&0x3f); \
            (i)+=3; \
        } \
    } \
}

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * The offset may point to the lead byte of a multi-byte sequence,
 * in which case the macro will read the whole sequence.
 * If the offset points to a trail byte or an illegal UTF-8 sequence, then
 * c is set to a negative value.
 *
 * @param s const uint8_t * string
 * @param i int32_t string offset, must be i<length
 * @param length int32_t string length
 * @param c output UChar32 variable, set to <0 in case of an error
 * @see U8_NEXT_UNSAFE
 * @stable ICU 2.4
 */
#define U8_NEXT(s, i, length, c) { \
    (c)=(uint8_t)(s)[(i)++]; \
    if((c)>=0x80) { \
        uint8_t __t1, __t2; \
        if( /* handle U+1000..U+CFFF inline */ \
            (0xe0<(c) && (c)<=0xec) && \
            (((i)+1)<(length) || (length)<0) && \
            (__t1=(uint8_t)((s)[i]-0x80))<=0x3f && \
            (__t2=(uint8_t)((s)[(i)+1]-0x80))<= 0x3f \
        ) { \
            /* no need for (c&0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */ \
            (c)=(UChar)(((c)<<12)|(__t1<<6)|__t2); \
            (i)+=2; \
        } else if( /* handle U+0080..U+07FF inline */ \
            ((c)<0xe0 && (c)>=0xc2) && \
            ((i)!=(length)) && \
            (__t1=(uint8_t)((s)[i]-0x80))<=0x3f \
        ) { \
            (c)=(((c)&0x1f)<<6)|__t1; \
            ++(i); \
        } else { \
            /* function call for "complicated" and error cases */ \
            (c)=utf8_nextCharSafeBody((const uint8_t *)s, &(i), (length), c, -1); \
        } \
    } \
}

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * The offset may point to the lead byte of a multi-byte sequence,
 * in which case the macro will read the whole sequence.
 * If the offset points to a trail byte or an illegal UTF-8 sequence, then
 * c is set to U+FFFD.
 *
 * This macro does not distinguish between a real U+FFFD in the text
 * and U+FFFD returned for an ill-formed sequence.
 * Use U8_NEXT() if that distinction is important.
 *
 * @param s const uint8_t * string
 * @param i int32_t string offset, must be i<length
 * @param length int32_t string length
 * @param c output UChar32 variable, set to U+FFFD in case of an error
 * @see U8_NEXT
 * @stable ICU 51
 */
#define U8_NEXT_OR_FFFD(s, i, length, c) { \
    (c)=(uint8_t)(s)[(i)++]; \
    if((c)>=0x80) { \
        uint8_t __t1, __t2; \
        if( /* handle U+1000..U+CFFF inline */ \
            (0xe0<(c) && (c)<=0xec) && \
            (((i)+1)<(length) || (length)<0) && \
            (__t1=(uint8_t)((s)[i]-0x80))<=0x3f && \
            (__t2=(uint8_t)((s)[(i)+1]-0x80))<= 0x3f \
        ) { \
            /* no need for (c&0xf) because the upper bits are truncated after <<12 in the cast to (UChar) */ \
            (c)=(UChar)(((c)<<12)|(__t1<<6)|__t2); \
            (i)+=2; \
        } else if( /* handle U+0080..U+07FF inline */ \
            ((c)<0xe0 && (c)>=0xc2) && \
            ((i)!=(length)) && \
            (__t1=(uint8_t)((s)[i]-0x80))<=0x3f \
        ) { \
            (c)=(((c)&0x1f)<<6)|__t1; \
            ++(i); \
        } else { \
            /* function call for "complicated" and error cases */ \
            (c)=utf8_nextCharSafeBody((const uint8_t *)s, &(i), (length), c, -3); \
        } \
    } \
}

/**
 * Append a code point to a string, overwriting 1 to 4 bytes.
 * The offset points to the current end of the string contents
 * and is advanced (post-increment).
 * "Unsafe" macro, assumes a valid code point and sufficient space in the string.
 * Otherwise, the result is undefined.
 *
 * @param s const uint8_t * string buffer
 * @param i string offset
 * @param c code point to append
 * @see U8_APPEND
 * @stable ICU 2.4
 */
#define U8_APPEND_UNSAFE(s, i, c) { \
    if((uint32_t)(c)<=0x7f) { \
        (s)[(i)++]=(uint8_t)(c); \
    } else { \
        if((uint32_t)(c)<=0x7ff) { \
            (s)[(i)++]=(uint8_t)(((c)>>6)|0xc0); \
        } else { \
            if((uint32_t)(c)<=0xffff) { \
                (s)[(i)++]=(uint8_t)(((c)>>12)|0xe0); \
            } else { \
                (s)[(i)++]=(uint8_t)(((c)>>18)|0xf0); \
                (s)[(i)++]=(uint8_t)((((c)>>12)&0x3f)|0x80); \
            } \
            (s)[(i)++]=(uint8_t)((((c)>>6)&0x3f)|0x80); \
        } \
        (s)[(i)++]=(uint8_t)(((c)&0x3f)|0x80); \
    } \
}

/**
 * Append a code point to a string, overwriting 1 to 4 bytes.
 * The offset points to the current end of the string contents
 * and is advanced (post-increment).
 * "Safe" macro, checks for a valid code point.
 * If a non-ASCII code point is written, checks for sufficient space in the string.
 * If the code point is not valid or trail bytes do not fit,
 * then isError is set to TRUE.
 *
 * @param s const uint8_t * string buffer
 * @param i int32_t string offset, must be i<capacity
 * @param capacity int32_t size of the string buffer
 * @param c UChar32 code point to append
 * @param isError output UBool set to TRUE if an error occurs, otherwise not modified
 * @see U8_APPEND_UNSAFE
 * @stable ICU 2.4
 */
#define U8_APPEND(s, i, capacity, c, isError) { \
    if((uint32_t)(c)<=0x7f) { \
        (s)[(i)++]=(uint8_t)(c); \
    } else if((uint32_t)(c)<=0x7ff && (i)+1<(capacity)) { \
        (s)[(i)++]=(uint8_t)(((c)>>6)|0xc0); \
        (s)[(i)++]=(uint8_t)(((c)&0x3f)|0x80); \
    } else if((uint32_t)(c)<=0xd7ff && (i)+2<(capacity)) { \
        (s)[(i)++]=(uint8_t)(((c)>>12)|0xe0); \
        (s)[(i)++]=(uint8_t)((((c)>>6)&0x3f)|0x80); \
        (s)[(i)++]=(uint8_t)(((c)&0x3f)|0x80); \
    } else { \
        (i)=utf8_appendCharSafeBody(s, (i), (capacity), c, &(isError)); \
    } \
}

/**
 * Advance the string offset from one code point boundary to the next.
 * (Post-incrementing iteration.)
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @see U8_FWD_1
 * @stable ICU 2.4
 */
#define U8_FWD_1_UNSAFE(s, i) { \
    (i)+=1+U8_COUNT_TRAIL_BYTES_UNSAFE((uint8_t)(s)[i]); \
}

/**
 * Advance the string offset from one code point boundary to the next.
 * (Post-incrementing iteration.)
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * @param s const uint8_t * string
 * @param i int32_t string offset, must be i<length
 * @param length int32_t string length
 * @see U8_FWD_1_UNSAFE
 * @stable ICU 2.4
 */
#define U8_FWD_1(s, i, length) { \
    uint8_t __b=(uint8_t)(s)[(i)++]; \
    if(U8_IS_LEAD(__b)) { \
        uint8_t __count=U8_COUNT_TRAIL_BYTES(__b); \
        if((i)+__count>(length) && (length)>=0) { \
            __count=(uint8_t)((length)-(i)); \
        } \
        while(__count>0 && U8_IS_TRAIL((s)[i])) { \
            ++(i); \
            --__count; \
        } \
    } \
}

/**
 * Advance the string offset from one code point boundary to the n-th next one,
 * i.e., move forward by n code points.
 * (Post-incrementing iteration.)
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @param n number of code points to skip
 * @see U8_FWD_N
 * @stable ICU 2.4
 */
#define U8_FWD_N_UNSAFE(s, i, n) { \
    int32_t __N=(n); \
    while(__N>0) { \
        U8_FWD_1_UNSAFE(s, i); \
        --__N; \
    } \
}

/**
 * Advance the string offset from one code point boundary to the n-th next one,
 * i.e., move forward by n code points.
 * (Post-incrementing iteration.)
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * @param s const uint8_t * string
 * @param i int32_t string offset, must be i<length
 * @param length int32_t string length
 * @param n number of code points to skip
 * @see U8_FWD_N_UNSAFE
 * @stable ICU 2.4
 */
#define U8_FWD_N(s, i, length, n) { \
    int32_t __N=(n); \
    while(__N>0 && ((i)<(length) || ((length)<0 && (s)[i]!=0))) { \
        U8_FWD_1(s, i, length); \
        --__N; \
    } \
}

/**
 * Adjust a random-access offset to a code point boundary
 * at the start of a code point.
 * If the offset points to a UTF-8 trail byte,
 * then the offset is moved backward to the corresponding lead byte.
 * Otherwise, it is not modified.
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @see U8_SET_CP_START
 * @stable ICU 2.4
 */
#define U8_SET_CP_START_UNSAFE(s, i) { \
    while(U8_IS_TRAIL((s)[i])) { --(i); } \
}

/**
 * Adjust a random-access offset to a code point boundary
 * at the start of a code point.
 * If the offset points to a UTF-8 trail byte,
 * then the offset is moved backward to the corresponding lead byte.
 * Otherwise, it is not modified.
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset (usually 0)
 * @param i int32_t string offset, must be start<=i
 * @see U8_SET_CP_START_UNSAFE
 * @stable ICU 2.4
 */
#define U8_SET_CP_START(s, start, i) { \
    if(U8_IS_TRAIL((s)[(i)])) { \
        (i)=utf8_back1SafeBody(s, start, (i)); \
    } \
}

/* definitions with backward iteration -------------------------------------- */

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a multi-byte sequence, then the macro will read
 * the whole sequence.
 * If the offset is behind a lead byte, then that itself
 * will be returned as the code point.
 * The result is undefined if the offset is behind an illegal UTF-8 sequence.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @param c output UChar32 variable
 * @see U8_PREV
 * @stable ICU 2.4
 */
#define U8_PREV_UNSAFE(s, i, c) { \
    (c)=(uint8_t)(s)[--(i)]; \
    if(U8_IS_TRAIL(c)) { \
        uint8_t __b, __count=1, __shift=6; \
\
        /* c is a trail byte */ \
        (c)&=0x3f; \
        for(;;) { \
            __b=(uint8_t)(s)[--(i)]; \
            if(__b>=0xc0) { \
                U8_MASK_LEAD_BYTE(__b, __count); \
                (c)|=(UChar32)__b<<__shift; \
                break; \
            } else { \
                (c)|=(UChar32)(__b&0x3f)<<__shift; \
                ++__count; \
                __shift+=6; \
            } \
        } \
    } \
}

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a multi-byte sequence, then the macro will read
 * the whole sequence.
 * If the offset is behind a lead byte, then that itself
 * will be returned as the code point.
 * If the offset is behind an illegal UTF-8 sequence, then c is set to a negative value.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset (usually 0)
 * @param i int32_t string offset, must be start<i
 * @param c output UChar32 variable, set to <0 in case of an error
 * @see U8_PREV_UNSAFE
 * @stable ICU 2.4
 */
#define U8_PREV(s, start, i, c) { \
    (c)=(uint8_t)(s)[--(i)]; \
    if((c)>=0x80) { \
        (c)=utf8_prevCharSafeBody((const uint8_t *)s, start, &(i), c, -1); \
    } \
}

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a multi-byte sequence, then the macro will read
 * the whole sequence.
 * If the offset is behind a lead byte, then that itself
 * will be returned as the code point.
 * If the offset is behind an illegal UTF-8 sequence, then c is set to U+FFFD.
 *
 * This macro does not distinguish between a real U+FFFD in the text
 * and U+FFFD returned for an ill-formed sequence.
 * Use U8_PREV() if that distinction is important.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset (usually 0)
 * @param i int32_t string offset, must be start<i
 * @param c output UChar32 variable, set to U+FFFD in case of an error
 * @see U8_PREV
 * @stable ICU 51
 */
#define U8_PREV_OR_FFFD(s, start, i, c) { \
    (c)=(uint8_t)(s)[--(i)]; \
    if((c)>=0x80) { \
        (c)=utf8_prevCharSafeBody((const uint8_t *)s, start, &(i), c, -3); \
    } \
}

/**
 * Move the string offset from one code point boundary to the previous one.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @see U8_BACK_1
 * @stable ICU 2.4
 */
#define U8_BACK_1_UNSAFE(s, i) { \
    while(U8_IS_TRAIL((s)[--(i)])) {} \
}

/**
 * Move the string offset from one code point boundary to the previous one.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset (usually 0)
 * @param i int32_t string offset, must be start<i
 * @see U8_BACK_1_UNSAFE
 * @stable ICU 2.4
 */
#define U8_BACK_1(s, start, i) { \
    if(U8_IS_TRAIL((s)[--(i)])) { \
        (i)=utf8_back1SafeBody(s, start, (i)); \
    } \
}

/**
 * Move the string offset from one code point boundary to the n-th one before it,
 * i.e., move backward by n code points.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @param n number of code points to skip
 * @see U8_BACK_N
 * @stable ICU 2.4
 */
#define U8_BACK_N_UNSAFE(s, i, n) { \
    int32_t __N=(n); \
    while(__N>0) { \
        U8_BACK_1_UNSAFE(s, i); \
        --__N; \
    } \
}

/**
 * Move the string offset from one code point boundary to the n-th one before it,
 * i.e., move backward by n code points.
 * (Pre-decrementing backward iteration.)
 * The input offset may be the same as the string length.
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * @param s const uint8_t * string
 * @param start int32_t index of the start of the string
 * @param i int32_t string offset, must be start<i
 * @param n number of code points to skip
 * @see U8_BACK_N_UNSAFE
 * @stable ICU 2.4
 */
#define U8_BACK_N(s, start, i, n) { \
    int32_t __N=(n); \
    while(__N>0 && (i)>(start)) { \
        U8_BACK_1(s, start, i); \
        --__N; \
    } \
}

/**
 * Adjust a random-access offset to a code point boundary after a code point.
 * If the offset is behind a partial multi-byte sequence,
 * then the offset is incremented to behind the whole sequence.
 * Otherwise, it is not modified.
 * The input offset may be the same as the string length.
 * "Unsafe" macro, assumes well-formed UTF-8.
 *
 * @param s const uint8_t * string
 * @param i string offset
 * @see U8_SET_CP_LIMIT
 * @stable ICU 2.4
 */
#define U8_SET_CP_LIMIT_UNSAFE(s, i) { \
    U8_BACK_1_UNSAFE(s, i); \
    U8_FWD_1_UNSAFE(s, i); \
}

/**
 * Adjust a random-access offset to a code point boundary after a code point.
 * If the offset is behind a partial multi-byte sequence,
 * then the offset is incremented to behind the whole sequence.
 * Otherwise, it is not modified.
 * The input offset may be the same as the string length.
 * "Safe" macro, checks for illegal sequences and for string boundaries.
 *
 * The length can be negative for a NUL-terminated string.
 *
 * @param s const uint8_t * string
 * @param start int32_t starting string offset (usually 0)
 * @param i int32_t string offset, must be start<=i<=length
 * @param length int32_t string length
 * @see U8_SET_CP_LIMIT_UNSAFE
 * @stable ICU 2.4
 */
#define U8_SET_CP_LIMIT(s, start, i, length) { \
    if((start)<(i) && ((i)<(length) || (length)<0)) { \
        U8_BACK_1(s, start, i); \
        U8_FWD_1(s, i, length); \
    } \
}

#endif
