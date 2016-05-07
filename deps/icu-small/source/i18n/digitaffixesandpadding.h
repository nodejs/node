/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitaffixesandpadding.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __DIGITAFFIXESANDPADDING_H__
#define __DIGITAFFIXESANDPADDING_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "pluralaffix.h"

U_NAMESPACE_BEGIN

class DigitList;
class ValueFormatter;
class UnicodeString;
class FieldPositionHandler;
class PluralRules;
class VisibleDigitsWithExponent;

/**
 * A formatter of numbers. This class can format any numerical value
 * except for not a number (NaN), positive infinity, and negative infinity.
 * This class manages prefixes, suffixes, and padding but delegates the
 * formatting of actual positive values to a ValueFormatter.
 */
class U_I18N_API DigitAffixesAndPadding : public UMemory {
public:

/**
 * Equivalent to DecimalFormat EPadPosition, but redeclared here to prevent
 * depending on DecimalFormat which would cause a circular dependency.
 */
enum EPadPosition {
    kPadBeforePrefix,
    kPadAfterPrefix,
    kPadBeforeSuffix,
    kPadAfterSuffix
};

/**
 * The positive prefix
 */
PluralAffix fPositivePrefix;

/**
 * The positive suffix
 */
PluralAffix fPositiveSuffix;

/**
 * The negative suffix
 */
PluralAffix fNegativePrefix;

/**
 * The negative suffix
 */
PluralAffix fNegativeSuffix;

/**
 * The padding position
 */
EPadPosition fPadPosition;

/**
 * The padding character.
 */
UChar32 fPadChar;

/**
 * The field width in code points. The format method inserts instances of
 * the padding character as needed in the desired padding position so that
 * the entire formatted string contains this many code points. If the
 * formatted string already exceeds this many code points, the format method
 * inserts no padding.
 */
int32_t fWidth;

/**
 * Pad position is before prefix; padding character is '*' field width is 0.
 * The affixes are all the empty string with no annotated fields with just
 * the 'other' plural variation.
 */
DigitAffixesAndPadding()
        : fPadPosition(kPadBeforePrefix), fPadChar(0x2a), fWidth(0) { }

/**
 * Returns TRUE if this object is equal to rhs.
 */
UBool equals(const DigitAffixesAndPadding &rhs) const {
    return (fPositivePrefix.equals(rhs.fPositivePrefix) &&
            fPositiveSuffix.equals(rhs.fPositiveSuffix) &&
            fNegativePrefix.equals(rhs.fNegativePrefix) &&
            fNegativeSuffix.equals(rhs.fNegativeSuffix) &&
            fPadPosition == rhs.fPadPosition &&
            fWidth == rhs.fWidth &&
            fPadChar == rhs.fPadChar);
}

/**
 * Returns TRUE if a plural rules instance is needed to complete the
 * formatting by detecting if any of the affixes have multiple plural
 * variations.
 */
UBool needsPluralRules() const;

/**
 * Formats value and appends to appendTo.
 *
 * @param value the value to format. May be NaN or ininite.
 * @param formatter handles the details of formatting the actual value.
 * @param handler records field positions
 * @param optPluralRules the plural rules, but may be NULL if
 *   needsPluralRules returns FALSE.
 * @appendTo formatted string appended here.
 * @status any error returned here.
 */
UnicodeString &format(
        const VisibleDigitsWithExponent &value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo,
        UErrorCode &status) const;

/**
 * For testing only.
 */
UnicodeString &format(
        DigitList &value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo,
        UErrorCode &status) const;

/**
 * Formats a 32-bit integer and appends to appendTo. When formatting an
 * integer, this method is preferred to plain format as it can run
 * several times faster under certain conditions.
 *
 * @param value the value to format.
 * @param formatter handles the details of formatting the actual value.
 * @param handler records field positions
 * @param optPluralRules the plural rules, but may be NULL if
 *   needsPluralRules returns FALSE.
 * @appendTo formatted string appended here.
 * @status any error returned here.
 */
UnicodeString &formatInt32(
        int32_t value,
        const ValueFormatter &formatter,
        FieldPositionHandler &handler,
        const PluralRules *optPluralRules,
        UnicodeString &appendTo,
        UErrorCode &status) const;

private:
UnicodeString &appendPadding(int32_t paddingCount, UnicodeString &appendTo) const;

};


U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif  // __DIGITAFFIXANDPADDING_H__
