/*
*******************************************************************************
*
*   Copyright (C) 1998-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*
* File ustr.c
*
* Modification History:
*
*   Date        Name        Description
*   05/28/99    stephen     Creation.
*******************************************************************************
*/

#include "ustr.h"
#include "cmemory.h"
#include "cstring.h"
#include "unicode/ustring.h"
#include "unicode/putil.h"
#include "unicode/utf16.h"

/* Protos */
static void ustr_resize(struct UString *s, int32_t len, UErrorCode *status);

/* Macros */
#define ALLOCATION(minSize) (minSize < 0x80 ? 0x80 : (2 * minSize + 0x80) & ~(0x80 - 1))

U_CFUNC void
ustr_init(struct UString *s)
{
    s->fChars = 0;
    s->fLength = s->fCapacity = 0;
}

U_CFUNC void
ustr_initChars(struct UString *s, const char* source, int32_t length, UErrorCode *status)
{
    int i = 0;
    if (U_FAILURE(*status)) return;
    s->fChars = 0;
    s->fLength = s->fCapacity = 0;
    if (length == -1) {
        length = (int32_t)uprv_strlen(source);
    }
    if(s->fCapacity < length) {
      ustr_resize(s, ALLOCATION(length), status);
      if(U_FAILURE(*status)) return;
    }
    for (; i < length; i++)
    {
      UChar charToAppend;
      u_charsToUChars(source+i, &charToAppend, 1);
      ustr_ucat(s, charToAppend, status);
      /*
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
        ustr_ucat(s, (UChar)(uint8_t)(source[i]), status);
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
        ustr_ucat(s, (UChar)asciiFromEbcdic[(uint8_t)(*cs++)], status);
#else
#   error U_CHARSET_FAMILY is not valid
#endif
      */
    }
}

U_CFUNC void
ustr_deinit(struct UString *s)
{
    if (s) {
        uprv_free(s->fChars);
        s->fChars = 0;
        s->fLength = s->fCapacity = 0;
    }
}

U_CFUNC void
ustr_cpy(struct UString *dst,
     const struct UString *src,
     UErrorCode *status)
{
    if(U_FAILURE(*status) || dst == src)
        return;

    if(dst->fCapacity < src->fLength) {
        ustr_resize(dst, ALLOCATION(src->fLength), status);
        if(U_FAILURE(*status))
            return;
    }
    if(src->fChars == NULL || dst->fChars == NULL){
        return;
    }
    uprv_memcpy(dst->fChars, src->fChars, sizeof(UChar) * src->fLength);
    dst->fLength = src->fLength;
    dst->fChars[dst->fLength] = 0x0000;
}

U_CFUNC void
ustr_setlen(struct UString *s,
        int32_t len,
        UErrorCode *status)
{
    if(U_FAILURE(*status))
        return;

    if(s->fCapacity < (len + 1)) {
        ustr_resize(s, ALLOCATION(len), status);
        if(U_FAILURE(*status))
            return;
    }

    s->fLength = len;
    s->fChars[len] = 0x0000;
}

U_CFUNC void
ustr_cat(struct UString *dst,
     const struct UString *src,
     UErrorCode *status)
{
    ustr_ncat(dst, src, src->fLength, status);
}

U_CFUNC void
ustr_ncat(struct UString *dst,
      const struct UString *src,
      int32_t n,
      UErrorCode *status)
{
    if(U_FAILURE(*status) || dst == src)
        return;
    
    if(dst->fCapacity < (dst->fLength + n)) {
        ustr_resize(dst, ALLOCATION(dst->fLength + n), status);
        if(U_FAILURE(*status))
            return;
    }
    
    uprv_memcpy(dst->fChars + dst->fLength, src->fChars,
                sizeof(UChar) * n);
    dst->fLength += src->fLength;
    dst->fChars[dst->fLength] = 0x0000;
}

U_CFUNC void
ustr_ucat(struct UString *dst,
      UChar c,
      UErrorCode *status)
{
    if(U_FAILURE(*status))
        return;

    if(dst->fCapacity < (dst->fLength + 1)) {
        ustr_resize(dst, ALLOCATION(dst->fLength + 1), status);
        if(U_FAILURE(*status))
            return;
    }

    uprv_memcpy(dst->fChars + dst->fLength, &c,
        sizeof(UChar) * 1);
    dst->fLength += 1;
    dst->fChars[dst->fLength] = 0x0000;
}
U_CFUNC void 
ustr_u32cat(struct UString *dst, UChar32 c, UErrorCode *status){
    if(c > 0x10FFFF){
        *status = U_ILLEGAL_CHAR_FOUND;
        return;
    }
    if(c >0xFFFF){
        ustr_ucat(dst, U16_LEAD(c), status);
        ustr_ucat(dst, U16_TRAIL(c), status);
    }else{
        ustr_ucat(dst, (UChar) c, status);
    }
}
U_CFUNC void
ustr_uscat(struct UString *dst,
      const UChar* src,int len,
      UErrorCode *status)
{
    if(U_FAILURE(*status)) 
        return;

    if(dst->fCapacity < (dst->fLength + len)) {
        ustr_resize(dst, ALLOCATION(dst->fLength + len), status);
        if(U_FAILURE(*status))
            return;
    }

    uprv_memcpy(dst->fChars + dst->fLength, src,
        sizeof(UChar) * len);
    dst->fLength += len;
    dst->fChars[dst->fLength] = 0x0000;
}

/* Destroys data in the string */
static void
ustr_resize(struct UString *s,
        int32_t len,
        UErrorCode *status)
{
    if(U_FAILURE(*status))
        return;

    /* +1 for trailing 0x0000 */
    s->fChars = (UChar*) uprv_realloc(s->fChars, sizeof(UChar) * (len + 1));
    if(s->fChars == 0) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        s->fLength = s->fCapacity = 0;
        return;
    }

    s->fCapacity = len;
}
