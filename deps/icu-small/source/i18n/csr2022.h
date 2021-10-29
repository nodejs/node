// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2015, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __CSR2022_H
#define __CSR2022_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "csrecog.h"

U_NAMESPACE_BEGIN

class CharsetMatch;

/**
 *  class CharsetRecog_2022  part of the ICU charset detection implementation.
 *                           This is a superclass for the individual detectors for
 *                           each of the detectable members of the ISO 2022 family
 *                           of encodings.
 * 
 *                           The separate classes are nested within this class.
 * 
 * @internal
 */
class CharsetRecog_2022 : public CharsetRecognizer
{

public:    
    virtual ~CharsetRecog_2022() = 0;

protected:

    /**
     * Matching function shared among the 2022 detectors JP, CN and KR
     * Counts up the number of legal an unrecognized escape sequences in
     * the sample of text, and computes a score based on the total number &
     * the proportion that fit the encoding.
     * 
     * 
     * @param text the byte buffer containing text to analyse
     * @param textLen  the size of the text in the byte.
     * @param escapeSequences the byte escape sequences to test for.
     * @return match quality, in the range of 0-100.
     */
    int32_t match_2022(const uint8_t *text,
                       int32_t textLen,
                       const uint8_t escapeSequences[][5],
                       int32_t escapeSequences_length) const;

};

class CharsetRecog_2022JP :public CharsetRecog_2022
{
public:
    virtual ~CharsetRecog_2022JP();

    const char *getName() const override;

    UBool match(InputText *textIn, CharsetMatch *results) const override;
};

#if !UCONFIG_ONLY_HTML_CONVERSION
class CharsetRecog_2022KR :public CharsetRecog_2022 {
public:
    virtual ~CharsetRecog_2022KR();

    const char *getName() const override;

    UBool match(InputText *textIn, CharsetMatch *results) const override;

};

class CharsetRecog_2022CN :public CharsetRecog_2022
{
public:
    virtual ~CharsetRecog_2022CN();

    const char* getName() const override;

    UBool match(InputText *textIn, CharsetMatch *results) const override;
};
#endif

U_NAMESPACE_END

#endif
#endif /* __CSR2022_H */
