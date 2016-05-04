/*
******************************************************************************
*
*   Copyright (C) 1998-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File ufmt_cmn.h
*
* Modification History:
*
*   Date        Name        Description
*   12/02/98    stephen        Creation.
*   03/12/99    stephen     Modified for new C API.
*   03/15/99    stephen     Added defaultCPToUnicode, unicodeToDefaultCP
******************************************************************************
*/

#ifndef UFMT_CMN_H
#define UFMT_CMN_H

#include "unicode/utypes.h"
#include "unicode/utf16.h"

#define UFMT_DEFAULT_BUFFER_SIZE 128
#define MAX_UCHAR_BUFFER_SIZE(buffer) (sizeof(buffer)/(U16_MAX_LENGTH*sizeof(UChar)))
#define MAX_UCHAR_BUFFER_NEEDED(strLen) ((strLen+1)*U16_MAX_LENGTH*sizeof(UChar))

/** 
 * Enum representing the possible argument types for uprintf/uscanf
 */
typedef enum ufmt_type_info {
    ufmt_empty = 0,
    ufmt_simple_percent, /* %% do nothing */
    ufmt_count,      /* special flag for count */
    ufmt_int,        /* int */
    ufmt_char,       /* int, cast to char */
    ufmt_string,     /* char* */
    ufmt_pointer,    /* void* */
    ufmt_float,      /* float */
    ufmt_double,     /* double */
    ufmt_uchar,      /* int, cast to UChar */
    ufmt_ustring     /* UChar* */
    /*ufmt_wchar,*/      /* wchar_t */
    /*ufmt_wstring,*/    /* wchar_t* */
    /*ufmt_date,*/       /* Date */
    /*ufmt_last*/
} ufmt_type_info;

/**
 * Union representing a uprintf/uscanf argument
 */
typedef union ufmt_args {
    int64_t int64Value;    /* int, UChar */
    float   floatValue;    /* float */
    double  doubleValue;   /* double */
    void    *ptrValue;     /* any pointer - void*, char*, wchar_t*, UChar* */
    /*wchar_t wcharValue;*/    /* wchar_t */    /* TODO: Should wchar_t be used? */
    /*UDate dateValue;*/     /* Date */
} ufmt_args;

/**
 * Macro for determining the minimum of two numbers.
 * @param a An integer
 * @param b An integer
 * @return <TT>a</TT> if </TT>a < b</TT>, <TT>b</TT> otherwise
 */
#define ufmt_min(a,b) ((a) < (b) ? (a) : (b))

/**
 * Convert a UChar in hex radix to an integer value.
 * @param c The UChar to convert.
 * @return The integer value of <TT>c</TT>.
 */
int
ufmt_digitvalue(UChar c);

/**
 * Determine if a UChar is a digit for a specified radix.
 * @param c The UChar to check.
 * @param radix The desired radix.
 * @return TRUE if <TT>c</TT> is a digit in <TT>radix</TT>, FALSE otherwise.
 */
UBool
ufmt_isdigit(UChar     c,
         int32_t     radix);

/**
 * Convert an int64_t to a UChar* in a specified radix
 * @param buffer The target buffer
 * @param len On input, the size of <TT>buffer</TT>.  On output,
 * the number of UChars written to <TT>buffer</TT>.
 * @param value The value to be converted
 * @param radix The desired radix
 * @param uselower TRUE means lower case will be used, FALSE means upper case
 * @param minDigits The minimum number of digits for for the formatted number,
 * which will be padded with zeroes. -1 means do not pad.
 */
void 
ufmt_64tou(UChar     *buffer, 
      int32_t     *len,
      uint64_t     value, 
      uint8_t     radix,
      UBool    uselower,
      int32_t    minDigits);

/**
 * It's like ufmt_64tou, but with a pointer.
 * This functions avoids size constraints of 64-bit types.
 * Pointers can be at 32-128 bits in size.
 */
void 
ufmt_ptou(UChar    *buffer, 
          int32_t   *len,
          void      *value, 
          UBool     uselower);

/**
 * Convert a UChar* in a specified radix to an int64_t.
 * @param buffer The target buffer
 * @param len On input, the size of <TT>buffer</TT>.  On output,
 * the number of UChars read from <TT>buffer</TT>.
 * @param radix The desired radix
 * @return The numeric value.
 */
int64_t
ufmt_uto64(const UChar     *buffer, 
      int32_t     *len,
      int8_t     radix);

/**
 * Convert a UChar* in a specified radix to a pointer,
 * @param buffer The target buffer
 * @param len On input, the size of <TT>buffer</TT>.  On output,
 * the number of UChars read from <TT>buffer</TT>.
 * @param radix The desired radix
 * @return The pointer value.
 */
void *
ufmt_utop(const UChar     *buffer,
      int32_t     *len);

/**
 * Convert a string from the default codepage to Unicode.
 * @param s The string to convert, in the default codepage.
 * @param sSize The size of s to convert.
 * @param target The buffer to convert to.
 * @param tSize The size of target
 * @return A pointer to a newly allocated converted version of s, or 0 
 * on error.
 */
UChar*
ufmt_defaultCPToUnicode(const char *s, int32_t sSize,
                        UChar *target, int32_t tSize);



#endif

