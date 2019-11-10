// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include <typeinfo>
#include <array>
#include "number_types.h"
#include "number_patternstring.h"
#include "numparse_types.h"
#include "numparse_impl.h"
#include "numparse_symbols.h"
#include "numparse_decimal.h"
#include "unicode/numberformatter.h"
#include "cstr.h"
#include "number_mapper.h"
#include "static_unicode_sets.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;
using namespace icu::numparse;
using namespace icu::numparse::impl;


NumberParseMatcher::~NumberParseMatcher() = default;


NumberParserImpl*
NumberParserImpl::createSimpleParser(const Locale& locale, const UnicodeString& patternString,
                                     parse_flags_t parseFlags, UErrorCode& status) {

    LocalPointer<NumberParserImpl> parser(new NumberParserImpl(parseFlags));
    DecimalFormatSymbols symbols(locale, status);

    parser->fLocalMatchers.ignorables = {parseFlags};
    IgnorablesMatcher& ignorables = parser->fLocalMatchers.ignorables;

    DecimalFormatSymbols dfs(locale, status);
    dfs.setSymbol(DecimalFormatSymbols::kCurrencySymbol, u"IU$");
    dfs.setSymbol(DecimalFormatSymbols::kIntlCurrencySymbol, u"ICU");
    CurrencySymbols currencySymbols({u"ICU", status}, locale, dfs, status);

    ParsedPatternInfo patternInfo;
    PatternParser::parseToPatternInfo(patternString, patternInfo, status);

    // The following statements set up the affix matchers.
    AffixTokenMatcherSetupData affixSetupData = {
            currencySymbols, symbols, ignorables, locale, parseFlags};
    parser->fLocalMatchers.affixTokenMatcherWarehouse = {&affixSetupData};
    parser->fLocalMatchers.affixMatcherWarehouse = {&parser->fLocalMatchers.affixTokenMatcherWarehouse};
    parser->fLocalMatchers.affixMatcherWarehouse.createAffixMatchers(
            patternInfo, *parser, ignorables, parseFlags, status);

    Grouper grouper = Grouper::forStrategy(UNUM_GROUPING_AUTO);
    grouper.setLocaleData(patternInfo, locale);

    parser->addMatcher(parser->fLocalMatchers.ignorables);
    parser->addMatcher(parser->fLocalMatchers.decimal = {symbols, grouper, parseFlags});
    parser->addMatcher(parser->fLocalMatchers.minusSign = {symbols, false});
    parser->addMatcher(parser->fLocalMatchers.plusSign = {symbols, false});
    parser->addMatcher(parser->fLocalMatchers.percent = {symbols});
    parser->addMatcher(parser->fLocalMatchers.permille = {symbols});
    parser->addMatcher(parser->fLocalMatchers.nan = {symbols});
    parser->addMatcher(parser->fLocalMatchers.infinity = {symbols});
    parser->addMatcher(parser->fLocalMatchers.padding = {u"@"});
    parser->addMatcher(parser->fLocalMatchers.scientific = {symbols, grouper});
    parser->addMatcher(parser->fLocalMatchers.currency = {currencySymbols, symbols, parseFlags, status});
    parser->addMatcher(parser->fLocalValidators.number = {});

    parser->freeze();
    return parser.orphan();
}

NumberParserImpl*
NumberParserImpl::createParserFromProperties(const number::impl::DecimalFormatProperties& properties,
                                             const DecimalFormatSymbols& symbols, bool parseCurrency,
                                             UErrorCode& status) {
    Locale locale = symbols.getLocale();
    PropertiesAffixPatternProvider localPAPP;
    CurrencyPluralInfoAffixProvider localCPIAP;
    AffixPatternProvider* affixProvider;
    if (properties.currencyPluralInfo.fPtr.isNull()) {
        localPAPP.setTo(properties, status);
        affixProvider = &localPAPP;
    } else {
        localCPIAP.setTo(*properties.currencyPluralInfo.fPtr, properties, status);
        affixProvider = &localCPIAP;
    }
    if (affixProvider == nullptr || U_FAILURE(status)) { return nullptr; }
    CurrencyUnit currency = resolveCurrency(properties, locale, status);
    CurrencySymbols currencySymbols(currency, locale, symbols, status);
    bool isStrict = properties.parseMode.getOrDefault(PARSE_MODE_STRICT) == PARSE_MODE_STRICT;
    Grouper grouper = Grouper::forProperties(properties);
    int parseFlags = 0;
    if (affixProvider == nullptr || U_FAILURE(status)) { return nullptr; }
    if (!properties.parseCaseSensitive) {
        parseFlags |= PARSE_FLAG_IGNORE_CASE;
    }
    if (properties.parseIntegerOnly) {
        parseFlags |= PARSE_FLAG_INTEGER_ONLY;
    }
    if (properties.signAlwaysShown) {
        parseFlags |= PARSE_FLAG_PLUS_SIGN_ALLOWED;
    }
    if (isStrict) {
        parseFlags |= PARSE_FLAG_STRICT_GROUPING_SIZE;
        parseFlags |= PARSE_FLAG_STRICT_SEPARATORS;
        parseFlags |= PARSE_FLAG_USE_FULL_AFFIXES;
        parseFlags |= PARSE_FLAG_EXACT_AFFIX;
        parseFlags |= PARSE_FLAG_STRICT_IGNORABLES;
    } else {
        parseFlags |= PARSE_FLAG_INCLUDE_UNPAIRED_AFFIXES;
    }
    if (grouper.getPrimary() <= 0) {
        parseFlags |= PARSE_FLAG_GROUPING_DISABLED;
    }
    if (parseCurrency || affixProvider->hasCurrencySign()) {
        parseFlags |= PARSE_FLAG_MONETARY_SEPARATORS;
    }
    if (!parseCurrency) {
        parseFlags |= PARSE_FLAG_NO_FOREIGN_CURRENCY;
    }

    LocalPointer<NumberParserImpl> parser(new NumberParserImpl(parseFlags));

    parser->fLocalMatchers.ignorables = {parseFlags};
    IgnorablesMatcher& ignorables = parser->fLocalMatchers.ignorables;

    //////////////////////
    /// AFFIX MATCHERS ///
    //////////////////////

    // The following statements set up the affix matchers.
    AffixTokenMatcherSetupData affixSetupData = {
            currencySymbols, symbols, ignorables, locale, parseFlags};
    parser->fLocalMatchers.affixTokenMatcherWarehouse = {&affixSetupData};
    parser->fLocalMatchers.affixMatcherWarehouse = {&parser->fLocalMatchers.affixTokenMatcherWarehouse};
    parser->fLocalMatchers.affixMatcherWarehouse.createAffixMatchers(
            *affixProvider, *parser, ignorables, parseFlags, status);

    ////////////////////////
    /// CURRENCY MATCHER ///
    ////////////////////////

    if (parseCurrency || affixProvider->hasCurrencySign()) {
        parser->addMatcher(parser->fLocalMatchers.currency = {currencySymbols, symbols, parseFlags, status});
    }

    ///////////////
    /// PERCENT ///
    ///////////////

    // ICU-TC meeting, April 11, 2018: accept percent/permille only if it is in the pattern,
    // and to maintain regressive behavior, divide by 100 even if no percent sign is present.
    if (!isStrict && affixProvider->containsSymbolType(AffixPatternType::TYPE_PERCENT, status)) {
        parser->addMatcher(parser->fLocalMatchers.percent = {symbols});
    }
    if (!isStrict && affixProvider->containsSymbolType(AffixPatternType::TYPE_PERMILLE, status)) {
        parser->addMatcher(parser->fLocalMatchers.permille = {symbols});
    }

    ///////////////////////////////
    /// OTHER STANDARD MATCHERS ///
    ///////////////////////////////

    if (!isStrict) {
        parser->addMatcher(parser->fLocalMatchers.plusSign = {symbols, false});
        parser->addMatcher(parser->fLocalMatchers.minusSign = {symbols, false});
    }
    parser->addMatcher(parser->fLocalMatchers.nan = {symbols});
    parser->addMatcher(parser->fLocalMatchers.infinity = {symbols});
    UnicodeString padString = properties.padString;
    if (!padString.isBogus() && !ignorables.getSet()->contains(padString)) {
        parser->addMatcher(parser->fLocalMatchers.padding = {padString});
    }
    parser->addMatcher(parser->fLocalMatchers.ignorables);
    parser->addMatcher(parser->fLocalMatchers.decimal = {symbols, grouper, parseFlags});
    // NOTE: parseNoExponent doesn't disable scientific parsing if we have a scientific formatter
    if (!properties.parseNoExponent || properties.minimumExponentDigits > 0) {
        parser->addMatcher(parser->fLocalMatchers.scientific = {symbols, grouper});
    }

    //////////////////
    /// VALIDATORS ///
    //////////////////

    parser->addMatcher(parser->fLocalValidators.number = {});
    if (isStrict) {
        parser->addMatcher(parser->fLocalValidators.affix = {});
    }
    if (parseCurrency) {
        parser->addMatcher(parser->fLocalValidators.currency = {});
    }
    if (properties.decimalPatternMatchRequired) {
        bool patternHasDecimalSeparator =
                properties.decimalSeparatorAlwaysShown || properties.maximumFractionDigits != 0;
        parser->addMatcher(parser->fLocalValidators.decimalSeparator = {patternHasDecimalSeparator});
    }
    // The multiplier takes care of scaling percentages.
    Scale multiplier = scaleFromProperties(properties);
    if (multiplier.isValid()) {
        parser->addMatcher(parser->fLocalValidators.multiplier = {multiplier});
    }

    parser->freeze();
    return parser.orphan();
}

NumberParserImpl::NumberParserImpl(parse_flags_t parseFlags)
        : fParseFlags(parseFlags) {
}

NumberParserImpl::~NumberParserImpl() {
    fNumMatchers = 0;
}

void NumberParserImpl::addMatcher(NumberParseMatcher& matcher) {
    if (fNumMatchers + 1 > fMatchers.getCapacity()) {
        fMatchers.resize(fNumMatchers * 2, fNumMatchers);
    }
    fMatchers[fNumMatchers] = &matcher;
    fNumMatchers++;
}

void NumberParserImpl::freeze() {
    fFrozen = true;
}

parse_flags_t NumberParserImpl::getParseFlags() const {
    return fParseFlags;
}

void NumberParserImpl::parse(const UnicodeString& input, bool greedy, ParsedNumber& result,
                             UErrorCode& status) const {
    return parse(input, 0, greedy, result, status);
}

void NumberParserImpl::parse(const UnicodeString& input, int32_t start, bool greedy, ParsedNumber& result,
                             UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(fFrozen);
    // TODO: Check start >= 0 and start < input.length()
    StringSegment segment(input, 0 != (fParseFlags & PARSE_FLAG_IGNORE_CASE));
    segment.adjustOffset(start);
    if (greedy) {
        parseGreedy(segment, result, status);
    } else if (0 != (fParseFlags & PARSE_FLAG_ALLOW_INFINITE_RECURSION)) {
        // Start at 1 so that recursionLevels never gets to 0
        parseLongestRecursive(segment, result, 1, status);
    } else {
        // Arbitrary recursion safety limit: 100 levels.
        parseLongestRecursive(segment, result, -100, status);
    }
    for (int32_t i = 0; i < fNumMatchers; i++) {
        fMatchers[i]->postProcess(result);
    }
    result.postProcess();
}

void NumberParserImpl::parseGreedy(StringSegment& segment, ParsedNumber& result,
                                            UErrorCode& status) const {
    // Note: this method is not recursive in order to avoid stack overflow.
    for (int i = 0; i <fNumMatchers;) {
        // Base Case
        if (segment.length() == 0) {
            return;
        }
        const NumberParseMatcher* matcher = fMatchers[i];
        if (!matcher->smokeTest(segment)) {
            // Matcher failed smoke test: try the next one
            i++;
            continue;
        }
        int32_t initialOffset = segment.getOffset();
        matcher->match(segment, result, status);
        if (U_FAILURE(status)) {
            return;
        }
        if (segment.getOffset() != initialOffset) {
            // Greedy heuristic: accept the match and loop back
            i = 0;
            continue;
        } else {
            // Matcher did not match: try the next one
            i++;
            continue;
        }
        UPRV_UNREACHABLE;
    }

    // NOTE: If we get here, the greedy parse completed without consuming the entire string.
}

void NumberParserImpl::parseLongestRecursive(StringSegment& segment, ParsedNumber& result,
                                             int32_t recursionLevels,
                                             UErrorCode& status) const {
    // Base Case
    if (segment.length() == 0) {
        return;
    }

    // Safety against stack overflow
    if (recursionLevels == 0) {
        return;
    }

    // TODO: Give a nice way for the matcher to reset the ParsedNumber?
    ParsedNumber initial(result);
    ParsedNumber candidate;

    int initialOffset = segment.getOffset();
    for (int32_t i = 0; i < fNumMatchers; i++) {
        const NumberParseMatcher* matcher = fMatchers[i];
        if (!matcher->smokeTest(segment)) {
            continue;
        }

        // In a non-greedy parse, we attempt all possible matches and pick the best.
        for (int32_t charsToConsume = 0; charsToConsume < segment.length();) {
            charsToConsume += U16_LENGTH(segment.codePointAt(charsToConsume));

            // Run the matcher on a segment of the current length.
            candidate = initial;
            segment.setLength(charsToConsume);
            bool maybeMore = matcher->match(segment, candidate, status);
            segment.resetLength();
            if (U_FAILURE(status)) {
                return;
            }

            // If the entire segment was consumed, recurse.
            if (segment.getOffset() - initialOffset == charsToConsume) {
                parseLongestRecursive(segment, candidate, recursionLevels + 1, status);
                if (U_FAILURE(status)) {
                    return;
                }
                if (candidate.isBetterThan(result)) {
                    result = candidate;
                }
            }

            // Since the segment can be re-used, reset the offset.
            // This does not have an effect if the matcher did not consume any chars.
            segment.setOffset(initialOffset);

            // Unless the matcher wants to see the next char, continue to the next matcher.
            if (!maybeMore) {
                break;
            }
        }
    }
}

UnicodeString NumberParserImpl::toString() const {
    UnicodeString result(u"<NumberParserImpl matchers:[");
    for (int32_t i = 0; i < fNumMatchers; i++) {
        result.append(u' ');
        result.append(fMatchers[i]->toString());
    }
    result.append(u" ]>", -1);
    return result;
}


#endif /* #if !UCONFIG_NO_FORMATTING */
