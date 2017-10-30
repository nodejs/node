// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* pluralaffix.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __PLURALAFFIX_H__
#define __PLURALAFFIX_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unum.h"
#include "unicode/uobject.h"

#include "digitaffix.h"
#include "pluralmap.h"

U_NAMESPACE_BEGIN

class FieldPositionHandler;

// Export an explicit template instantiation.
//
//    MSVC requires this, even though it should not be necessary.
//    No direct access leaks out of the i18n library.
//
//    Macintosh produces duplicate definition linker errors with the explicit template
//    instantiation.
//
#if !U_PLATFORM_IS_DARWIN_BASED
template class U_I18N_API PluralMap<DigitAffix>;
#endif


/**
 * A plural aware prefix or suffix of a formatted number.
 *
 * PluralAffix is essentially a map of DigitAffix objects keyed by plural
 * category. The 'other' category is the default and always has some
 * value. The rest of the categories are optional. Querying for a category that
 * is not set always returns the DigitAffix stored in the 'other' category.
 *
 * To use one of these objects, build it up first using append() and
 * setVariant() methods. Once built, leave unchanged and let multiple threads
 * safely access.
 *
 * The following code is sample code for building up:
 *   one: US Dollar -
 *   other: US Dollars -
 *
 * and storing it in "negativeCurrencyPrefix"
 *
 * UErrorCode status = U_ZERO_ERROR;
 *
 * PluralAffix negativeCurrencyPrefix;
 *
 * PluralAffix currencyName;
 * currencyName.setVariant("one", "US Dollar", status);
 * currencyName.setVariant("other", "US Dollars", status);
 *
 * negativeCurrencyPrefix.append(currencyName, UNUM_CURRENCY_FIELD, status);
 * negativeCurrencyPrefix.append(" ");
 * negativeCurrencyPrefix.append("-", UNUM_SIGN_FIELD, status);
 */
class U_I18N_API PluralAffix : public UMemory {
public:

    /**
     * Create empty PluralAffix.
     */
    PluralAffix() : affixes() { }

    /**
     * Create a PluralAffix where the 'other' variant is otherVariant.
     */
    PluralAffix(const DigitAffix &otherVariant) : affixes(otherVariant) { }

    /**
     * Sets a particular variant for a plural category while overwriting
     * anything that may have been previously stored for that plural
     * category. The set value has no field annotations.
     * @param category "one", "two", "few", ...
     * @param variant the variant to store under the particular category
     * @param status Any error returned here.
     */
    UBool setVariant(
            const char *category,
            const UnicodeString &variant,
            UErrorCode &status);
    /**
     * Make the 'other' variant be the empty string with no field annotations
     * and remove the variants for the rest of the plural categories.
     */
    void remove();

    /**
     * Append value to all set plural categories. If fieldId present, value
     * is that field type.
     */
    void appendUChar(UChar value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append value to all set plural categories. If fieldId present, value
     * is that field type.
     */
    void append(const UnicodeString &value, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append value to all set plural categories. If fieldId present, value
     * is that field type.
     */
    void append(const UChar *value, int32_t charCount, int32_t fieldId=UNUM_FIELD_COUNT);

    /**
     * Append the value for each plural category in rhs to the corresponding
     * plural category in this instance. Each value appended from rhs is
     * of type fieldId.
     */
    UBool append(
            const PluralAffix &rhs,
            int32_t fieldId,
            UErrorCode &status);
    /**
     * Get the DigitAffix for a paricular category such as "zero", "one", ...
     * If the particular category is not set, returns the 'other' category
     * which is always set.
     */
    const DigitAffix &getByCategory(const char *category) const;

    /**
     * Get the DigitAffix for a paricular category such as "zero", "one", ...
     * If the particular category is not set, returns the 'other' category
     * which is always set.
     */
    const DigitAffix &getByCategory(const UnicodeString &category) const;

    /**
     * Get the DigitAffix for the other category which is always set.
     */
    const DigitAffix &getOtherVariant() const {
        return affixes.getOther();
    }

    /**
     * Returns TRUE if this instance has variants stored besides the "other"
     * variant.
     */
    UBool hasMultipleVariants() const;

    /**
     * Returns TRUE if this instance equals rhs.
     */
    UBool equals(const PluralAffix &rhs) const {
        return affixes.equals(rhs.affixes, &eq);
    }

private:
    PluralMap<DigitAffix> affixes;

    static UBool eq(const DigitAffix &x, const DigitAffix &y) {
        return x.equals(y);
    }
};


U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif  // __PLURALAFFIX_H__
