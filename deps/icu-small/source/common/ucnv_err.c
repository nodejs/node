// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *****************************************************************************
 *
 *   Copyright (C) 1998-2016, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 *****************************************************************************
 *
 *  ucnv_err.c
 *  Implements error behaviour functions called by T_UConverter_{from,to}Unicode
 *
 *
*   Change history:
*
*   06/29/2000  helena      Major rewrite of the callback APIs.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucnv_err.h"
#include "unicode/ucnv_cb.h"
#include "ucnv_cnv.h"
#include "cmemory.h"
#include "unicode/ucnv.h"
#include "ustrfmt.h"

#define VALUE_STRING_LENGTH 48
/*Magic # 32 = 4(number of char in value string) * 8(max number of bytes per char for any converter) */
#define UNICODE_PERCENT_SIGN_CODEPOINT  0x0025
#define UNICODE_U_CODEPOINT             0x0055
#define UNICODE_X_CODEPOINT             0x0058
#define UNICODE_RS_CODEPOINT            0x005C
#define UNICODE_U_LOW_CODEPOINT         0x0075
#define UNICODE_X_LOW_CODEPOINT         0x0078
#define UNICODE_AMP_CODEPOINT           0x0026
#define UNICODE_HASH_CODEPOINT          0x0023
#define UNICODE_SEMICOLON_CODEPOINT     0x003B
#define UNICODE_PLUS_CODEPOINT          0x002B
#define UNICODE_LEFT_CURLY_CODEPOINT    0x007B
#define UNICODE_RIGHT_CURLY_CODEPOINT   0x007D
#define UNICODE_SPACE_CODEPOINT         0x0020
#define UCNV_PRV_ESCAPE_ICU         0
#define UCNV_PRV_ESCAPE_C           'C'
#define UCNV_PRV_ESCAPE_XML_DEC     'D'
#define UCNV_PRV_ESCAPE_XML_HEX     'X'
#define UCNV_PRV_ESCAPE_JAVA        'J'
#define UCNV_PRV_ESCAPE_UNICODE     'U'
#define UCNV_PRV_ESCAPE_CSS2        'S'
#define UCNV_PRV_STOP_ON_ILLEGAL    'i'

/*
 * IS_DEFAULT_IGNORABLE_CODE_POINT
 * This is to check if a code point has the default ignorable unicode property.
 * As such, this list needs to be updated if the ignorable code point list ever
 * changes.
 * To avoid dependency on other code, this list is hard coded here.
 * When an ignorable code point is found and is unmappable, the default callbacks
 * will ignore them.
 * For a list of the default ignorable code points, use this link: http://unicode.org/cldr/utility/list-unicodeset.jsp?a=[%3ADI%3A]&g=
 *
 * This list should be sync with the one in CharsetCallback.java
 */
#define IS_DEFAULT_IGNORABLE_CODE_POINT(c) (\
    (c == 0x00AD) || \
    (c == 0x034F) || \
    (c == 0x061C) || \
    (c == 0x115F) || \
    (c == 0x1160) || \
    (0x17B4 <= c && c <= 0x17B5) || \
    (0x180B <= c && c <= 0x180E) || \
    (0x200B <= c && c <= 0x200F) || \
    (0x202A <= c && c <= 0x202E) || \
    (c == 0x2060) || \
    (0x2066 <= c && c <= 0x2069) || \
    (0x2061 <= c && c <= 0x2064) || \
    (0x206A <= c && c <= 0x206F) || \
    (c == 0x3164) || \
    (0x0FE00 <= c && c <= 0x0FE0F) || \
    (c == 0x0FEFF) || \
    (c == 0x0FFA0) || \
    (0x01BCA0  <= c && c <= 0x01BCA3) || \
    (0x01D173 <= c && c <= 0x01D17A) || \
    (c == 0x0E0001) || \
    (0x0E0020 <= c && c <= 0x0E007F) || \
    (0x0E0100 <= c && c <= 0x0E01EF) || \
    (c == 0x2065) || \
    (0x0FFF0 <= c && c <= 0x0FFF8) || \
    (c == 0x0E0000) || \
    (0x0E0002 <= c && c <= 0x0E001F) || \
    (0x0E0080 <= c && c <= 0x0E00FF) || \
    (0x0E01F0 <= c && c <= 0x0E0FFF) \
    )


/*Function Pointer STOPS at the ILLEGAL_SEQUENCE */
U_CAPI void    U_EXPORT2
UCNV_FROM_U_CALLBACK_STOP (
                  const void *context,
                  UConverterFromUnicodeArgs *fromUArgs,
                  const UChar* codeUnits,
                  int32_t length,
                  UChar32 codePoint,
                  UConverterCallbackReason reason,
                  UErrorCode * err)
{
    if (reason == UCNV_UNASSIGNED && IS_DEFAULT_IGNORABLE_CODE_POINT(codePoint))
    {
        /*
         * Skip if the codepoint has unicode property of default ignorable.
         */
        *err = U_ZERO_ERROR;
    }
    /* the caller must have set the error code accordingly */
    return;
}


/*Function Pointer STOPS at the ILLEGAL_SEQUENCE */
U_CAPI void    U_EXPORT2
UCNV_TO_U_CALLBACK_STOP (
                   const void *context,
                   UConverterToUnicodeArgs *toUArgs,
                   const char* codePoints,
                   int32_t length,
                   UConverterCallbackReason reason,
                   UErrorCode * err)
{
    /* the caller must have set the error code accordingly */
    return;
}

U_CAPI void    U_EXPORT2
UCNV_FROM_U_CALLBACK_SKIP (
                  const void *context,
                  UConverterFromUnicodeArgs *fromUArgs,
                  const UChar* codeUnits,
                  int32_t length,
                  UChar32 codePoint,
                  UConverterCallbackReason reason,
                  UErrorCode * err)
{
    if (reason <= UCNV_IRREGULAR)
    {
        if (reason == UCNV_UNASSIGNED && IS_DEFAULT_IGNORABLE_CODE_POINT(codePoint))
        {
            /*
             * Skip if the codepoint has unicode property of default ignorable.
             */
            *err = U_ZERO_ERROR;
        }
        else if (context == NULL || (*((char*)context) == UCNV_PRV_STOP_ON_ILLEGAL && reason == UCNV_UNASSIGNED))
        {
            *err = U_ZERO_ERROR;
        }
        /* else the caller must have set the error code accordingly. */
    }
    /* else ignore the reset, close and clone calls. */
}

U_CAPI void    U_EXPORT2
UCNV_FROM_U_CALLBACK_SUBSTITUTE (
                  const void *context,
                  UConverterFromUnicodeArgs *fromArgs,
                  const UChar* codeUnits,
                  int32_t length,
                  UChar32 codePoint,
                  UConverterCallbackReason reason,
                  UErrorCode * err)
{
    if (reason <= UCNV_IRREGULAR)
    {
        if (reason == UCNV_UNASSIGNED && IS_DEFAULT_IGNORABLE_CODE_POINT(codePoint))
        {
            /*
             * Skip if the codepoint has unicode property of default ignorable.
             */
            *err = U_ZERO_ERROR;
        }
        else if (context == NULL || (*((char*)context) == UCNV_PRV_STOP_ON_ILLEGAL && reason == UCNV_UNASSIGNED))
        {
            *err = U_ZERO_ERROR;
            ucnv_cbFromUWriteSub(fromArgs, 0, err);
        }
        /* else the caller must have set the error code accordingly. */
    }
    /* else ignore the reset, close and clone calls. */
}

/*uses uprv_itou to get a unicode escape sequence of the offensive sequence,
 *uses a clean copy (resetted) of the converter, to convert that unicode
 *escape sequence to the target codepage (if conversion failure happens then
 *we revert to substituting with subchar)
 */
U_CAPI void    U_EXPORT2
UCNV_FROM_U_CALLBACK_ESCAPE (
                         const void *context,
                         UConverterFromUnicodeArgs *fromArgs,
                         const UChar *codeUnits,
                         int32_t length,
                         UChar32 codePoint,
                         UConverterCallbackReason reason,
                         UErrorCode * err)
{

  UChar valueString[VALUE_STRING_LENGTH];
  int32_t valueStringLength = 0;
  int32_t i = 0;

  const UChar *myValueSource = NULL;
  UErrorCode err2 = U_ZERO_ERROR;
  UConverterFromUCallback original = NULL;
  const void *originalContext;

  UConverterFromUCallback ignoredCallback = NULL;
  const void *ignoredContext;

  if (reason > UCNV_IRREGULAR)
  {
      return;
  }
  else if (reason == UCNV_UNASSIGNED && IS_DEFAULT_IGNORABLE_CODE_POINT(codePoint))
  {
      /*
       * Skip if the codepoint has unicode property of default ignorable.
       */
      *err = U_ZERO_ERROR;
      return;
  }

  ucnv_setFromUCallBack (fromArgs->converter,
                     (UConverterFromUCallback) UCNV_FROM_U_CALLBACK_SUBSTITUTE,
                     NULL,
                     &original,
                     &originalContext,
                     &err2);

  if (U_FAILURE (err2))
  {
    *err = err2;
    return;
  }
  if(context==NULL)
  {
      while (i < length)
      {
        valueString[valueStringLength++] = (UChar) UNICODE_PERCENT_SIGN_CODEPOINT;  /* adding % */
        valueString[valueStringLength++] = (UChar) UNICODE_U_CODEPOINT; /* adding U */
        valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[i++], 16, 4);
      }
  }
  else
  {
      switch(*((char*)context))
      {
      case UCNV_PRV_ESCAPE_JAVA:
          while (i < length)
          {
              valueString[valueStringLength++] = (UChar) UNICODE_RS_CODEPOINT;    /* adding \ */
              valueString[valueStringLength++] = (UChar) UNICODE_U_LOW_CODEPOINT; /* adding u */
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[i++], 16, 4);
          }
          break;

      case UCNV_PRV_ESCAPE_C:
          valueString[valueStringLength++] = (UChar) UNICODE_RS_CODEPOINT;    /* adding \ */

          if(length==2){
              valueString[valueStringLength++] = (UChar) UNICODE_U_CODEPOINT; /* adding U */
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, codePoint, 16, 8);

          }
          else{
              valueString[valueStringLength++] = (UChar) UNICODE_U_LOW_CODEPOINT; /* adding u */
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[0], 16, 4);
          }
          break;

      case UCNV_PRV_ESCAPE_XML_DEC:

          valueString[valueStringLength++] = (UChar) UNICODE_AMP_CODEPOINT;   /* adding & */
          valueString[valueStringLength++] = (UChar) UNICODE_HASH_CODEPOINT;  /* adding # */
          if(length==2){
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, codePoint, 10, 0);
          }
          else{
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[0], 10, 0);
          }
          valueString[valueStringLength++] = (UChar) UNICODE_SEMICOLON_CODEPOINT; /* adding ; */
          break;

      case UCNV_PRV_ESCAPE_XML_HEX:

          valueString[valueStringLength++] = (UChar) UNICODE_AMP_CODEPOINT;   /* adding & */
          valueString[valueStringLength++] = (UChar) UNICODE_HASH_CODEPOINT;  /* adding # */
          valueString[valueStringLength++] = (UChar) UNICODE_X_LOW_CODEPOINT; /* adding x */
          if(length==2){
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, codePoint, 16, 0);
          }
          else{
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[0], 16, 0);
          }
          valueString[valueStringLength++] = (UChar) UNICODE_SEMICOLON_CODEPOINT; /* adding ; */
          break;

      case UCNV_PRV_ESCAPE_UNICODE:
          valueString[valueStringLength++] = (UChar) UNICODE_LEFT_CURLY_CODEPOINT;    /* adding { */
          valueString[valueStringLength++] = (UChar) UNICODE_U_CODEPOINT;    /* adding U */
          valueString[valueStringLength++] = (UChar) UNICODE_PLUS_CODEPOINT; /* adding + */
          if (length == 2) {
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, codePoint, 16, 4);
          } else {
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[0], 16, 4);
          }
          valueString[valueStringLength++] = (UChar) UNICODE_RIGHT_CURLY_CODEPOINT;    /* adding } */
          break;

      case UCNV_PRV_ESCAPE_CSS2:
          valueString[valueStringLength++] = (UChar) UNICODE_RS_CODEPOINT;    /* adding \ */
          valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, codePoint, 16, 0);
          /* Always add space character, becase the next character might be whitespace,
             which would erroneously be considered the termination of the escape sequence. */
          valueString[valueStringLength++] = (UChar) UNICODE_SPACE_CODEPOINT;
          break;

      default:
          while (i < length)
          {
              valueString[valueStringLength++] = (UChar) UNICODE_PERCENT_SIGN_CODEPOINT;  /* adding % */
              valueString[valueStringLength++] = (UChar) UNICODE_U_CODEPOINT;             /* adding U */
              valueStringLength += uprv_itou (valueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint16_t)codeUnits[i++], 16, 4);
          }
      }
  }
  myValueSource = valueString;

  /* reset the error */
  *err = U_ZERO_ERROR;

  ucnv_cbFromUWriteUChars(fromArgs, &myValueSource, myValueSource+valueStringLength, 0, err);

  ucnv_setFromUCallBack (fromArgs->converter,
                         original,
                         originalContext,
                         &ignoredCallback,
                         &ignoredContext,
                         &err2);
  if (U_FAILURE (err2))
  {
      *err = err2;
      return;
  }

  return;
}



U_CAPI void  U_EXPORT2
UCNV_TO_U_CALLBACK_SKIP (
                 const void *context,
                 UConverterToUnicodeArgs *toArgs,
                 const char* codeUnits,
                 int32_t length,
                 UConverterCallbackReason reason,
                 UErrorCode * err)
{
    if (reason <= UCNV_IRREGULAR)
    {
        if (context == NULL || (*((char*)context) == UCNV_PRV_STOP_ON_ILLEGAL && reason == UCNV_UNASSIGNED))
        {
            *err = U_ZERO_ERROR;
        }
        /* else the caller must have set the error code accordingly. */
    }
    /* else ignore the reset, close and clone calls. */
}

U_CAPI void    U_EXPORT2
UCNV_TO_U_CALLBACK_SUBSTITUTE (
                 const void *context,
                 UConverterToUnicodeArgs *toArgs,
                 const char* codeUnits,
                 int32_t length,
                 UConverterCallbackReason reason,
                 UErrorCode * err)
{
    if (reason <= UCNV_IRREGULAR)
    {
        if (context == NULL || (*((char*)context) == UCNV_PRV_STOP_ON_ILLEGAL && reason == UCNV_UNASSIGNED))
        {
            *err = U_ZERO_ERROR;
            ucnv_cbToUWriteSub(toArgs,0,err);
        }
        /* else the caller must have set the error code accordingly. */
    }
    /* else ignore the reset, close and clone calls. */
}

/*uses uprv_itou to get a unicode escape sequence of the offensive sequence,
 *and uses that as the substitution sequence
 */
U_CAPI void   U_EXPORT2
UCNV_TO_U_CALLBACK_ESCAPE (
                 const void *context,
                 UConverterToUnicodeArgs *toArgs,
                 const char* codeUnits,
                 int32_t length,
                 UConverterCallbackReason reason,
                 UErrorCode * err)
{
    UChar uniValueString[VALUE_STRING_LENGTH];
    int32_t valueStringLength = 0;
    int32_t i = 0;

    if (reason > UCNV_IRREGULAR)
    {
        return;
    }

    if(context==NULL)
    {
        while (i < length)
        {
            uniValueString[valueStringLength++] = (UChar) UNICODE_PERCENT_SIGN_CODEPOINT; /* adding % */
            uniValueString[valueStringLength++] = (UChar) UNICODE_X_CODEPOINT;    /* adding X */
            valueStringLength += uprv_itou (uniValueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint8_t) codeUnits[i++], 16, 2);
        }
    }
    else
    {
        switch(*((char*)context))
        {
        case UCNV_PRV_ESCAPE_XML_DEC:
            while (i < length)
            {
                uniValueString[valueStringLength++] = (UChar) UNICODE_AMP_CODEPOINT;   /* adding & */
                uniValueString[valueStringLength++] = (UChar) UNICODE_HASH_CODEPOINT;  /* adding # */
                valueStringLength += uprv_itou (uniValueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint8_t)codeUnits[i++], 10, 0);
                uniValueString[valueStringLength++] = (UChar) UNICODE_SEMICOLON_CODEPOINT; /* adding ; */
            }
            break;

        case UCNV_PRV_ESCAPE_XML_HEX:
            while (i < length)
            {
                uniValueString[valueStringLength++] = (UChar) UNICODE_AMP_CODEPOINT;   /* adding & */
                uniValueString[valueStringLength++] = (UChar) UNICODE_HASH_CODEPOINT;  /* adding # */
                uniValueString[valueStringLength++] = (UChar) UNICODE_X_LOW_CODEPOINT; /* adding x */
                valueStringLength += uprv_itou (uniValueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint8_t)codeUnits[i++], 16, 0);
                uniValueString[valueStringLength++] = (UChar) UNICODE_SEMICOLON_CODEPOINT; /* adding ; */
            }
            break;
        case UCNV_PRV_ESCAPE_C:
            while (i < length)
            {
                uniValueString[valueStringLength++] = (UChar) UNICODE_RS_CODEPOINT;    /* adding \ */
                uniValueString[valueStringLength++] = (UChar) UNICODE_X_LOW_CODEPOINT; /* adding x */
                valueStringLength += uprv_itou (uniValueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint8_t)codeUnits[i++], 16, 2);
            }
            break;
        default:
            while (i < length)
            {
                uniValueString[valueStringLength++] = (UChar) UNICODE_PERCENT_SIGN_CODEPOINT; /* adding % */
                uniValueString[valueStringLength++] = (UChar) UNICODE_X_CODEPOINT;    /* adding X */
                uprv_itou (uniValueString + valueStringLength, VALUE_STRING_LENGTH - valueStringLength, (uint8_t) codeUnits[i++], 16, 2);
                valueStringLength += 2;
            }
        }
    }
    /* reset the error */
    *err = U_ZERO_ERROR;

    ucnv_cbToUWriteUChars(toArgs, uniValueString, valueStringLength, 0, err);
}

#endif
