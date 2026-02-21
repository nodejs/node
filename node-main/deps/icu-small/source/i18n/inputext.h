// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2008, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __INPUTEXT_H
#define __INPUTEXT_H

/**
 * \file
 * \internal
 *
 * This is an internal header for the Character Set Detection code. The
 * name is probably too generic...
 */


#include "unicode/uobject.h"

#if !UCONFIG_NO_CONVERSION

U_NAMESPACE_BEGIN 

class InputText : public UMemory
{
    // Prevent copying
    InputText(const InputText &);
public:
    InputText(UErrorCode &status);
    ~InputText();

    void setText(const char *in, int32_t len);
    void setDeclaredEncoding(const char *encoding, int32_t len);
    UBool isSet() const; 
    void MungeInput(UBool fStripTags);

    // The text to be checked.  Markup will have been
    //   removed if appropriate.
    uint8_t    *fInputBytes;
    int32_t     fInputLen;          // Length of the byte data in fInputBytes.
    // byte frequency statistics for the input text.
    //   Value is percent, not absolute.
    //   Value is rounded up, so zero really means zero occurrences. 
    int16_t  *fByteStats;
    UBool     fC1Bytes;          // True if any bytes in the range 0x80 - 0x9F are in the input;false by default
    char     *fDeclaredEncoding;

    const uint8_t           *fRawInput;     // Original, untouched input bytes.
    //  If user gave us a byte array, this is it.
    //  If user gave us a stream, it's read to a 
    //   buffer here.
    int32_t                  fRawLength;    // Length of data in fRawInput array.

};

U_NAMESPACE_END

#endif
#endif /* __INPUTEXT_H */
