// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2005-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINNMFMT.H
*
********************************************************************************
*/

#ifndef __WINNMFMT
#define __WINNMFMT

#include "unicode/utypes.h"

#if U_PLATFORM_USES_ONLY_WIN32_API

#include "unicode/format.h"
#include "unicode/datefmt.h"
#include "unicode/calendar.h"
#include "unicode/ustring.h"
#include "unicode/locid.h"

#if !UCONFIG_NO_FORMATTING

/**
 * \file 
 * \brief C++ API: Format numbers using Windows API.
 */

U_NAMESPACE_BEGIN

union FormatInfo;

class Win32NumberFormat : public NumberFormat
{
public:
    Win32NumberFormat(const Locale &locale, UBool currency, UErrorCode &status);

    Win32NumberFormat(const Win32NumberFormat &other);

    virtual ~Win32NumberFormat();

    virtual Win32NumberFormat *clone() const;

    Win32NumberFormat &operator=(const Win32NumberFormat &other);

    /**
     * Format a double number. Concrete subclasses must implement
     * these pure virtual methods.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
     */
    virtual UnicodeString& format(double number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;
    /**
     * Format a long number. Concrete subclasses must implement
     * these pure virtual methods.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
    */
    virtual UnicodeString& format(int32_t number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;

    /**
     * Format an int64 number.
     *
     * @param number    The value to be formatted.
     * @param appendTo  Output parameter to receive result.
     *                  Result is appended to existing contents.
     * @param pos       On input: an alignment field, if desired.
     *                  On output: the offsets of the alignment field.
     * @return          Reference to 'appendTo' parameter.
    */
    virtual UnicodeString& format(int64_t number,
                                  UnicodeString& appendTo,
                                  FieldPosition& pos) const;

    using NumberFormat::format;

// Use the default behavior for the following.
//    virtual UnicodeString &format(double number, UnicodeString &appendTo) const;
//    virtual UnicodeString &format(int32_t number, UnicodeString &appendTo) const;
//    virtual UnicodeString &format(int64_t number, UnicodeString &appendTo) const;

    virtual void parse(const UnicodeString& text, Formattable& result, ParsePosition& parsePosition) const;

    /**
     * Sets the maximum number of digits allowed in the fraction portion of a
     * number. maximumFractionDigits must be >= minimumFractionDigits.  If the
     * new value for maximumFractionDigits is less than the current value
     * of minimumFractionDigits, then minimumFractionDigits will also be set to
     * the new value.
     * @param newValue    the new value to be set.
     * @see getMaximumFractionDigits
     */
    virtual void setMaximumFractionDigits(int32_t newValue);

    /**
     * Sets the minimum number of digits allowed in the fraction portion of a
     * number. minimumFractionDigits must be &lt;= maximumFractionDigits.   If the
     * new value for minimumFractionDigits exceeds the current value
     * of maximumFractionDigits, then maximumIntegerDigits will also be set to
     * the new value
     * @param newValue    the new value to be set.
     * @see getMinimumFractionDigits
     */
    virtual void setMinimumFractionDigits(int32_t newValue);

    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       derived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     */
    U_I18N_API static UClassID U_EXPORT2 getStaticClassID();

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     */
    virtual UClassID getDynamicClassID() const;

private:
    UnicodeString &format(int32_t numDigits, UnicodeString &appendTo, const wchar_t *format, ...) const;

    UBool fCurrency;
    Locale fLocale;
    int32_t fLCID;
    FormatInfo *fFormatInfo;
    UBool fFractionDigitsSet;

    UnicodeString* fWindowsLocaleName; // Stores the equivalent Windows locale name.
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // U_PLATFORM_USES_ONLY_WIN32_API

#endif // __WINNMFMT
