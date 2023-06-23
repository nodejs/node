// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1999-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*   file name:  utf_impl.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999sep13
*   created by: Markus W. Scherer
*
*   This file provides implementation functions for macros in the utfXX.h
*   that would otherwise be too long as macros.
*/

/* set import/export definitions */
#ifndef U_UTF8_IMPL
#   define U_UTF8_IMPL
#endif

#include "unicode/utypes.h"
#include "unicode/utf.h"
#include "unicode/utf8.h"
#include "uassert.h"

/*
 * Table of the number of utf8 trail bytes, indexed by the lead byte.
 * Used by the deprecated macro UTF8_COUNT_TRAIL_BYTES, defined in utf_old.h
 *
 * The current macro, U8_COUNT_TRAIL_BYTES, does _not_ use this table.
 *
 * Note that this table cannot be removed, even if UTF8_COUNT_TRAIL_BYTES were
 * changed to no longer use it. References to the table from expansions of UTF8_COUNT_TRAIL_BYTES
 * may exist in old client code that must continue to run with newer icu library versions.
 *
 * This table could be replaced on many machines by
 * a few lines of assembler code using an
 * "index of first 0-bit from msb" instruction and
 * one or two more integer instructions.
 *
 * For example, on an i386, do something like
 * - MOV AL, leadByte
 * - NOT AL         (8-bit, leave b15..b8==0..0, reverse only b7..b0)
 * - MOV AH, 0
 * - BSR BX, AX     (16-bit)
 * - MOV AX, 6      (result)
 * - JZ finish      (ZF==1 if leadByte==0xff)
 * - SUB AX, BX (result)
 * -finish:
 * (BSR: Bit Scan Reverse, scans for a 1-bit, starting from the MSB)
 */
U_CAPI const uint8_t
utf8_countTrailBytes[256]={
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

    // illegal C0 & C1
    // 2-byte lead bytes C2..DF
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,

    // 3-byte lead bytes E0..EF
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    // 4-byte lead bytes F0..F4
    // illegal F5..FF
    3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const UChar32
utf8_errorValue[6]={
    // Same values as UTF8_ERROR_VALUE_1, UTF8_ERROR_VALUE_2, UTF_ERROR_VALUE,
    // but without relying on the obsolete unicode/utf_old.h.
    0x15, 0x9f, 0xffff,
    0x10ffff
};

static UChar32
errorValue(int32_t count, int8_t strict) {
    if(strict>=0) {
        return utf8_errorValue[count];
    } else if(strict==-3) {
        return 0xfffd;
    } else {
        return U_SENTINEL;
    }
}

/*
 * Handle the non-inline part of the U8_NEXT() and U8_NEXT_FFFD() macros
 * and their obsolete sibling UTF8_NEXT_CHAR_SAFE().
 *
 * U8_NEXT() supports NUL-terminated strings indicated via length<0.
 *
 * The "strict" parameter controls the error behavior:
 * <0  "Safe" behavior of U8_NEXT():
 *     -1: All illegal byte sequences yield U_SENTINEL=-1.
 *     -2: Same as -1, except for lenient treatment of surrogate code points as legal.
 *         Some implementations use this for roundtripping of
 *         Unicode 16-bit strings that are not well-formed UTF-16, that is, they
 *         contain unpaired surrogates.
 *     -3: All illegal byte sequences yield U+FFFD.
 *  0  Obsolete "safe" behavior of UTF8_NEXT_CHAR_SAFE(..., false):
 *     All illegal byte sequences yield a positive code point such that this
 *     result code point would be encoded with the same number of bytes as
 *     the illegal sequence.
 * >0  Obsolete "strict" behavior of UTF8_NEXT_CHAR_SAFE(..., true):
 *     Same as the obsolete "safe" behavior, but non-characters are also treated
 *     like illegal sequences.
 *
 * Note that a UBool is the same as an int8_t.
 */
U_CAPI UChar32 U_EXPORT2
utf8_nextCharSafeBody(const uint8_t *s, int32_t *pi, int32_t length, UChar32 c, UBool strict) {
    // *pi is one after byte c.
    int32_t i=*pi;
    // length can be negative for NUL-terminated strings: Read and validate one byte at a time.
    if(i==length || c>0xf4) {
        // end of string, or not a lead byte
    } else if(c>=0xf0) {
        // Test for 4-byte sequences first because
        // U8_NEXT() handles shorter valid sequences inline.
        uint8_t t1=s[i], t2, t3;
        c&=7;
        if(U8_IS_VALID_LEAD4_AND_T1(c, t1) &&
                ++i!=length && (t2=s[i]-0x80)<=0x3f &&
                ++i!=length && (t3=s[i]-0x80)<=0x3f) {
            ++i;
            c=(c<<18)|((t1&0x3f)<<12)|(t2<<6)|t3;
            // strict: forbid non-characters like U+fffe
            if(strict<=0 || !U_IS_UNICODE_NONCHAR(c)) {
                *pi=i;
                return c;
            }
        }
    } else if(c>=0xe0) {
        c&=0xf;
        if(strict!=-2) {
            uint8_t t1=s[i], t2;
            if(U8_IS_VALID_LEAD3_AND_T1(c, t1) &&
                    ++i!=length && (t2=s[i]-0x80)<=0x3f) {
                ++i;
                c=(c<<12)|((t1&0x3f)<<6)|t2;
                // strict: forbid non-characters like U+fffe
                if(strict<=0 || !U_IS_UNICODE_NONCHAR(c)) {
                    *pi=i;
                    return c;
                }
            }
        } else {
            // strict=-2 -> lenient: allow surrogates
            uint8_t t1=s[i]-0x80, t2;
            if(t1<=0x3f && (c>0 || t1>=0x20) &&
                    ++i!=length && (t2=s[i]-0x80)<=0x3f) {
                *pi=i+1;
                return (c<<12)|(t1<<6)|t2;
            }
        }
    } else if(c>=0xc2) {
        uint8_t t1=s[i]-0x80;
        if(t1<=0x3f) {
            *pi=i+1;
            return ((c-0xc0)<<6)|t1;
        }
    }  // else 0x80<=c<0xc2 is not a lead byte

    /* error handling */
    c=errorValue(i-*pi, strict);
    *pi=i;
    return c;
}

U_CAPI int32_t U_EXPORT2
utf8_appendCharSafeBody(uint8_t *s, int32_t i, int32_t length, UChar32 c, UBool *pIsError) {
    if((uint32_t)(c)<=0x7ff) {
        if((i)+1<(length)) {
            (s)[(i)++]=(uint8_t)(((c)>>6)|0xc0);
            (s)[(i)++]=(uint8_t)(((c)&0x3f)|0x80);
            return i;
        }
    } else if((uint32_t)(c)<=0xffff) {
        /* Starting with Unicode 3.2, surrogate code points must not be encoded in UTF-8. */
        if((i)+2<(length) && !U_IS_SURROGATE(c)) {
            (s)[(i)++]=(uint8_t)(((c)>>12)|0xe0);
            (s)[(i)++]=(uint8_t)((((c)>>6)&0x3f)|0x80);
            (s)[(i)++]=(uint8_t)(((c)&0x3f)|0x80);
            return i;
        }
    } else if((uint32_t)(c)<=0x10ffff) {
        if((i)+3<(length)) {
            (s)[(i)++]=(uint8_t)(((c)>>18)|0xf0);
            (s)[(i)++]=(uint8_t)((((c)>>12)&0x3f)|0x80);
            (s)[(i)++]=(uint8_t)((((c)>>6)&0x3f)|0x80);
            (s)[(i)++]=(uint8_t)(((c)&0x3f)|0x80);
            return i;
        }
    }
    /* c>0x10ffff or not enough space, write an error value */
    if(pIsError!=nullptr) {
        *pIsError=true;
    } else {
        length-=i;
        if(length>0) {
            int32_t offset;
            if(length>3) {
                length=3;
            }
            s+=i;
            offset=0;
            c=utf8_errorValue[length-1];
            U8_APPEND_UNSAFE(s, offset, c);
            i=i+offset;
        }
    }
    return i;
}

U_CAPI UChar32 U_EXPORT2
utf8_prevCharSafeBody(const uint8_t *s, int32_t start, int32_t *pi, UChar32 c, UBool strict) {
    // *pi is the index of byte c.
    int32_t i=*pi;
    if(U8_IS_TRAIL(c) && i>start) {
        uint8_t b1=s[--i];
        if(U8_IS_LEAD(b1)) {
            if(b1<0xe0) {
                *pi=i;
                return ((b1-0xc0)<<6)|(c&0x3f);
            } else if(b1<0xf0 ? U8_IS_VALID_LEAD3_AND_T1(b1, c) : U8_IS_VALID_LEAD4_AND_T1(b1, c)) {
                // Truncated 3- or 4-byte sequence.
                *pi=i;
                return errorValue(1, strict);
            }
        } else if(U8_IS_TRAIL(b1) && i>start) {
            // Extract the value bits from the last trail byte.
            c&=0x3f;
            uint8_t b2=s[--i];
            if(0xe0<=b2 && b2<=0xf4) {
                if(b2<0xf0) {
                    b2&=0xf;
                    if(strict!=-2) {
                        if(U8_IS_VALID_LEAD3_AND_T1(b2, b1)) {
                            *pi=i;
                            c=(b2<<12)|((b1&0x3f)<<6)|c;
                            if(strict<=0 || !U_IS_UNICODE_NONCHAR(c)) {
                                return c;
                            } else {
                                // strict: forbid non-characters like U+fffe
                                return errorValue(2, strict);
                            }
                        }
                    } else {
                        // strict=-2 -> lenient: allow surrogates
                        b1-=0x80;
                        if((b2>0 || b1>=0x20)) {
                            *pi=i;
                            return (b2<<12)|(b1<<6)|c;
                        }
                    }
                } else if(U8_IS_VALID_LEAD4_AND_T1(b2, b1)) {
                    // Truncated 4-byte sequence.
                    *pi=i;
                    return errorValue(2, strict);
                }
            } else if(U8_IS_TRAIL(b2) && i>start) {
                uint8_t b3=s[--i];
                if(0xf0<=b3 && b3<=0xf4) {
                    b3&=7;
                    if(U8_IS_VALID_LEAD4_AND_T1(b3, b2)) {
                        *pi=i;
                        c=(b3<<18)|((b2&0x3f)<<12)|((b1&0x3f)<<6)|c;
                        if(strict<=0 || !U_IS_UNICODE_NONCHAR(c)) {
                            return c;
                        } else {
                            // strict: forbid non-characters like U+fffe
                            return errorValue(3, strict);
                        }
                    }
                }
            }
        }
    }
    return errorValue(0, strict);
}

U_CAPI int32_t U_EXPORT2
utf8_back1SafeBody(const uint8_t *s, int32_t start, int32_t i) {
    // Same as utf8_prevCharSafeBody(..., strict=-1) minus assembling code points.
    int32_t orig_i=i;
    uint8_t c=s[i];
    if(U8_IS_TRAIL(c) && i>start) {
        uint8_t b1=s[--i];
        if(U8_IS_LEAD(b1)) {
            if(b1<0xe0 ||
                    (b1<0xf0 ? U8_IS_VALID_LEAD3_AND_T1(b1, c) : U8_IS_VALID_LEAD4_AND_T1(b1, c))) {
                return i;
            }
        } else if(U8_IS_TRAIL(b1) && i>start) {
            uint8_t b2=s[--i];
            if(0xe0<=b2 && b2<=0xf4) {
                if(b2<0xf0 ? U8_IS_VALID_LEAD3_AND_T1(b2, b1) : U8_IS_VALID_LEAD4_AND_T1(b2, b1)) {
                    return i;
                }
            } else if(U8_IS_TRAIL(b2) && i>start) {
                uint8_t b3=s[--i];
                if(0xf0<=b3 && b3<=0xf4 && U8_IS_VALID_LEAD4_AND_T1(b3, b2)) {
                    return i;
                }
            }
        }
    }
    return orig_i;
}
