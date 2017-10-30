// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitaffix.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __DIGITAFFIX_H__
#define __DIGITAFFIX_H__

#include "unicode/uobject.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/utypes.h"

U_NAMESPACE_BEGIN

class FieldPositionHandler;

/**
 * A prefix or suffix of a formatted number.
 */
class U_I18N_API DigitAffix : public UMemory {
public:

    /**
     * Creates an empty DigitAffix.
     */
    DigitAffix();

    /**
     * Creates a DigitAffix containing given UChars where all of it has
     * a field type of fieldId.
     */
    DigitAffix(
            const UChar *value,
            int32_t charCount,
            int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Makes this affix be the empty string.
     */
    void remove();

    /**
     * Append value to this affix. If fieldId is present, the appended
     * string is considered to be the type fieldId.
     */
    void appendUChar(UChar value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append value to this affix. If fieldId is present, the appended
     * string is considered to be the type fieldId.
     */
    void append(const UnicodeString &value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Sets this affix to given string. The entire string
     * is considered to be the type fieldId.
     */
    void setTo(const UnicodeString &value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append value to this affix. If fieldId is present, the appended
     * string is considered to be the type fieldId.
     */
    void append(const UChar *value, int32_t charCount, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Formats this affix.
     */
    UnicodeString &format(
            FieldPositionHandler &handler, UnicodeString &appendTo) const;
    int32_t countChar32() const { return fAffix.countChar32(); }

    /**
     * Returns this affix as a unicode string.
     */
    const UnicodeString & toString() const { return fAffix; }

    /**
     * Returns TRUE if this object equals rhs.
     */
    UBool equals(const DigitAffix &rhs) const {
        return ((fAffix == rhs.fAffix) && (fAnnotations == rhs.fAnnotations));
    }
private:
    UnicodeString fAffix;
    UnicodeString fAnnotations;
};


U_NAMESPACE_END
#endif // #if !UCONFIG_NO_FORMATTING
#endif  // __DIGITAFFIX_H__
