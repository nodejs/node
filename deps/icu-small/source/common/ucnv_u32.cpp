// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*  
**********************************************************************
*   Copyright (C) 2002-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  ucnv_u32.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002jul01
*   created by: Markus W. Scherer
*
*   UTF-32 converter implementation. Used to be in ucnv_utf.c.
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION && !UCONFIG_ONLY_HTML_CONVERSION

#include "unicode/ucnv.h"
#include "unicode/utf.h"
#include "ucnv_bld.h"
#include "ucnv_cnv.h"
#include "cmemory.h"

#define MAXIMUM_UCS2            0x0000FFFF
#define MAXIMUM_UTF             0x0010FFFF
#define HALF_SHIFT              10
#define HALF_BASE               0x0010000
#define HALF_MASK               0x3FF
#define SURROGATE_HIGH_START    0xD800
#define SURROGATE_LOW_START     0xDC00

/* -SURROGATE_LOW_START + HALF_BASE */
#define SURROGATE_LOW_BASE      9216

enum {
    UCNV_NEED_TO_WRITE_BOM=1
};

/* UTF-32BE ----------------------------------------------------------------- */
U_CDECL_BEGIN
static void U_CALLCONV
T_UConverter_toUnicode_UTF32_BE(UConverterToUnicodeArgs * args,
                                UErrorCode * err)
{
    const unsigned char *mySource = (unsigned char *) args->source;
    UChar *myTarget = args->target;
    const unsigned char *sourceLimit = (unsigned char *) args->sourceLimit;
    const UChar *targetLimit = args->targetLimit;
    unsigned char *toUBytes = args->converter->toUBytes;
    uint32_t ch, i;

    /* Restore state of current sequence */
    if (args->converter->toULength > 0 && myTarget < targetLimit) {
        i = args->converter->toULength;       /* restore # of bytes consumed */
        args->converter->toULength = 0;

        ch = args->converter->toUnicodeStatus - 1;/*Stores the previously calculated ch from a previous call*/
        args->converter->toUnicodeStatus = 0;
        goto morebytes;
    }

    while (mySource < sourceLimit && myTarget < targetLimit) {
        i = 0;
        ch = 0;
morebytes:
        while (i < sizeof(uint32_t)) {
            if (mySource < sourceLimit) {
                ch = (ch << 8) | (uint8_t)(*mySource);
                toUBytes[i++] = (char) *(mySource++);
            }
            else {
                /* stores a partially calculated target*/
                /* + 1 to make 0 a valid character */
                args->converter->toUnicodeStatus = ch + 1;
                args->converter->toULength = (int8_t) i;
                goto donefornow;
            }
        }

        if (ch <= MAXIMUM_UTF && !U_IS_SURROGATE(ch)) {
            /* Normal valid byte when the loop has not prematurely terminated (i < inBytes) */
            if (ch <= MAXIMUM_UCS2) 
            {
                /* fits in 16 bits */
                *(myTarget++) = (UChar) ch;
            }
            else {
                /* write out the surrogates */
                *(myTarget++) = U16_LEAD(ch);
                ch = U16_TRAIL(ch);
                if (myTarget < targetLimit) {
                    *(myTarget++) = (UChar)ch;
                }
                else {
                    /* Put in overflow buffer (not handled here) */
                    args->converter->UCharErrorBuffer[0] = (UChar) ch;
                    args->converter->UCharErrorBufferLength = 1;
                    *err = U_BUFFER_OVERFLOW_ERROR;
                    break;
                }
            }
        }
        else {
            args->converter->toULength = (int8_t)i;
            *err = U_ILLEGAL_CHAR_FOUND;
            break;
        }
    }

donefornow:
    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err)) {
        /* End of target buffer */
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = myTarget;
    args->source = (const char *) mySource;
}

static void U_CALLCONV
T_UConverter_toUnicode_UTF32_BE_OFFSET_LOGIC(UConverterToUnicodeArgs * args,
                                             UErrorCode * err)
{
    const unsigned char *mySource = (unsigned char *) args->source;
    UChar *myTarget = args->target;
    int32_t *myOffsets = args->offsets;
    const unsigned char *sourceLimit = (unsigned char *) args->sourceLimit;
    const UChar *targetLimit = args->targetLimit;
    unsigned char *toUBytes = args->converter->toUBytes;
    uint32_t ch, i;
    int32_t offsetNum = 0;

    /* Restore state of current sequence */
    if (args->converter->toULength > 0 && myTarget < targetLimit) {
        i = args->converter->toULength;       /* restore # of bytes consumed */
        args->converter->toULength = 0;

        ch = args->converter->toUnicodeStatus - 1;/*Stores the previously calculated ch from a previous call*/
        args->converter->toUnicodeStatus = 0;
        goto morebytes;
    }

    while (mySource < sourceLimit && myTarget < targetLimit) {
        i = 0;
        ch = 0;
morebytes:
        while (i < sizeof(uint32_t)) {
            if (mySource < sourceLimit) {
                ch = (ch << 8) | (uint8_t)(*mySource);
                toUBytes[i++] = (char) *(mySource++);
            }
            else {
                /* stores a partially calculated target*/
                /* + 1 to make 0 a valid character */
                args->converter->toUnicodeStatus = ch + 1;
                args->converter->toULength = (int8_t) i;
                goto donefornow;
            }
        }

        if (ch <= MAXIMUM_UTF && !U_IS_SURROGATE(ch)) {
            /* Normal valid byte when the loop has not prematurely terminated (i < inBytes) */
            if (ch <= MAXIMUM_UCS2) {
                /* fits in 16 bits */
                *(myTarget++) = (UChar) ch;
                *(myOffsets++) = offsetNum;
            }
            else {
                /* write out the surrogates */
                *(myTarget++) = U16_LEAD(ch);
                *myOffsets++ = offsetNum;
                ch = U16_TRAIL(ch);
                if (myTarget < targetLimit)
                {
                    *(myTarget++) = (UChar)ch;
                    *(myOffsets++) = offsetNum;
                }
                else {
                    /* Put in overflow buffer (not handled here) */
                    args->converter->UCharErrorBuffer[0] = (UChar) ch;
                    args->converter->UCharErrorBufferLength = 1;
                    *err = U_BUFFER_OVERFLOW_ERROR;
                    break;
                }
            }
        }
        else {
            args->converter->toULength = (int8_t)i;
            *err = U_ILLEGAL_CHAR_FOUND;
            break;
        }
        offsetNum += i;
    }

donefornow:
    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err))
    {
        /* End of target buffer */
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = myTarget;
    args->source = (const char *) mySource;
    args->offsets = myOffsets;
}

static void U_CALLCONV
T_UConverter_fromUnicode_UTF32_BE(UConverterFromUnicodeArgs * args,
                                  UErrorCode * err)
{
    const UChar *mySource = args->source;
    unsigned char *myTarget;
    const UChar *sourceLimit = args->sourceLimit;
    const unsigned char *targetLimit = (unsigned char *) args->targetLimit;
    UChar32 ch, ch2;
    unsigned int indexToWrite;
    unsigned char temp[sizeof(uint32_t)];

    if(mySource >= sourceLimit) {
        /* no input, nothing to do */
        return;
    }

    /* write the BOM if necessary */
    if(args->converter->fromUnicodeStatus==UCNV_NEED_TO_WRITE_BOM) {
        static const char bom[]={ 0, 0, (char)0xfeu, (char)0xffu };
        ucnv_fromUWriteBytes(args->converter,
                             bom, 4,
                             &args->target, args->targetLimit,
                             &args->offsets, -1,
                             err);
        args->converter->fromUnicodeStatus=0;
    }

    myTarget = (unsigned char *) args->target;
    temp[0] = 0;

    if (args->converter->fromUChar32) {
        ch = args->converter->fromUChar32;
        args->converter->fromUChar32 = 0;
        goto lowsurogate;
    }

    while (mySource < sourceLimit && myTarget < targetLimit) {
        ch = *(mySource++);

        if (U_IS_SURROGATE(ch)) {
            if (U_IS_LEAD(ch)) {
lowsurogate:
                if (mySource < sourceLimit) {
                    ch2 = *mySource;
                    if (U_IS_TRAIL(ch2)) {
                        ch = ((ch - SURROGATE_HIGH_START) << HALF_SHIFT) + ch2 + SURROGATE_LOW_BASE;
                        mySource++;
                    }
                    else {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        args->converter->fromUChar32 = ch;
                        *err = U_ILLEGAL_CHAR_FOUND;
                        break;
                    }
                }
                else {
                    /* ran out of source */
                    args->converter->fromUChar32 = ch;
                    if (args->flush) {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        *err = U_ILLEGAL_CHAR_FOUND;
                    }
                    break;
                }
            }
            else {
                /* this is an unmatched trail code unit (2nd surrogate) */
                /* callback(illegal) */
                args->converter->fromUChar32 = ch;
                *err = U_ILLEGAL_CHAR_FOUND;
                break;
            }
        }

        /* We cannot get any larger than 10FFFF because we are coming from UTF-16 */
        temp[1] = (uint8_t) (ch >> 16 & 0x1F);
        temp[2] = (uint8_t) (ch >> 8);  /* unsigned cast implicitly does (ch & FF) */
        temp[3] = (uint8_t) (ch);       /* unsigned cast implicitly does (ch & FF) */

        for (indexToWrite = 0; indexToWrite <= sizeof(uint32_t) - 1; indexToWrite++) {
            if (myTarget < targetLimit) {
                *(myTarget++) = temp[indexToWrite];
            }
            else {
                args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] = temp[indexToWrite];
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
        }
    }

    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err)) {
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = (char *) myTarget;
    args->source = mySource;
}

static void U_CALLCONV
T_UConverter_fromUnicode_UTF32_BE_OFFSET_LOGIC(UConverterFromUnicodeArgs * args,
                                               UErrorCode * err)
{
    const UChar *mySource = args->source;
    unsigned char *myTarget;
    int32_t *myOffsets;
    const UChar *sourceLimit = args->sourceLimit;
    const unsigned char *targetLimit = (unsigned char *) args->targetLimit;
    UChar32 ch, ch2;
    int32_t offsetNum = 0;
    unsigned int indexToWrite;
    unsigned char temp[sizeof(uint32_t)];

    if(mySource >= sourceLimit) {
        /* no input, nothing to do */
        return;
    }

    /* write the BOM if necessary */
    if(args->converter->fromUnicodeStatus==UCNV_NEED_TO_WRITE_BOM) {
        static const char bom[]={ 0, 0, (char)0xfeu, (char)0xffu };
        ucnv_fromUWriteBytes(args->converter,
                             bom, 4,
                             &args->target, args->targetLimit,
                             &args->offsets, -1,
                             err);
        args->converter->fromUnicodeStatus=0;
    }

    myTarget = (unsigned char *) args->target;
    myOffsets = args->offsets;
    temp[0] = 0;

    if (args->converter->fromUChar32) {
        ch = args->converter->fromUChar32;
        args->converter->fromUChar32 = 0;
        goto lowsurogate;
    }

    while (mySource < sourceLimit && myTarget < targetLimit) {
        ch = *(mySource++);

        if (U_IS_SURROGATE(ch)) {
            if (U_IS_LEAD(ch)) {
lowsurogate:
                if (mySource < sourceLimit) {
                    ch2 = *mySource;
                    if (U_IS_TRAIL(ch2)) {
                        ch = ((ch - SURROGATE_HIGH_START) << HALF_SHIFT) + ch2 + SURROGATE_LOW_BASE;
                        mySource++;
                    }
                    else {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        args->converter->fromUChar32 = ch;
                        *err = U_ILLEGAL_CHAR_FOUND;
                        break;
                    }
                }
                else {
                    /* ran out of source */
                    args->converter->fromUChar32 = ch;
                    if (args->flush) {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        *err = U_ILLEGAL_CHAR_FOUND;
                    }
                    break;
                }
            }
            else {
                /* this is an unmatched trail code unit (2nd surrogate) */
                /* callback(illegal) */
                args->converter->fromUChar32 = ch;
                *err = U_ILLEGAL_CHAR_FOUND;
                break;
            }
        }

        /* We cannot get any larger than 10FFFF because we are coming from UTF-16 */
        temp[1] = (uint8_t) (ch >> 16 & 0x1F);
        temp[2] = (uint8_t) (ch >> 8);  /* unsigned cast implicitly does (ch & FF) */
        temp[3] = (uint8_t) (ch);       /* unsigned cast implicitly does (ch & FF) */

        for (indexToWrite = 0; indexToWrite <= sizeof(uint32_t) - 1; indexToWrite++) {
            if (myTarget < targetLimit) {
                *(myTarget++) = temp[indexToWrite];
                *(myOffsets++) = offsetNum;
            }
            else {
                args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] = temp[indexToWrite];
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
        }
        offsetNum = offsetNum + 1 + (temp[1] != 0);
    }

    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err)) {
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = (char *) myTarget;
    args->source = mySource;
    args->offsets = myOffsets;
}

static UChar32 U_CALLCONV
T_UConverter_getNextUChar_UTF32_BE(UConverterToUnicodeArgs* args,
                                   UErrorCode* err)
{
    const uint8_t *mySource;
    UChar32 myUChar;
    int32_t length;

    mySource = (const uint8_t *)args->source;
    if (mySource >= (const uint8_t *)args->sourceLimit)
    {
        /* no input */
        *err = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0xffff;
    }

    length = (int32_t)((const uint8_t *)args->sourceLimit - mySource);
    if (length < 4) 
    {
        /* got a partial character */
        uprv_memcpy(args->converter->toUBytes, mySource, length);
        args->converter->toULength = (int8_t)length;
        args->source = (const char *)(mySource + length);
        *err = U_TRUNCATED_CHAR_FOUND;
        return 0xffff;
    }

    /* Don't even try to do a direct cast because the value may be on an odd address. */
    myUChar = ((UChar32)mySource[0] << 24)
            | ((UChar32)mySource[1] << 16)
            | ((UChar32)mySource[2] << 8)
            | ((UChar32)mySource[3]);

    args->source = (const char *)(mySource + 4);
    if ((uint32_t)myUChar <= MAXIMUM_UTF && !U_IS_SURROGATE(myUChar)) {
        return myUChar;
    }

    uprv_memcpy(args->converter->toUBytes, mySource, 4);
    args->converter->toULength = 4;

    *err = U_ILLEGAL_CHAR_FOUND;
    return 0xffff;
}
U_CDECL_END
static const UConverterImpl _UTF32BEImpl = {
    UCNV_UTF32_BigEndian,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    T_UConverter_toUnicode_UTF32_BE,
    T_UConverter_toUnicode_UTF32_BE_OFFSET_LOGIC,
    T_UConverter_fromUnicode_UTF32_BE,
    T_UConverter_fromUnicode_UTF32_BE_OFFSET_LOGIC,
    T_UConverter_getNextUChar_UTF32_BE,

    NULL,
    NULL,
    NULL,
    NULL,
    ucnv_getNonSurrogateUnicodeSet,

    NULL,
    NULL
};

/* The 1232 CCSID refers to any version of Unicode with any endianness of UTF-32 */
static const UConverterStaticData _UTF32BEStaticData = {
    sizeof(UConverterStaticData),
    "UTF-32BE",
    1232,
    UCNV_IBM, UCNV_UTF32_BigEndian, 4, 4,
    { 0, 0, 0xff, 0xfd }, 4, false, false,
    0,
    0,
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } /* reserved */
};

const UConverterSharedData _UTF32BEData =
        UCNV_IMMUTABLE_SHARED_DATA_INITIALIZER(&_UTF32BEStaticData, &_UTF32BEImpl);

/* UTF-32LE ---------------------------------------------------------- */
U_CDECL_BEGIN
static void U_CALLCONV
T_UConverter_toUnicode_UTF32_LE(UConverterToUnicodeArgs * args,
                                UErrorCode * err)
{
    const unsigned char *mySource = (unsigned char *) args->source;
    UChar *myTarget = args->target;
    const unsigned char *sourceLimit = (unsigned char *) args->sourceLimit;
    const UChar *targetLimit = args->targetLimit;
    unsigned char *toUBytes = args->converter->toUBytes;
    uint32_t ch, i;

    /* Restore state of current sequence */
    if (args->converter->toULength > 0 && myTarget < targetLimit)
    {
        i = args->converter->toULength;       /* restore # of bytes consumed */
        args->converter->toULength = 0;

        /* Stores the previously calculated ch from a previous call*/
        ch = args->converter->toUnicodeStatus - 1;
        args->converter->toUnicodeStatus = 0;
        goto morebytes;
    }

    while (mySource < sourceLimit && myTarget < targetLimit)
    {
        i = 0;
        ch = 0;
morebytes:
        while (i < sizeof(uint32_t))
        {
            if (mySource < sourceLimit)
            {
                ch |= ((uint8_t)(*mySource)) << (i * 8);
                toUBytes[i++] = (char) *(mySource++);
            }
            else
            {
                /* stores a partially calculated target*/
                /* + 1 to make 0 a valid character */
                args->converter->toUnicodeStatus = ch + 1;
                args->converter->toULength = (int8_t) i;
                goto donefornow;
            }
        }

        if (ch <= MAXIMUM_UTF && !U_IS_SURROGATE(ch)) {
            /* Normal valid byte when the loop has not prematurely terminated (i < inBytes) */
            if (ch <= MAXIMUM_UCS2) {
                /* fits in 16 bits */
                *(myTarget++) = (UChar) ch;
            }
            else {
                /* write out the surrogates */
                *(myTarget++) = U16_LEAD(ch);
                ch = U16_TRAIL(ch);
                if (myTarget < targetLimit) {
                    *(myTarget++) = (UChar)ch;
                }
                else {
                    /* Put in overflow buffer (not handled here) */
                    args->converter->UCharErrorBuffer[0] = (UChar) ch;
                    args->converter->UCharErrorBufferLength = 1;
                    *err = U_BUFFER_OVERFLOW_ERROR;
                    break;
                }
            }
        }
        else {
            args->converter->toULength = (int8_t)i;
            *err = U_ILLEGAL_CHAR_FOUND;
            break;
        }
    }

donefornow:
    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err))
    {
        /* End of target buffer */
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = myTarget;
    args->source = (const char *) mySource;
}

static void U_CALLCONV
T_UConverter_toUnicode_UTF32_LE_OFFSET_LOGIC(UConverterToUnicodeArgs * args,
                                             UErrorCode * err)
{
    const unsigned char *mySource = (unsigned char *) args->source;
    UChar *myTarget = args->target;
    int32_t *myOffsets = args->offsets;
    const unsigned char *sourceLimit = (unsigned char *) args->sourceLimit;
    const UChar *targetLimit = args->targetLimit;
    unsigned char *toUBytes = args->converter->toUBytes;
    uint32_t ch, i;
    int32_t offsetNum = 0;

    /* Restore state of current sequence */
    if (args->converter->toULength > 0 && myTarget < targetLimit)
    {
        i = args->converter->toULength;       /* restore # of bytes consumed */
        args->converter->toULength = 0;

        /* Stores the previously calculated ch from a previous call*/
        ch = args->converter->toUnicodeStatus - 1;
        args->converter->toUnicodeStatus = 0;
        goto morebytes;
    }

    while (mySource < sourceLimit && myTarget < targetLimit)
    {
        i = 0;
        ch = 0;
morebytes:
        while (i < sizeof(uint32_t))
        {
            if (mySource < sourceLimit)
            {
                ch |= ((uint8_t)(*mySource)) << (i * 8);
                toUBytes[i++] = (char) *(mySource++);
            }
            else
            {
                /* stores a partially calculated target*/
                /* + 1 to make 0 a valid character */
                args->converter->toUnicodeStatus = ch + 1;
                args->converter->toULength = (int8_t) i;
                goto donefornow;
            }
        }

        if (ch <= MAXIMUM_UTF && !U_IS_SURROGATE(ch))
        {
            /* Normal valid byte when the loop has not prematurely terminated (i < inBytes) */
            if (ch <= MAXIMUM_UCS2) 
            {
                /* fits in 16 bits */
                *(myTarget++) = (UChar) ch;
                *(myOffsets++) = offsetNum;
            }
            else {
                /* write out the surrogates */
                *(myTarget++) = U16_LEAD(ch);
                *(myOffsets++) = offsetNum;
                ch = U16_TRAIL(ch);
                if (myTarget < targetLimit)
                {
                    *(myTarget++) = (UChar)ch;
                    *(myOffsets++) = offsetNum;
                }
                else
                {
                    /* Put in overflow buffer (not handled here) */
                    args->converter->UCharErrorBuffer[0] = (UChar) ch;
                    args->converter->UCharErrorBufferLength = 1;
                    *err = U_BUFFER_OVERFLOW_ERROR;
                    break;
                }
            }
        }
        else
        {
            args->converter->toULength = (int8_t)i;
            *err = U_ILLEGAL_CHAR_FOUND;
            break;
        }
        offsetNum += i;
    }

donefornow:
    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err))
    {
        /* End of target buffer */
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = myTarget;
    args->source = (const char *) mySource;
    args->offsets = myOffsets;
}

static void U_CALLCONV
T_UConverter_fromUnicode_UTF32_LE(UConverterFromUnicodeArgs * args,
                                  UErrorCode * err)
{
    const UChar *mySource = args->source;
    unsigned char *myTarget;
    const UChar *sourceLimit = args->sourceLimit;
    const unsigned char *targetLimit = (unsigned char *) args->targetLimit;
    UChar32 ch, ch2;
    unsigned int indexToWrite;
    unsigned char temp[sizeof(uint32_t)];

    if(mySource >= sourceLimit) {
        /* no input, nothing to do */
        return;
    }

    /* write the BOM if necessary */
    if(args->converter->fromUnicodeStatus==UCNV_NEED_TO_WRITE_BOM) {
        static const char bom[]={ (char)0xffu, (char)0xfeu, 0, 0 };
        ucnv_fromUWriteBytes(args->converter,
                             bom, 4,
                             &args->target, args->targetLimit,
                             &args->offsets, -1,
                             err);
        args->converter->fromUnicodeStatus=0;
    }

    myTarget = (unsigned char *) args->target;
    temp[3] = 0;

    if (args->converter->fromUChar32)
    {
        ch = args->converter->fromUChar32;
        args->converter->fromUChar32 = 0;
        goto lowsurogate;
    }

    while (mySource < sourceLimit && myTarget < targetLimit)
    {
        ch = *(mySource++);

        if (U16_IS_SURROGATE(ch)) {
            if (U16_IS_LEAD(ch))
            {
lowsurogate:
                if (mySource < sourceLimit)
                {
                    ch2 = *mySource;
                    if (U16_IS_TRAIL(ch2)) {
                        ch = ((ch - SURROGATE_HIGH_START) << HALF_SHIFT) + ch2 + SURROGATE_LOW_BASE;
                        mySource++;
                    }
                    else {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        args->converter->fromUChar32 = ch;
                        *err = U_ILLEGAL_CHAR_FOUND;
                        break;
                    }
                }
                else {
                    /* ran out of source */
                    args->converter->fromUChar32 = ch;
                    if (args->flush) {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        *err = U_ILLEGAL_CHAR_FOUND;
                    }
                    break;
                }
            }
            else {
                /* this is an unmatched trail code unit (2nd surrogate) */
                /* callback(illegal) */
                args->converter->fromUChar32 = ch;
                *err = U_ILLEGAL_CHAR_FOUND;
                break;
            }
        }

        /* We cannot get any larger than 10FFFF because we are coming from UTF-16 */
        temp[2] = (uint8_t) (ch >> 16 & 0x1F);
        temp[1] = (uint8_t) (ch >> 8);  /* unsigned cast implicitly does (ch & FF) */
        temp[0] = (uint8_t) (ch);       /* unsigned cast implicitly does (ch & FF) */

        for (indexToWrite = 0; indexToWrite <= sizeof(uint32_t) - 1; indexToWrite++)
        {
            if (myTarget < targetLimit)
            {
                *(myTarget++) = temp[indexToWrite];
            }
            else
            {
                args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] = temp[indexToWrite];
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
        }
    }

    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err))
    {
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = (char *) myTarget;
    args->source = mySource;
}

static void U_CALLCONV
T_UConverter_fromUnicode_UTF32_LE_OFFSET_LOGIC(UConverterFromUnicodeArgs * args,
                                               UErrorCode * err)
{
    const UChar *mySource = args->source;
    unsigned char *myTarget;
    int32_t *myOffsets;
    const UChar *sourceLimit = args->sourceLimit;
    const unsigned char *targetLimit = (unsigned char *) args->targetLimit;
    UChar32 ch, ch2;
    unsigned int indexToWrite;
    unsigned char temp[sizeof(uint32_t)];
    int32_t offsetNum = 0;

    if(mySource >= sourceLimit) {
        /* no input, nothing to do */
        return;
    }

    /* write the BOM if necessary */
    if(args->converter->fromUnicodeStatus==UCNV_NEED_TO_WRITE_BOM) {
        static const char bom[]={ (char)0xffu, (char)0xfeu, 0, 0 };
        ucnv_fromUWriteBytes(args->converter,
                             bom, 4,
                             &args->target, args->targetLimit,
                             &args->offsets, -1,
                             err);
        args->converter->fromUnicodeStatus=0;
    }

    myTarget = (unsigned char *) args->target;
    myOffsets = args->offsets;
    temp[3] = 0;

    if (args->converter->fromUChar32)
    {
        ch = args->converter->fromUChar32;
        args->converter->fromUChar32 = 0;
        goto lowsurogate;
    }

    while (mySource < sourceLimit && myTarget < targetLimit)
    {
        ch = *(mySource++);

        if (U16_IS_SURROGATE(ch)) {
            if (U16_IS_LEAD(ch))
            {
lowsurogate:
                if (mySource < sourceLimit)
                {
                    ch2 = *mySource;
                    if (U16_IS_TRAIL(ch2))
                    {
                        ch = ((ch - SURROGATE_HIGH_START) << HALF_SHIFT) + ch2 + SURROGATE_LOW_BASE;
                        mySource++;
                    }
                    else {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        args->converter->fromUChar32 = ch;
                        *err = U_ILLEGAL_CHAR_FOUND;
                        break;
                    }
                }
                else {
                    /* ran out of source */
                    args->converter->fromUChar32 = ch;
                    if (args->flush) {
                        /* this is an unmatched trail code unit (2nd surrogate) */
                        /* callback(illegal) */
                        *err = U_ILLEGAL_CHAR_FOUND;
                    }
                    break;
                }
            }
            else {
                /* this is an unmatched trail code unit (2nd surrogate) */
                /* callback(illegal) */
                args->converter->fromUChar32 = ch;
                *err = U_ILLEGAL_CHAR_FOUND;
                break;
            }
        }

        /* We cannot get any larger than 10FFFF because we are coming from UTF-16 */
        temp[2] = (uint8_t) (ch >> 16 & 0x1F);
        temp[1] = (uint8_t) (ch >> 8);  /* unsigned cast implicitly does (ch & FF) */
        temp[0] = (uint8_t) (ch);       /* unsigned cast implicitly does (ch & FF) */

        for (indexToWrite = 0; indexToWrite <= sizeof(uint32_t) - 1; indexToWrite++)
        {
            if (myTarget < targetLimit)
            {
                *(myTarget++) = temp[indexToWrite];
                *(myOffsets++) = offsetNum;
            }
            else
            {
                args->converter->charErrorBuffer[args->converter->charErrorBufferLength++] = temp[indexToWrite];
                *err = U_BUFFER_OVERFLOW_ERROR;
            }
        }
        offsetNum = offsetNum + 1 + (temp[2] != 0);
    }

    if (mySource < sourceLimit && myTarget >= targetLimit && U_SUCCESS(*err))
    {
        *err = U_BUFFER_OVERFLOW_ERROR;
    }

    args->target = (char *) myTarget;
    args->source = mySource;
    args->offsets = myOffsets;
}

static UChar32 U_CALLCONV
T_UConverter_getNextUChar_UTF32_LE(UConverterToUnicodeArgs* args,
                                   UErrorCode* err)
{
    const uint8_t *mySource;
    UChar32 myUChar;
    int32_t length;

    mySource = (const uint8_t *)args->source;
    if (mySource >= (const uint8_t *)args->sourceLimit)
    {
        /* no input */
        *err = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0xffff;
    }

    length = (int32_t)((const uint8_t *)args->sourceLimit - mySource);
    if (length < 4) 
    {
        /* got a partial character */
        uprv_memcpy(args->converter->toUBytes, mySource, length);
        args->converter->toULength = (int8_t)length;
        args->source = (const char *)(mySource + length);
        *err = U_TRUNCATED_CHAR_FOUND;
        return 0xffff;
    }

    /* Don't even try to do a direct cast because the value may be on an odd address. */
    myUChar = ((UChar32)mySource[3] << 24)
            | ((UChar32)mySource[2] << 16)
            | ((UChar32)mySource[1] << 8)
            | ((UChar32)mySource[0]);

    args->source = (const char *)(mySource + 4);
    if ((uint32_t)myUChar <= MAXIMUM_UTF && !U_IS_SURROGATE(myUChar)) {
        return myUChar;
    }

    uprv_memcpy(args->converter->toUBytes, mySource, 4);
    args->converter->toULength = 4;

    *err = U_ILLEGAL_CHAR_FOUND;
    return 0xffff;
}
U_CDECL_END
static const UConverterImpl _UTF32LEImpl = {
    UCNV_UTF32_LittleEndian,

    NULL,
    NULL,

    NULL,
    NULL,
    NULL,

    T_UConverter_toUnicode_UTF32_LE,
    T_UConverter_toUnicode_UTF32_LE_OFFSET_LOGIC,
    T_UConverter_fromUnicode_UTF32_LE,
    T_UConverter_fromUnicode_UTF32_LE_OFFSET_LOGIC,
    T_UConverter_getNextUChar_UTF32_LE,

    NULL,
    NULL,
    NULL,
    NULL,
    ucnv_getNonSurrogateUnicodeSet,

    NULL,
    NULL
};

/* The 1232 CCSID refers to any version of Unicode with any endianness of UTF-32 */
static const UConverterStaticData _UTF32LEStaticData = {
    sizeof(UConverterStaticData),
    "UTF-32LE",
    1234,
    UCNV_IBM, UCNV_UTF32_LittleEndian, 4, 4,
    { 0xfd, 0xff, 0, 0 }, 4, false, false,
    0,
    0,
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } /* reserved */
};


const UConverterSharedData _UTF32LEData =
        UCNV_IMMUTABLE_SHARED_DATA_INITIALIZER(&_UTF32LEStaticData, &_UTF32LEImpl);

/* UTF-32 (Detect BOM) ------------------------------------------------------ */

/*
 * Detect a BOM at the beginning of the stream and select UTF-32BE or UTF-32LE
 * accordingly.
 *
 * State values:
 * 0    initial state
 * 1    saw 00
 * 2    saw 00 00
 * 3    saw 00 00 FE
 * 4    -
 * 5    saw FF
 * 6    saw FF FE
 * 7    saw FF FE 00
 * 8    UTF-32BE mode
 * 9    UTF-32LE mode
 *
 * During detection: state&3==number of matching bytes so far.
 *
 * On output, emit U+FEFF as the first code point.
 */
U_CDECL_BEGIN
static void U_CALLCONV
_UTF32Reset(UConverter *cnv, UConverterResetChoice choice) {
    if(choice<=UCNV_RESET_TO_UNICODE) {
        /* reset toUnicode: state=0 */
        cnv->mode=0;
    }
    if(choice!=UCNV_RESET_TO_UNICODE) {
        /* reset fromUnicode: prepare to output the UTF-32PE BOM */
        cnv->fromUnicodeStatus=UCNV_NEED_TO_WRITE_BOM;
    }
}

static void U_CALLCONV
_UTF32Open(UConverter *cnv,
           UConverterLoadArgs *pArgs,
           UErrorCode *pErrorCode) {
    (void)pArgs;
    (void)pErrorCode;
    _UTF32Reset(cnv, UCNV_RESET_BOTH);
}

static const char utf32BOM[8]={ 0, 0, (char)0xfeu, (char)0xffu, (char)0xffu, (char)0xfeu, 0, 0 };

static void U_CALLCONV
_UTF32ToUnicodeWithOffsets(UConverterToUnicodeArgs *pArgs,
                           UErrorCode *pErrorCode) {
    UConverter *cnv=pArgs->converter;
    const char *source=pArgs->source;
    const char *sourceLimit=pArgs->sourceLimit;
    int32_t *offsets=pArgs->offsets;

    int32_t state, offsetDelta;
    char b;

    state=cnv->mode;

    /*
     * If we detect a BOM in this buffer, then we must add the BOM size to the
     * offsets because the actual converter function will not see and count the BOM.
     * offsetDelta will have the number of the BOM bytes that are in the current buffer.
     */
    offsetDelta=0;

    while(source<sourceLimit && U_SUCCESS(*pErrorCode)) {
        switch(state) {
        case 0:
            b=*source;
            if(b==0) {
                state=1; /* could be 00 00 FE FF */
            } else if(b==(char)0xffu) {
                state=5; /* could be FF FE 00 00 */
            } else {
                state=8; /* default to UTF-32BE */
                continue;
            }
            ++source;
            break;
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
        case 7:
            if(*source==utf32BOM[state]) {
                ++state;
                ++source;
                if(state==4) {
                    state=8; /* detect UTF-32BE */
                    offsetDelta=(int32_t)(source-pArgs->source);
                } else if(state==8) {
                    state=9; /* detect UTF-32LE */
                    offsetDelta=(int32_t)(source-pArgs->source);
                }
            } else {
                /* switch to UTF-32BE and pass the previous bytes */
                int32_t count=(int32_t)(source-pArgs->source); /* number of bytes from this buffer */

                /* reset the source */
                source=pArgs->source;

                if(count==(state&3)) {
                    /* simple: all in the same buffer, just reset source */
                } else {
                    UBool oldFlush=pArgs->flush;

                    /* some of the bytes are from a previous buffer, replay those first */
                    pArgs->source=utf32BOM+(state&4); /* select the correct BOM */
                    pArgs->sourceLimit=pArgs->source+((state&3)-count); /* replay previous bytes */
                    pArgs->flush=false; /* this sourceLimit is not the real source stream limit */

                    /* no offsets: bytes from previous buffer, and not enough for output */
                    T_UConverter_toUnicode_UTF32_BE(pArgs, pErrorCode);

                    /* restore real pointers; pArgs->source will be set in case 8/9 */
                    pArgs->sourceLimit=sourceLimit;
                    pArgs->flush=oldFlush;
                }
                state=8;
                continue;
            }
            break;
        case 8:
            /* call UTF-32BE */
            pArgs->source=source;
            if(offsets==NULL) {
                T_UConverter_toUnicode_UTF32_BE(pArgs, pErrorCode);
            } else {
                T_UConverter_toUnicode_UTF32_BE_OFFSET_LOGIC(pArgs, pErrorCode);
            }
            source=pArgs->source;
            break;
        case 9:
            /* call UTF-32LE */
            pArgs->source=source;
            if(offsets==NULL) {
                T_UConverter_toUnicode_UTF32_LE(pArgs, pErrorCode);
            } else {
                T_UConverter_toUnicode_UTF32_LE_OFFSET_LOGIC(pArgs, pErrorCode);
            }
            source=pArgs->source;
            break;
        default:
            break; /* does not occur */
        }
    }

    /* add BOM size to offsets - see comment at offsetDelta declaration */
    if(offsets!=NULL && offsetDelta!=0) {
        int32_t *offsetsLimit=pArgs->offsets;
        while(offsets<offsetsLimit) {
            *offsets++ += offsetDelta;
        }
    }

    pArgs->source=source;

    if(source==sourceLimit && pArgs->flush) {
        /* handle truncated input */
        switch(state) {
        case 0:
            break; /* no input at all, nothing to do */
        case 8:
            T_UConverter_toUnicode_UTF32_BE(pArgs, pErrorCode);
            break;
        case 9:
            T_UConverter_toUnicode_UTF32_LE(pArgs, pErrorCode);
            break;
        default:
            /* handle 0<state<8: call UTF-32BE with too-short input */
            pArgs->source=utf32BOM+(state&4); /* select the correct BOM */
            pArgs->sourceLimit=pArgs->source+(state&3); /* replay bytes */

            /* no offsets: not enough for output */
            T_UConverter_toUnicode_UTF32_BE(pArgs, pErrorCode);
            pArgs->source=source;
            pArgs->sourceLimit=sourceLimit;
            state=8;
            break;
        }
    }

    cnv->mode=state;
}

static UChar32 U_CALLCONV
_UTF32GetNextUChar(UConverterToUnicodeArgs *pArgs,
                   UErrorCode *pErrorCode) {
    switch(pArgs->converter->mode) {
    case 8:
        return T_UConverter_getNextUChar_UTF32_BE(pArgs, pErrorCode);
    case 9:
        return T_UConverter_getNextUChar_UTF32_LE(pArgs, pErrorCode);
    default:
        return UCNV_GET_NEXT_UCHAR_USE_TO_U;
    }
}
U_CDECL_END
static const UConverterImpl _UTF32Impl = {
    UCNV_UTF32,

    NULL,
    NULL,

    _UTF32Open,
    NULL,
    _UTF32Reset,

    _UTF32ToUnicodeWithOffsets,
    _UTF32ToUnicodeWithOffsets,
#if U_IS_BIG_ENDIAN
    T_UConverter_fromUnicode_UTF32_BE,
    T_UConverter_fromUnicode_UTF32_BE_OFFSET_LOGIC,
#else
    T_UConverter_fromUnicode_UTF32_LE,
    T_UConverter_fromUnicode_UTF32_LE_OFFSET_LOGIC,
#endif
    _UTF32GetNextUChar,

    NULL, /* ### TODO implement getStarters for all Unicode encodings?! */
    NULL,
    NULL,
    NULL,
    ucnv_getNonSurrogateUnicodeSet,

    NULL,
    NULL
};

/* The 1236 CCSID refers to any version of Unicode with a BOM sensitive endianness of UTF-32 */
static const UConverterStaticData _UTF32StaticData = {
    sizeof(UConverterStaticData),
    "UTF-32",
    1236,
    UCNV_IBM, UCNV_UTF32, 4, 4,
#if U_IS_BIG_ENDIAN
    { 0, 0, 0xff, 0xfd }, 4,
#else
    { 0xfd, 0xff, 0, 0 }, 4,
#endif
    false, false,
    0,
    0,
    { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } /* reserved */
};

const UConverterSharedData _UTF32Data = 
        UCNV_IMMUTABLE_SHARED_DATA_INITIALIZER(&_UTF32StaticData, &_UTF32Impl);

#endif
