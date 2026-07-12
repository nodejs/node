// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "numparse_types.h"
#include "numparse_scientific.h"
#include "static_unicode_sets.h"
#include "string_segment.h"

using namespace icu;
using namespace icu::numparse;
using namespace icu::numparse::impl;


namespace {

inline const UnicodeSet& minusSignSet() {
    return *unisets::get(unisets::MINUS_SIGN);
}

inline const UnicodeSet& plusSignSet() {
    return *unisets::get(unisets::PLUS_SIGN);
}

} // namespace


ScientificMatcher::ScientificMatcher(const DecimalFormatSymbols& dfs, const Grouper& grouper)
        : fExponentSeparatorString(dfs.getConstSymbol(DecimalFormatSymbols::kExponentialSymbol)),
          fExponentMatcher(dfs, grouper, PARSE_FLAG_INTEGER_ONLY | PARSE_FLAG_GROUPING_DISABLED),
          fIgnorablesMatcher(PARSE_FLAG_STRICT_IGNORABLES) {

    const UnicodeString& minusSign = dfs.getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol);
    if (minusSignSet().contains(minusSign)) {
        fCustomMinusSign.setToBogus();
    } else {
        fCustomMinusSign = minusSign;
    }

    const UnicodeString& plusSign = dfs.getConstSymbol(DecimalFormatSymbols::kPlusSignSymbol);
    if (plusSignSet().contains(plusSign)) {
        fCustomPlusSign.setToBogus();
    } else {
        fCustomPlusSign = plusSign;
    }
}

bool ScientificMatcher::match(StringSegment& segment, ParsedNumber& result, UErrorCode& status) const {
    // Only accept scientific notation after the mantissa.
    if (!result.seenNumber()) {
        return false;
    }

    // Only accept one exponent per string.
    if (0 != (result.flags & FLAG_HAS_EXPONENT)) {
        return false;
    }

    // First match the scientific separator, and then match another number after it.
    // NOTE: This is guarded by the smoke test; no need to check fExponentSeparatorString length again.
    int32_t initialOffset = segment.getOffset();
    int32_t overlap = segment.getCommonPrefixLength(fExponentSeparatorString);
    if (overlap == fExponentSeparatorString.length()) {
        // Full exponent separator match.

        // First attempt to get a code point, returning true if we can't get one.
        if (segment.length() == overlap) {
            return true;
        }
        segment.adjustOffset(overlap);

        // Allow ignorables before the sign.
        // Note: call site is guarded by the segment.length() check above.
        // Note: the ignorables matcher should not touch the result.
        fIgnorablesMatcher.match(segment, result, status);
        if (segment.length() == 0) {
            segment.setOffset(initialOffset);
            return true;
        }

        // Allow a sign, and then try to match digits.
        int8_t exponentSign = 1;
        if (segment.startsWith(minusSignSet())) {
            exponentSign = -1;
            segment.adjustOffsetByCodePoint();
        } else if (segment.startsWith(plusSignSet())) {
            segment.adjustOffsetByCodePoint();
        } else if (segment.startsWith(fCustomMinusSign)) {
            overlap = segment.getCommonPrefixLength(fCustomMinusSign);
            if (overlap != fCustomMinusSign.length()) {
                // Partial custom sign match
                segment.setOffset(initialOffset);
                return true;
            }
            exponentSign = -1;
            segment.adjustOffset(overlap);
        } else if (segment.startsWith(fCustomPlusSign)) {
            overlap = segment.getCommonPrefixLength(fCustomPlusSign);
            if (overlap != fCustomPlusSign.length()) {
                // Partial custom sign match
                segment.setOffset(initialOffset);
                return true;
            }
            segment.adjustOffset(overlap);
        }

        // Return true if the segment is empty.
        if (segment.length() == 0) {
            segment.setOffset(initialOffset);
            return true;
        }

        // Allow ignorables after the sign.
        // Note: call site is guarded by the segment.length() check above.
        // Note: the ignorables matcher should not touch the result.
        fIgnorablesMatcher.match(segment, result, status);
        if (segment.length() == 0) {
            segment.setOffset(initialOffset);
            return true;
        }

        // We are supposed to accept E0 after NaN, so we need to make sure result.quantity is available.
        bool wasBogus = result.quantity.bogus;
        result.quantity.bogus = false;
        int digitsOffset = segment.getOffset();
        bool digitsReturnValue = fExponentMatcher.match(segment, result, exponentSign, status);
        result.quantity.bogus = wasBogus;

        if (segment.getOffset() != digitsOffset) {
            // At least one exponent digit was matched.
            result.flags |= FLAG_HAS_EXPONENT;
        } else {
            // No exponent digits were matched
            segment.setOffset(initialOffset);
        }
        return digitsReturnValue;

    } else if (overlap == segment.length()) {
        // Partial exponent separator match
        return true;
    }

    // No match
    return false;
}

bool ScientificMatcher::smokeTest(const StringSegment& segment) const {
    return segment.startsWith(fExponentSeparatorString);
}

UnicodeString ScientificMatcher::toString() const {
    return u"<Scientific>";
}


#endif /* #if !UCONFIG_NO_FORMATTING */
