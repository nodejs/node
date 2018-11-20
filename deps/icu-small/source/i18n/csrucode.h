// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2005-2012, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#ifndef __CSRUCODE_H
#define __CSRUCODE_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "csrecog.h"

U_NAMESPACE_BEGIN

/**
 * This class matches UTF-16 and UTF-32, both big- and little-endian. The
 * BOM will be used if it is present.
 *
 * @internal
 */
class CharsetRecog_Unicode : public CharsetRecognizer
{

public:

    virtual ~CharsetRecog_Unicode();
    /* (non-Javadoc)
     * @see com.ibm.icu.text.CharsetRecognizer#getName()
     */
    const char* getName() const = 0;

    /* (non-Javadoc)
     * @see com.ibm.icu.text.CharsetRecognizer#match(com.ibm.icu.text.CharsetDetector)
     */
    UBool match(InputText* textIn, CharsetMatch *results) const = 0;
};


class CharsetRecog_UTF_16_BE : public CharsetRecog_Unicode
{
public:

    virtual ~CharsetRecog_UTF_16_BE();

    const char *getName() const;

    UBool match(InputText* textIn, CharsetMatch *results) const;
};

class CharsetRecog_UTF_16_LE : public CharsetRecog_Unicode
{
public:

    virtual ~CharsetRecog_UTF_16_LE();

    const char *getName() const;

    UBool match(InputText* textIn, CharsetMatch *results) const;
};

class CharsetRecog_UTF_32 : public CharsetRecog_Unicode
{
protected:
    virtual int32_t getChar(const uint8_t *input, int32_t index) const = 0;
public:

    virtual ~CharsetRecog_UTF_32();

    const char* getName() const = 0;

    UBool match(InputText* textIn, CharsetMatch *results) const;
};


class CharsetRecog_UTF_32_BE : public CharsetRecog_UTF_32
{
protected:
    int32_t getChar(const uint8_t *input, int32_t index) const;

public:

    virtual ~CharsetRecog_UTF_32_BE();

    const char *getName() const;
};


class CharsetRecog_UTF_32_LE : public CharsetRecog_UTF_32
{
protected:
    int32_t getChar(const uint8_t *input, int32_t index) const;

public:
    virtual ~CharsetRecog_UTF_32_LE();

    const char* getName() const;
};

U_NAMESPACE_END

#endif
#endif /* __CSRUCODE_H */
