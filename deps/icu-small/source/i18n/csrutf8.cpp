// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2014, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "csrutf8.h"
#include "csmatch.h"

U_NAMESPACE_BEGIN

CharsetRecog_UTF8::~CharsetRecog_UTF8()
{
    // nothing to do
}

const char *CharsetRecog_UTF8::getName() const
{
    return "UTF-8";
}

UBool CharsetRecog_UTF8::match(InputText* input, CharsetMatch *results) const {
    bool hasBOM = false;
    int32_t numValid = 0;
    int32_t numInvalid = 0;
    const uint8_t *inputBytes = input->fRawInput;
    int32_t i;
    int32_t trailBytes = 0;
    int32_t confidence;

    if (input->fRawLength >= 3 && 
        inputBytes[0] == 0xEF && inputBytes[1] == 0xBB && inputBytes[2] == 0xBF) {
            hasBOM = true;
    }

    // Scan for multi-byte sequences
    for (i=0; i < input->fRawLength; i += 1) {
        int32_t b = inputBytes[i];

        if ((b & 0x80) == 0) {
            continue;   // ASCII
        }

        // Hi bit on char found.  Figure out how long the sequence should be
        if ((b & 0x0E0) == 0x0C0) {
            trailBytes = 1;
        } else if ((b & 0x0F0) == 0x0E0) {
            trailBytes = 2;
        } else if ((b & 0x0F8) == 0xF0) {
            trailBytes = 3;
        } else {
            numInvalid += 1;
            continue;
        }

        // Verify that we've got the right number of trail bytes in the sequence
        for (;;) {
            i += 1;

            if (i >= input->fRawLength) {
                break;
            }

            b = inputBytes[i];

            if ((b & 0xC0) != 0x080) {
                numInvalid += 1;
                break;
            }

            if (--trailBytes == 0) {
                numValid += 1;
                break;
            }
        }

    }

    // Cook up some sort of confidence score, based on presence of a BOM
    //    and the existence of valid and/or invalid multi-byte sequences.
    confidence = 0;
    if (hasBOM && numInvalid == 0) {
        confidence = 100;
    } else if (hasBOM && numValid > numInvalid*10) {
        confidence = 80;
    } else if (numValid > 3 && numInvalid == 0) {
        confidence = 100;
    } else if (numValid > 0 && numInvalid == 0) {
        confidence = 80;
    } else if (numValid == 0 && numInvalid == 0) {
        // Plain ASCII. Confidence must be > 10, it's more likely than UTF-16, which
        //              accepts ASCII with confidence = 10.
        confidence = 15;
    } else if (numValid > numInvalid*10) {
        // Probably corrupt utf-8 data.  Valid sequences aren't likely by chance.
        confidence = 25;
    }

    results->set(input, this, confidence);
    return (confidence > 0);
}

U_NAMESPACE_END
#endif
