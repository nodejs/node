// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "numparse_types.h"
#include "numparse_validators.h"
#include "static_unicode_sets.h"

using namespace icu;
using namespace icu::numparse;
using namespace icu::numparse::impl;


void RequireAffixValidator::postProcess(ParsedNumber& result) const {
    if (result.prefix.isBogus() || result.suffix.isBogus()) {
        // We saw a prefix or a suffix but not both. Fail the parse.
        result.flags |= FLAG_FAIL;
    }
}

UnicodeString RequireAffixValidator::toString() const {
    return u"<ReqAffix>";
}


void RequireCurrencyValidator::postProcess(ParsedNumber& result) const {
    if (result.currencyCode[0] == 0) {
        result.flags |= FLAG_FAIL;
    }
}

UnicodeString RequireCurrencyValidator::toString() const {
    return u"<ReqCurrency>";
}


RequireDecimalSeparatorValidator::RequireDecimalSeparatorValidator(bool patternHasDecimalSeparator)
        : fPatternHasDecimalSeparator(patternHasDecimalSeparator) {
}

void RequireDecimalSeparatorValidator::postProcess(ParsedNumber& result) const {
    bool parseHasDecimalSeparator = 0 != (result.flags & FLAG_HAS_DECIMAL_SEPARATOR);
    if (parseHasDecimalSeparator != fPatternHasDecimalSeparator) {
        result.flags |= FLAG_FAIL;
    }
}

UnicodeString RequireDecimalSeparatorValidator::toString() const {
    return u"<ReqDecimal>";
}


void RequireNumberValidator::postProcess(ParsedNumber& result) const {
    // Require that a number is matched.
    if (!result.seenNumber()) {
        result.flags |= FLAG_FAIL;
    }
}

UnicodeString RequireNumberValidator::toString() const {
    return u"<ReqNumber>";
}

MultiplierParseHandler::MultiplierParseHandler(::icu::number::Scale multiplier)
        : fMultiplier(std::move(multiplier)) {}

void MultiplierParseHandler::postProcess(ParsedNumber& result) const {
    if (!result.quantity.bogus) {
        fMultiplier.applyReciprocalTo(result.quantity);
        // NOTE: It is okay if the multiplier was negative.
    }
}

UnicodeString MultiplierParseHandler::toString() const {
    return u"<Scale>";
}

#endif /* #if !UCONFIG_NO_FORMATTING */
