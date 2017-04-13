// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2014-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* quantityformatter.h
*/

#ifndef __QUANTITY_FORMATTER_H__
#define __QUANTITY_FORMATTER_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

#if !UCONFIG_NO_FORMATTING

#include "standardplural.h"

U_NAMESPACE_BEGIN

class SimpleFormatter;
class UnicodeString;
class PluralRules;
class NumberFormat;
class Formattable;
class FieldPosition;

/**
 * A plural aware formatter that is good for expressing a single quantity and
 * a unit.
 * <p>
 * First use the add() methods to add a pattern for each plural variant.
 * There must be a pattern for the "other" variant.
 * Then use the format() method.
 * <p>
 * Concurrent calls only to const methods on a QuantityFormatter object are
 * safe, but concurrent const and non-const method calls on a QuantityFormatter
 * object are not safe and require synchronization.
 *
 */
class U_I18N_API QuantityFormatter : public UMemory {
public:
    /**
     * Default constructor.
     */
    QuantityFormatter();

    /**
     * Copy constructor.
     */
    QuantityFormatter(const QuantityFormatter& other);

    /**
     * Assignment operator
     */
    QuantityFormatter &operator=(const QuantityFormatter& other);

    /**
     * Destructor.
     */
    ~QuantityFormatter();

    /**
     * Removes all variants from this object including the "other" variant.
     */
    void reset();

    /**
     * Adds a plural variant if there is none yet for the plural form.
     *
     * @param variant "zero", "one", "two", "few", "many", "other"
     * @param rawPattern the pattern for the variant e.g "{0} meters"
     * @param status any error returned here.
     * @return TRUE on success; FALSE if status was set to a non zero error.
     */
    UBool addIfAbsent(const char *variant, const UnicodeString &rawPattern, UErrorCode &status);

    /**
     * returns TRUE if this object has at least the "other" variant.
     */
    UBool isValid() const;

    /**
     * Gets the pattern formatter that would be used for a particular variant.
     * If isValid() returns TRUE, this method is guaranteed to return a
     * non-NULL value.
     */
    const SimpleFormatter *getByVariant(const char *variant) const;

    /**
     * Formats a number with this object appending the result to appendTo.
     * At least the "other" variant must be added to this object for this
     * method to work.
     *
     * @param number the single number.
     * @param fmt formats the number
     * @param rules computes the plural variant to use.
     * @param appendTo result appended here.
     * @param status any error returned here.
     * @return appendTo
     */
    UnicodeString &format(
            const Formattable &number,
            const NumberFormat &fmt,
            const PluralRules &rules,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;

    /**
     * Selects the standard plural form for the number/formatter/rules.
     */
    static StandardPlural::Form selectPlural(
            const Formattable &number,
            const NumberFormat &fmt,
            const PluralRules &rules,
            UnicodeString &formattedNumber,
            FieldPosition &pos,
            UErrorCode &status);

    /**
     * Formats the pattern with the value and adjusts the FieldPosition.
     */
    static UnicodeString &format(
            const SimpleFormatter &pattern,
            const UnicodeString &value,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status);

private:
    SimpleFormatter *formatters[StandardPlural::COUNT];
};

U_NAMESPACE_END

#endif

#endif
