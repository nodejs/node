// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "numparse_types.h"
#include "numparse_symbols.h"
#include "numparse_utils.h"
#include "string_segment.h"

using namespace icu;
using namespace icu::numparse;
using namespace icu::numparse::impl;


SymbolMatcher::SymbolMatcher(const UnicodeString& symbolString, unisets::Key key) {
    fUniSet = unisets::get(key);
    if (fUniSet->contains(symbolString)) {
        fString.setToBogus();
    } else {
        fString = symbolString;
    }
}

const UnicodeSet* SymbolMatcher::getSet() const {
    return fUniSet;
}

bool SymbolMatcher::match(StringSegment& segment, ParsedNumber& result, UErrorCode&) const {
    // Smoke test first; this matcher might be disabled.
    if (isDisabled(result)) {
        return false;
    }

    // Test the string first in order to consume trailing chars greedily.
    int overlap = 0;
    if (!fString.isEmpty()) {
        overlap = segment.getCommonPrefixLength(fString);
        if (overlap == fString.length()) {
            segment.adjustOffset(fString.length());
            accept(segment, result);
            return false;
        }
    }

    int cp = segment.getCodePoint();
    if (cp != -1 && fUniSet->contains(cp)) {
        segment.adjustOffset(U16_LENGTH(cp));
        accept(segment, result);
        return false;
    }

    return overlap == segment.length();
}

bool SymbolMatcher::smokeTest(const StringSegment& segment) const {
    return segment.startsWith(*fUniSet) || segment.startsWith(fString);
}

UnicodeString SymbolMatcher::toString() const {
    // TODO: Customize output for each symbol
    return u"<Symbol>";
}


IgnorablesMatcher::IgnorablesMatcher(parse_flags_t parseFlags) :
        SymbolMatcher(
            {},
            (0 != (parseFlags & PARSE_FLAG_STRICT_IGNORABLES)) ?
                unisets::STRICT_IGNORABLES :
                unisets::DEFAULT_IGNORABLES) {
}

bool IgnorablesMatcher::isFlexible() const {
    return true;
}

UnicodeString IgnorablesMatcher::toString() const {
    return u"<Ignorables>";
}

bool IgnorablesMatcher::isDisabled(const ParsedNumber&) const {
    return false;
}

void IgnorablesMatcher::accept(StringSegment&, ParsedNumber&) const {
    // No-op
}


InfinityMatcher::InfinityMatcher(const DecimalFormatSymbols& dfs)
        : SymbolMatcher(dfs.getConstSymbol(DecimalFormatSymbols::kInfinitySymbol), unisets::INFINITY_SIGN) {
}

bool InfinityMatcher::isDisabled(const ParsedNumber& result) const {
    return 0 != (result.flags & FLAG_INFINITY);
}

void InfinityMatcher::accept(StringSegment& segment, ParsedNumber& result) const {
    result.flags |= FLAG_INFINITY;
    result.setCharsConsumed(segment);
}


MinusSignMatcher::MinusSignMatcher(const DecimalFormatSymbols& dfs, bool allowTrailing)
        : SymbolMatcher(dfs.getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol), unisets::MINUS_SIGN),
          fAllowTrailing(allowTrailing) {
}

bool MinusSignMatcher::isDisabled(const ParsedNumber& result) const {
    return !fAllowTrailing && result.seenNumber();
}

void MinusSignMatcher::accept(StringSegment& segment, ParsedNumber& result) const {
    result.flags |= FLAG_NEGATIVE;
    result.setCharsConsumed(segment);
}


NanMatcher::NanMatcher(const DecimalFormatSymbols& dfs)
        : SymbolMatcher(dfs.getConstSymbol(DecimalFormatSymbols::kNaNSymbol), unisets::EMPTY) {
}

bool NanMatcher::isDisabled(const ParsedNumber& result) const {
    return result.seenNumber();
}

void NanMatcher::accept(StringSegment& segment, ParsedNumber& result) const {
    result.flags |= FLAG_NAN;
    result.setCharsConsumed(segment);
}


PaddingMatcher::PaddingMatcher(const UnicodeString& padString)
        : SymbolMatcher(padString, unisets::EMPTY) {}

bool PaddingMatcher::isFlexible() const {
    return true;
}

bool PaddingMatcher::isDisabled(const ParsedNumber&) const {
    return false;
}

void PaddingMatcher::accept(StringSegment&, ParsedNumber&) const {
    // No-op
}


PercentMatcher::PercentMatcher(const DecimalFormatSymbols& dfs)
        : SymbolMatcher(dfs.getConstSymbol(DecimalFormatSymbols::kPercentSymbol), unisets::PERCENT_SIGN) {
}

bool PercentMatcher::isDisabled(const ParsedNumber& result) const {
    return 0 != (result.flags & FLAG_PERCENT);
}

void PercentMatcher::accept(StringSegment& segment, ParsedNumber& result) const {
    result.flags |= FLAG_PERCENT;
    result.setCharsConsumed(segment);
}


PermilleMatcher::PermilleMatcher(const DecimalFormatSymbols& dfs)
        : SymbolMatcher(dfs.getConstSymbol(DecimalFormatSymbols::kPerMillSymbol), unisets::PERMILLE_SIGN) {
}

bool PermilleMatcher::isDisabled(const ParsedNumber& result) const {
    return 0 != (result.flags & FLAG_PERMILLE);
}

void PermilleMatcher::accept(StringSegment& segment, ParsedNumber& result) const {
    result.flags |= FLAG_PERMILLE;
    result.setCharsConsumed(segment);
}


PlusSignMatcher::PlusSignMatcher(const DecimalFormatSymbols& dfs, bool allowTrailing)
        : SymbolMatcher(dfs.getConstSymbol(DecimalFormatSymbols::kPlusSignSymbol), unisets::PLUS_SIGN),
          fAllowTrailing(allowTrailing) {
}

bool PlusSignMatcher::isDisabled(const ParsedNumber& result) const {
    return !fAllowTrailing && result.seenNumber();
}

void PlusSignMatcher::accept(StringSegment& segment, ParsedNumber& result) const {
    result.setCharsConsumed(segment);
}


#endif /* #if !UCONFIG_NO_FORMATTING */
