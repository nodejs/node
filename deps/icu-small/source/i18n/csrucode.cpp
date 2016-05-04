/*
 **********************************************************************
 *   Copyright (C) 2005-2013, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "csrucode.h"
#include "csmatch.h"

U_NAMESPACE_BEGIN

CharsetRecog_Unicode::~CharsetRecog_Unicode()
{
    // nothing to do
}

CharsetRecog_UTF_16_BE::~CharsetRecog_UTF_16_BE()
{
    // nothing to do
}

const char *CharsetRecog_UTF_16_BE::getName() const
{
    return "UTF-16BE";
}

// UTF-16 confidence calculation. Very simple minded, but better than nothing.
//   Any 8 bit non-control characters bump the confidence up. These have a zero high byte,
//     and are very likely to be UTF-16, although they could also be part of a UTF-32 code.
//   NULs are a contra-indication, they will appear commonly if the actual encoding is UTF-32.
//   NULs should be rare in actual text. 

static int32_t adjustConfidence(UChar codeUnit, int32_t confidence) {
    if (codeUnit == 0) {
        confidence -= 10;
    } else if ((codeUnit >= 0x20 && codeUnit <= 0xff) || codeUnit == 0x0a) {
        confidence += 10;
    }
    if (confidence < 0) {
        confidence = 0;
    } else if (confidence > 100) {
        confidence = 100;
    }
    return confidence;
}


UBool CharsetRecog_UTF_16_BE::match(InputText* textIn, CharsetMatch *results) const
{
    const uint8_t *input = textIn->fRawInput;
    int32_t confidence = 10;
    int32_t length = textIn->fRawLength;

    int32_t bytesToCheck = (length > 30) ? 30 : length;
    for (int32_t charIndex=0; charIndex<bytesToCheck-1; charIndex+=2) {
        UChar codeUnit = (input[charIndex] << 8) | input[charIndex + 1];
        if (charIndex == 0 && codeUnit == 0xFEFF) {
            confidence = 100;
            break;
        }
        confidence = adjustConfidence(codeUnit, confidence);
        if (confidence == 0 || confidence == 100) {
            break;
        }
    }
    if (bytesToCheck < 4 && confidence < 100) {
        confidence = 0;
    }
    results->set(textIn, this, confidence);
    return (confidence > 0);
}

CharsetRecog_UTF_16_LE::~CharsetRecog_UTF_16_LE()
{
    // nothing to do
}

const char *CharsetRecog_UTF_16_LE::getName() const
{
    return "UTF-16LE";
}

UBool CharsetRecog_UTF_16_LE::match(InputText* textIn, CharsetMatch *results) const
{
    const uint8_t *input = textIn->fRawInput;
    int32_t confidence = 10;
    int32_t length = textIn->fRawLength;

    int32_t bytesToCheck = (length > 30) ? 30 : length;
    for (int32_t charIndex=0; charIndex<bytesToCheck-1; charIndex+=2) {
        UChar codeUnit = input[charIndex] | (input[charIndex + 1] << 8);
        if (charIndex == 0 && codeUnit == 0xFEFF) {
            confidence = 100;     // UTF-16 BOM
            if (length >= 4 && input[2] == 0 && input[3] == 0) {
                confidence = 0;   // UTF-32 BOM
            }
            break;
        }
        confidence = adjustConfidence(codeUnit, confidence);
        if (confidence == 0 || confidence == 100) {
            break;
        }
    }
    if (bytesToCheck < 4 && confidence < 100) {
        confidence = 0;
    }
    results->set(textIn, this, confidence);
    return (confidence > 0);
}

CharsetRecog_UTF_32::~CharsetRecog_UTF_32()
{
    // nothing to do
}

UBool CharsetRecog_UTF_32::match(InputText* textIn, CharsetMatch *results) const
{
    const uint8_t *input = textIn->fRawInput;
    int32_t limit = (textIn->fRawLength / 4) * 4;
    int32_t numValid = 0;
    int32_t numInvalid = 0;
    bool hasBOM = FALSE;
    int32_t confidence = 0;

    if (limit > 0 && getChar(input, 0) == 0x0000FEFFUL) {
        hasBOM = TRUE;
    }

    for(int32_t i = 0; i < limit; i += 4) {
        int32_t ch = getChar(input, i);

        if (ch < 0 || ch >= 0x10FFFF || (ch >= 0xD800 && ch <= 0xDFFF)) {
            numInvalid += 1;
        } else {
            numValid += 1;
        }
    }


    // Cook up some sort of confidence score, based on presense of a BOM
    //    and the existence of valid and/or invalid multi-byte sequences.
    if (hasBOM && numInvalid==0) {
        confidence = 100;
    } else if (hasBOM && numValid > numInvalid*10) {
        confidence = 80;
    } else if (numValid > 3 && numInvalid == 0) {
        confidence = 100;            
    } else if (numValid > 0 && numInvalid == 0) {
        confidence = 80;
    } else if (numValid > numInvalid*10) {
        // Probably corruput UTF-32BE data.  Valid sequences aren't likely by chance.
        confidence = 25;
    }

    results->set(textIn, this, confidence);
    return (confidence > 0);
}

CharsetRecog_UTF_32_BE::~CharsetRecog_UTF_32_BE()
{
    // nothing to do
}

const char *CharsetRecog_UTF_32_BE::getName() const
{
    return "UTF-32BE";
}

int32_t CharsetRecog_UTF_32_BE::getChar(const uint8_t *input, int32_t index) const
{
    return input[index + 0] << 24 | input[index + 1] << 16 |
           input[index + 2] <<  8 | input[index + 3];
} 

CharsetRecog_UTF_32_LE::~CharsetRecog_UTF_32_LE()
{
    // nothing to do
}

const char *CharsetRecog_UTF_32_LE::getName() const
{
    return "UTF-32LE";
}

int32_t CharsetRecog_UTF_32_LE::getChar(const uint8_t *input, int32_t index) const
{
    return input[index + 3] << 24 | input[index + 2] << 16 |
           input[index + 1] <<  8 | input[index + 0];
}

U_NAMESPACE_END
#endif

