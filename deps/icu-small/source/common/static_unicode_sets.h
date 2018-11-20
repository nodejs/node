// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// This file is in common instead of i18n because it is needed by ucurr.cpp.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __STATIC_UNICODE_SETS_H__
#define __STATIC_UNICODE_SETS_H__

#include "unicode/uniset.h"
#include "unicode/unistr.h"

U_NAMESPACE_BEGIN
namespace unisets {

enum Key {
    // NONE is used to indicate null in chooseFrom().
    // EMPTY is used to get an empty UnicodeSet.
    NONE = -1,
    EMPTY = 0,

    // Ignorables
    DEFAULT_IGNORABLES,
    STRICT_IGNORABLES,

    // Separators
    // Notes:
    // - COMMA is a superset of STRICT_COMMA
    // - PERIOD is a superset of SCRICT_PERIOD
    // - ALL_SEPARATORS is the union of COMMA, PERIOD, and OTHER_GROUPING_SEPARATORS
    // - STRICT_ALL_SEPARATORS is the union of STRICT_COMMA, STRICT_PERIOD, and OTHER_GRP_SEPARATORS
    COMMA,
    PERIOD,
    STRICT_COMMA,
    STRICT_PERIOD,
    OTHER_GROUPING_SEPARATORS,
    ALL_SEPARATORS,
    STRICT_ALL_SEPARATORS,

    // Symbols
    MINUS_SIGN,
    PLUS_SIGN,
    PERCENT_SIGN,
    PERMILLE_SIGN,
    INFINITY_KEY, // INFINITY is defined in cmath

    // Currency Symbols
    DOLLAR_SIGN,
    POUND_SIGN,
    RUPEE_SIGN,
    YEN_SIGN, // not in CLDR data, but Currency.java wants it

    // Other
    DIGITS,

    // Combined Separators with Digits (for lead code points)
    DIGITS_OR_ALL_SEPARATORS,
    DIGITS_OR_STRICT_ALL_SEPARATORS,

    // The number of elements in the enum.
    COUNT
};

/**
 * Gets the static-allocated UnicodeSet according to the provided key. The
 * pointer will be deleted during u_cleanup(); the caller should NOT delete it.
 *
 * Exported as U_COMMON_API for ucurr.cpp
 *
 * @param key The desired UnicodeSet according to the enum in this file.
 * @return The requested UnicodeSet. Guaranteed to be frozen and non-null, but
 *         may be empty if an error occurred during data loading.
 */
U_COMMON_API const UnicodeSet* get(Key key);

/**
 * Checks if the UnicodeSet given by key1 contains the given string.
 *
 * Exported as U_COMMON_API for numparse_decimal.cpp
 *
 * @param str The string to check.
 * @param key1 The set to check.
 * @return key1 if the set contains str, or NONE if not.
 */
U_COMMON_API Key chooseFrom(UnicodeString str, Key key1);

/**
 * Checks if the UnicodeSet given by either key1 or key2 contains the string.
 *
 * Exported as U_COMMON_API for numparse_decimal.cpp
 *
 * @param str The string to check.
 * @param key1 The first set to check.
 * @param key2 The second set to check.
 * @return key1 if that set contains str; key2 if that set contains str; or
 *         NONE if neither set contains str.
 */
U_COMMON_API Key chooseFrom(UnicodeString str, Key key1, Key key2);

// Unused in C++:
// Key chooseCurrency(UnicodeString str);
// Used instead:
static const struct {
    Key key;
    UChar32 exemplar;
} kCurrencyEntries[] = {
    {DOLLAR_SIGN, u'$'},
    {POUND_SIGN, u'£'},
    {RUPEE_SIGN, u'₨'},
    {YEN_SIGN, u'¥'},
};

} // namespace unisets
U_NAMESPACE_END

#endif //__STATIC_UNICODE_SETS_H__
#endif /* #if !UCONFIG_NO_FORMATTING */
