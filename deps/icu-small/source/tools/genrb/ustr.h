// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1998-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File ustr.h
*
* Modification History:
*
*   Date        Name        Description
*   05/28/99    stephen     Creation.
*******************************************************************************
*/

#ifndef USTR_H
#define USTR_H 1

#include "unicode/utypes.h"

#define U_APPEND_CHAR32(c,target,len) {                         \
    if (c <= 0xffff)                                            \
    {                                                           \
        *(target)++ = (UChar) c;                                \
        len=1;                                                  \
    }                                                           \
    else                                                        \
    {                                                           \
        target[0] = U16_LEAD(c);                                \
        target[1] = U16_TRAIL(c);                               \
        len=2;                                                  \
        target +=2;                                             \
    }                                                           \
}

#define U_APPEND_CHAR32_ONLY(c,target) {                             \
    if (c <= 0xffff)                                            \
    {                                                           \
        *(target)++ = (UChar) c;                                \
    }                                                           \
    else                                                        \
    {                                                           \
        target[0] = U16_LEAD(c);                                \
        target[1] = U16_TRAIL(c);                               \
        target +=2;                                             \
    }                                                           \
}

/* A C representation of a string "object" (to avoid realloc all the time) */
struct UString {
  UChar *fChars;
  int32_t fLength;
  int32_t fCapacity;
};

U_CFUNC void ustr_init(struct UString *s);

U_CFUNC void
ustr_initChars(struct UString *s, const char* source, int32_t length, UErrorCode *status);

U_CFUNC void ustr_deinit(struct UString *s);

U_CFUNC void ustr_setlen(struct UString *s, int32_t len, UErrorCode *status);

U_CFUNC void ustr_cpy(struct UString *dst, const struct UString *src,
                      UErrorCode *status);

U_CFUNC void ustr_cat(struct UString *dst, const struct UString *src,
                      UErrorCode *status);

U_CFUNC void ustr_ncat(struct UString *dst, const struct UString *src,
                       int32_t n, UErrorCode *status);

U_CFUNC void ustr_ucat(struct UString *dst, UChar c, UErrorCode *status);
U_CFUNC void ustr_u32cat(struct UString *dst, UChar32 c, UErrorCode *status);
U_CFUNC void ustr_uscat(struct UString *dst, const UChar* src,int len,UErrorCode *status);
#endif
