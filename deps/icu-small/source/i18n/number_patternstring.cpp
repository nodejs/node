// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "uassert.h"
#include "number_patternstring.h"
#include "unicode/utf16.h"
#include "number_utils.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

void PatternParser::parseToPatternInfo(const UnicodeString& patternString, ParsedPatternInfo& patternInfo, UErrorCode &status) {
    patternInfo.consumePattern(patternString, status);
}

DecimalFormatProperties
PatternParser::parseToProperties(const UnicodeString& pattern, IgnoreRounding ignoreRounding,
                                 UErrorCode &status) {
    DecimalFormatProperties properties;
    parseToExistingPropertiesImpl(pattern, properties, ignoreRounding, status);
    return properties;
}

void PatternParser::parseToExistingProperties(const UnicodeString& pattern, DecimalFormatProperties& properties,
                                              IgnoreRounding ignoreRounding, UErrorCode &status) {
    parseToExistingPropertiesImpl(pattern, properties, ignoreRounding, status);
}

char16_t ParsedPatternInfo::charAt(int32_t flags, int32_t index) const {
    const Endpoints &endpoints = getEndpoints(flags);
    if (index < 0 || index >= endpoints.end - endpoints.start) {
        U_ASSERT(false);
    }
    return pattern.charAt(endpoints.start + index);
}

int32_t ParsedPatternInfo::length(int32_t flags) const {
    return getLengthFromEndpoints(getEndpoints(flags));
}

int32_t ParsedPatternInfo::getLengthFromEndpoints(const Endpoints &endpoints) {
    return endpoints.end - endpoints.start;
}

UnicodeString ParsedPatternInfo::getString(int32_t flags) const {
    const Endpoints &endpoints = getEndpoints(flags);
    if (endpoints.start == endpoints.end) {
        return UnicodeString();
    }
    // Create a new UnicodeString
    return UnicodeString(pattern, endpoints.start, endpoints.end - endpoints.start);
}

const Endpoints &ParsedPatternInfo::getEndpoints(int32_t flags) const {
    bool prefix = (flags & AFFIX_PREFIX) != 0;
    bool isNegative = (flags & AFFIX_NEGATIVE_SUBPATTERN) != 0;
    bool padding = (flags & AFFIX_PADDING) != 0;
    if (isNegative && padding) {
        return negative.paddingEndpoints;
    } else if (padding) {
        return positive.paddingEndpoints;
    } else if (prefix && isNegative) {
        return negative.prefixEndpoints;
    } else if (prefix) {
        return positive.prefixEndpoints;
    } else if (isNegative) {
        return negative.suffixEndpoints;
    } else {
        return positive.suffixEndpoints;
    }
}

bool ParsedPatternInfo::positiveHasPlusSign() const {
    return positive.hasPlusSign;
}

bool ParsedPatternInfo::hasNegativeSubpattern() const {
    return fHasNegativeSubpattern;
}

bool ParsedPatternInfo::negativeHasMinusSign() const {
    return negative.hasMinusSign;
}

bool ParsedPatternInfo::hasCurrencySign() const {
    return positive.hasCurrencySign || (fHasNegativeSubpattern && negative.hasCurrencySign);
}

bool ParsedPatternInfo::containsSymbolType(AffixPatternType type, UErrorCode &status) const {
    return AffixUtils::containsType(UnicodeStringCharSequence(pattern), type, status);
}

/////////////////////////////////////////////////////
/// BEGIN RECURSIVE DESCENT PARSER IMPLEMENTATION ///
/////////////////////////////////////////////////////

UChar32 ParsedPatternInfo::ParserState::peek() {
    if (offset == pattern.length()) {
        return -1;
    } else {
        return pattern.char32At(offset);
    }
}

UChar32 ParsedPatternInfo::ParserState::next() {
    int codePoint = peek();
    offset += U16_LENGTH(codePoint);
    return codePoint;
}

void ParsedPatternInfo::consumePattern(const UnicodeString& patternString, UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    this->pattern = patternString;

    // pattern := subpattern (';' subpattern)?
    currentSubpattern = &positive;
    consumeSubpattern(status);
    if (U_FAILURE(status)) { return; }
    if (state.peek() == u';') {
        state.next(); // consume the ';'
        // Don't consume the negative subpattern if it is empty (trailing ';')
        if (state.peek() != -1) {
            fHasNegativeSubpattern = true;
            currentSubpattern = &negative;
            consumeSubpattern(status);
            if (U_FAILURE(status)) { return; }
        }
    }
    if (state.peek() != -1) {
        state.toParseException(u"Found unquoted special character");
        status = U_UNQUOTED_SPECIAL;
    }
}

void ParsedPatternInfo::consumeSubpattern(UErrorCode &status) {
    // subpattern := literals? number exponent? literals?
    consumePadding(PadPosition::UNUM_PAD_BEFORE_PREFIX, status);
    if (U_FAILURE(status)) { return; }
    consumeAffix(currentSubpattern->prefixEndpoints, status);
    if (U_FAILURE(status)) { return; }
    consumePadding(PadPosition::UNUM_PAD_AFTER_PREFIX, status);
    if (U_FAILURE(status)) { return; }
    consumeFormat(status);
    if (U_FAILURE(status)) { return; }
    consumeExponent(status);
    if (U_FAILURE(status)) { return; }
    consumePadding(PadPosition::UNUM_PAD_BEFORE_SUFFIX, status);
    if (U_FAILURE(status)) { return; }
    consumeAffix(currentSubpattern->suffixEndpoints, status);
    if (U_FAILURE(status)) { return; }
    consumePadding(PadPosition::UNUM_PAD_AFTER_SUFFIX, status);
    if (U_FAILURE(status)) { return; }
}

void ParsedPatternInfo::consumePadding(PadPosition paddingLocation, UErrorCode &status) {
    if (state.peek() != u'*') {
        return;
    }
    if (!currentSubpattern->paddingLocation.isNull()) {
        state.toParseException(u"Cannot have multiple pad specifiers");
        status = U_MULTIPLE_PAD_SPECIFIERS;
        return;
    }
    currentSubpattern->paddingLocation = paddingLocation;
    state.next(); // consume the '*'
    currentSubpattern->paddingEndpoints.start = state.offset;
    consumeLiteral(status);
    currentSubpattern->paddingEndpoints.end = state.offset;
}

void ParsedPatternInfo::consumeAffix(Endpoints &endpoints, UErrorCode &status) {
    // literals := { literal }
    endpoints.start = state.offset;
    while (true) {
        switch (state.peek()) {
            case u'#':
            case u'@':
            case u';':
            case u'*':
            case u'.':
            case u',':
            case u'0':
            case u'1':
            case u'2':
            case u'3':
            case u'4':
            case u'5':
            case u'6':
            case u'7':
            case u'8':
            case u'9':
            case -1:
                // Characters that cannot appear unquoted in a literal
                // break outer;
                goto after_outer;

            case u'%':
                currentSubpattern->hasPercentSign = true;
                break;

            case u'‰':
                currentSubpattern->hasPerMilleSign = true;
                break;

            case u'¤':
                currentSubpattern->hasCurrencySign = true;
                break;

            case u'-':
                currentSubpattern->hasMinusSign = true;
                break;

            case u'+':
                currentSubpattern->hasPlusSign = true;
                break;

            default:
                break;
        }
        consumeLiteral(status);
        if (U_FAILURE(status)) { return; }
    }
    after_outer:
    endpoints.end = state.offset;
}

void ParsedPatternInfo::consumeLiteral(UErrorCode &status) {
    if (state.peek() == -1) {
        state.toParseException(u"Expected unquoted literal but found EOL");
        status = U_PATTERN_SYNTAX_ERROR;
        return;
    } else if (state.peek() == u'\'') {
        state.next(); // consume the starting quote
        while (state.peek() != u'\'') {
            if (state.peek() == -1) {
                state.toParseException(u"Expected quoted literal but found EOL");
                status = U_PATTERN_SYNTAX_ERROR;
                return;
            } else {
                state.next(); // consume a quoted character
            }
        }
        state.next(); // consume the ending quote
    } else {
        // consume a non-quoted literal character
        state.next();
    }
}

void ParsedPatternInfo::consumeFormat(UErrorCode &status) {
    consumeIntegerFormat(status);
    if (U_FAILURE(status)) { return; }
    if (state.peek() == u'.') {
        state.next(); // consume the decimal point
        currentSubpattern->hasDecimal = true;
        currentSubpattern->widthExceptAffixes += 1;
        consumeFractionFormat(status);
        if (U_FAILURE(status)) { return; }
    }
}

void ParsedPatternInfo::consumeIntegerFormat(UErrorCode &status) {
    // Convenience reference:
    ParsedSubpatternInfo &result = *currentSubpattern;

    while (true) {
        switch (state.peek()) {
            case u',':
                result.widthExceptAffixes += 1;
                result.groupingSizes <<= 16;
                break;

            case u'#':
                if (result.integerNumerals > 0) {
                    state.toParseException(u"# cannot follow 0 before decimal point");
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                result.widthExceptAffixes += 1;
                result.groupingSizes += 1;
                if (result.integerAtSigns > 0) {
                    result.integerTrailingHashSigns += 1;
                } else {
                    result.integerLeadingHashSigns += 1;
                }
                result.integerTotal += 1;
                break;

            case u'@':
                if (result.integerNumerals > 0) {
                    state.toParseException(u"Cannot mix 0 and @");
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                if (result.integerTrailingHashSigns > 0) {
                    state.toParseException(u"Cannot nest # inside of a run of @");
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                result.widthExceptAffixes += 1;
                result.groupingSizes += 1;
                result.integerAtSigns += 1;
                result.integerTotal += 1;
                break;

            case u'0':
            case u'1':
            case u'2':
            case u'3':
            case u'4':
            case u'5':
            case u'6':
            case u'7':
            case u'8':
            case u'9':
                if (result.integerAtSigns > 0) {
                    state.toParseException(u"Cannot mix @ and 0");
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                result.widthExceptAffixes += 1;
                result.groupingSizes += 1;
                result.integerNumerals += 1;
                result.integerTotal += 1;
                if (!result.rounding.isZero() || state.peek() != u'0') {
                    result.rounding.appendDigit(static_cast<int8_t>(state.peek() - u'0'), 0, true);
                }
                break;

            default:
                goto after_outer;
        }
        state.next(); // consume the symbol
    }

    after_outer:
    // Disallow patterns with a trailing ',' or with two ',' next to each other
    auto grouping1 = static_cast<int16_t> (result.groupingSizes & 0xffff);
    auto grouping2 = static_cast<int16_t> ((result.groupingSizes >> 16) & 0xffff);
    auto grouping3 = static_cast<int16_t> ((result.groupingSizes >> 32) & 0xffff);
    if (grouping1 == 0 && grouping2 != -1) {
        state.toParseException(u"Trailing grouping separator is invalid");
        status = U_UNEXPECTED_TOKEN;
        return;
    }
    if (grouping2 == 0 && grouping3 != -1) {
        state.toParseException(u"Grouping width of zero is invalid");
        status = U_PATTERN_SYNTAX_ERROR;
        return;
    }
}

void ParsedPatternInfo::consumeFractionFormat(UErrorCode &status) {
    // Convenience reference:
    ParsedSubpatternInfo &result = *currentSubpattern;

    int32_t zeroCounter = 0;
    while (true) {
        switch (state.peek()) {
            case u'#':
                result.widthExceptAffixes += 1;
                result.fractionHashSigns += 1;
                result.fractionTotal += 1;
                zeroCounter++;
                break;

            case u'0':
            case u'1':
            case u'2':
            case u'3':
            case u'4':
            case u'5':
            case u'6':
            case u'7':
            case u'8':
            case u'9':
                if (result.fractionHashSigns > 0) {
                    state.toParseException(u"0 cannot follow # after decimal point");
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                result.widthExceptAffixes += 1;
                result.fractionNumerals += 1;
                result.fractionTotal += 1;
                if (state.peek() == u'0') {
                    zeroCounter++;
                } else {
                    result.rounding
                            .appendDigit(static_cast<int8_t>(state.peek() - u'0'), zeroCounter, false);
                    zeroCounter = 0;
                }
                break;

            default:
                return;
        }
        state.next(); // consume the symbol
    }
}

void ParsedPatternInfo::consumeExponent(UErrorCode &status) {
    // Convenience reference:
    ParsedSubpatternInfo &result = *currentSubpattern;

    if (state.peek() != u'E') {
        return;
    }
    if ((result.groupingSizes & 0xffff0000L) != 0xffff0000L) {
        state.toParseException(u"Cannot have grouping separator in scientific notation");
        status = U_MALFORMED_EXPONENTIAL_PATTERN;
        return;
    }
    state.next(); // consume the E
    result.widthExceptAffixes++;
    if (state.peek() == u'+') {
        state.next(); // consume the +
        result.exponentHasPlusSign = true;
        result.widthExceptAffixes++;
    }
    while (state.peek() == u'0') {
        state.next(); // consume the 0
        result.exponentZeros += 1;
        result.widthExceptAffixes++;
    }
}

///////////////////////////////////////////////////
/// END RECURSIVE DESCENT PARSER IMPLEMENTATION ///
///////////////////////////////////////////////////

void
PatternParser::parseToExistingPropertiesImpl(const UnicodeString& pattern, DecimalFormatProperties &properties,
                                             IgnoreRounding ignoreRounding, UErrorCode &status) {
    if (pattern.length() == 0) {
        // Backwards compatibility requires that we reset to the default values.
        // TODO: Only overwrite the properties that "saveToProperties" normally touches?
        properties.clear();
        return;
    }

    ParsedPatternInfo patternInfo;
    parseToPatternInfo(pattern, patternInfo, status);
    if (U_FAILURE(status)) { return; }
    patternInfoToProperties(properties, patternInfo, ignoreRounding, status);
}

void PatternParser::patternInfoToProperties(DecimalFormatProperties &properties,
                                            ParsedPatternInfo& patternInfo,
                                            IgnoreRounding _ignoreRounding, UErrorCode &status) {
    // Translate from PatternParseResult to Properties.
    // Note that most data from "negative" is ignored per the specification of DecimalFormat.

    const ParsedSubpatternInfo &positive = patternInfo.positive;

    bool ignoreRounding;
    if (_ignoreRounding == IGNORE_ROUNDING_NEVER) {
        ignoreRounding = false;
    } else if (_ignoreRounding == IGNORE_ROUNDING_IF_CURRENCY) {
        ignoreRounding = positive.hasCurrencySign;
    } else {
        U_ASSERT(_ignoreRounding == IGNORE_ROUNDING_ALWAYS);
        ignoreRounding = true;
    }

    // Grouping settings
    auto grouping1 = static_cast<int16_t> (positive.groupingSizes & 0xffff);
    auto grouping2 = static_cast<int16_t> ((positive.groupingSizes >> 16) & 0xffff);
    auto grouping3 = static_cast<int16_t> ((positive.groupingSizes >> 32) & 0xffff);
    if (grouping2 != -1) {
        properties.groupingSize = grouping1;
    } else {
        properties.groupingSize = -1;
    }
    if (grouping3 != -1) {
        properties.secondaryGroupingSize = grouping2;
    } else {
        properties.secondaryGroupingSize = -1;
    }

    // For backwards compatibility, require that the pattern emit at least one min digit.
    int minInt, minFrac;
    if (positive.integerTotal == 0 && positive.fractionTotal > 0) {
        // patterns like ".##"
        minInt = 0;
        minFrac = uprv_max(1, positive.fractionNumerals);
    } else if (positive.integerNumerals == 0 && positive.fractionNumerals == 0) {
        // patterns like "#.##"
        minInt = 1;
        minFrac = 0;
    } else {
        minInt = positive.integerNumerals;
        minFrac = positive.fractionNumerals;
    }

    // Rounding settings
    // Don't set basic rounding when there is a currency sign; defer to CurrencyUsage
    if (positive.integerAtSigns > 0) {
        properties.minimumFractionDigits = -1;
        properties.maximumFractionDigits = -1;
        properties.roundingIncrement = 0.0;
        properties.minimumSignificantDigits = positive.integerAtSigns;
        properties.maximumSignificantDigits =
                positive.integerAtSigns + positive.integerTrailingHashSigns;
    } else if (!positive.rounding.isZero()) {
        if (!ignoreRounding) {
            properties.minimumFractionDigits = minFrac;
            properties.maximumFractionDigits = positive.fractionTotal;
            properties.roundingIncrement = positive.rounding.toDouble();
        } else {
            properties.minimumFractionDigits = -1;
            properties.maximumFractionDigits = -1;
            properties.roundingIncrement = 0.0;
        }
        properties.minimumSignificantDigits = -1;
        properties.maximumSignificantDigits = -1;
    } else {
        if (!ignoreRounding) {
            properties.minimumFractionDigits = minFrac;
            properties.maximumFractionDigits = positive.fractionTotal;
            properties.roundingIncrement = 0.0;
        } else {
            properties.minimumFractionDigits = -1;
            properties.maximumFractionDigits = -1;
            properties.roundingIncrement = 0.0;
        }
        properties.minimumSignificantDigits = -1;
        properties.maximumSignificantDigits = -1;
    }

    // If the pattern ends with a '.' then force the decimal point.
    if (positive.hasDecimal && positive.fractionTotal == 0) {
        properties.decimalSeparatorAlwaysShown = true;
    } else {
        properties.decimalSeparatorAlwaysShown = false;
    }

    // Scientific notation settings
    if (positive.exponentZeros > 0) {
        properties.exponentSignAlwaysShown = positive.exponentHasPlusSign;
        properties.minimumExponentDigits = positive.exponentZeros;
        if (positive.integerAtSigns == 0) {
            // patterns without '@' can define max integer digits, used for engineering notation
            properties.minimumIntegerDigits = positive.integerNumerals;
            properties.maximumIntegerDigits = positive.integerTotal;
        } else {
            // patterns with '@' cannot define max integer digits
            properties.minimumIntegerDigits = 1;
            properties.maximumIntegerDigits = -1;
        }
    } else {
        properties.exponentSignAlwaysShown = false;
        properties.minimumExponentDigits = -1;
        properties.minimumIntegerDigits = minInt;
        properties.maximumIntegerDigits = -1;
    }

    // Compute the affix patterns (required for both padding and affixes)
    UnicodeString posPrefix = patternInfo.getString(AffixPatternProvider::AFFIX_PREFIX);
    UnicodeString posSuffix = patternInfo.getString(0);

    // Padding settings
    if (!positive.paddingLocation.isNull()) {
        // The width of the positive prefix and suffix templates are included in the padding
        int paddingWidth =
                positive.widthExceptAffixes + AffixUtils::estimateLength(UnicodeStringCharSequence(posPrefix), status) +
                AffixUtils::estimateLength(UnicodeStringCharSequence(posSuffix), status);
        properties.formatWidth = paddingWidth;
        UnicodeString rawPaddingString = patternInfo.getString(AffixPatternProvider::AFFIX_PADDING);
        if (rawPaddingString.length() == 1) {
            properties.padString = rawPaddingString;
        } else if (rawPaddingString.length() == 2) {
            if (rawPaddingString.charAt(0) == u'\'') {
                properties.padString.setTo(u"'", -1);
            } else {
                properties.padString = rawPaddingString;
            }
        } else {
            properties.padString = UnicodeString(rawPaddingString, 1, rawPaddingString.length() - 2);
        }
        properties.padPosition = positive.paddingLocation;
    } else {
        properties.formatWidth = -1;
        properties.padString.setToBogus();
        properties.padPosition.nullify();
    }

    // Set the affixes
    // Always call the setter, even if the prefixes are empty, especially in the case of the
    // negative prefix pattern, to prevent default values from overriding the pattern.
    properties.positivePrefixPattern = posPrefix;
    properties.positiveSuffixPattern = posSuffix;
    if (patternInfo.fHasNegativeSubpattern) {
        properties.negativePrefixPattern = patternInfo.getString(
                AffixPatternProvider::AFFIX_NEGATIVE_SUBPATTERN | AffixPatternProvider::AFFIX_PREFIX);
        properties.negativeSuffixPattern = patternInfo.getString(
                AffixPatternProvider::AFFIX_NEGATIVE_SUBPATTERN);
    } else {
        properties.negativePrefixPattern.setToBogus();
        properties.negativeSuffixPattern.setToBogus();
    }

    // Set the magnitude multiplier
    if (positive.hasPercentSign) {
        properties.magnitudeMultiplier = 2;
    } else if (positive.hasPerMilleSign) {
        properties.magnitudeMultiplier = 3;
    } else {
        properties.magnitudeMultiplier = 0;
    }
}

///////////////////////////////////////////////////////////////////
/// End PatternStringParser.java; begin PatternStringUtils.java ///
///////////////////////////////////////////////////////////////////

UnicodeString PatternStringUtils::propertiesToPatternString(const DecimalFormatProperties &properties,
                                                            UErrorCode &status) {
    UnicodeString sb;

    // Convenience references
    // The uprv_min() calls prevent DoS
    int dosMax = 100;
    int groupingSize = uprv_min(properties.secondaryGroupingSize, dosMax);
    int firstGroupingSize = uprv_min(properties.groupingSize, dosMax);
    int paddingWidth = uprv_min(properties.formatWidth, dosMax);
    NullableValue<PadPosition> paddingLocation = properties.padPosition;
    UnicodeString paddingString = properties.padString;
    int minInt = uprv_max(uprv_min(properties.minimumIntegerDigits, dosMax), 0);
    int maxInt = uprv_min(properties.maximumIntegerDigits, dosMax);
    int minFrac = uprv_max(uprv_min(properties.minimumFractionDigits, dosMax), 0);
    int maxFrac = uprv_min(properties.maximumFractionDigits, dosMax);
    int minSig = uprv_min(properties.minimumSignificantDigits, dosMax);
    int maxSig = uprv_min(properties.maximumSignificantDigits, dosMax);
    bool alwaysShowDecimal = properties.decimalSeparatorAlwaysShown;
    int exponentDigits = uprv_min(properties.minimumExponentDigits, dosMax);
    bool exponentShowPlusSign = properties.exponentSignAlwaysShown;
    UnicodeString pp = properties.positivePrefix;
    UnicodeString ppp = properties.positivePrefixPattern;
    UnicodeString ps = properties.positiveSuffix;
    UnicodeString psp = properties.positiveSuffixPattern;
    UnicodeString np = properties.negativePrefix;
    UnicodeString npp = properties.negativePrefixPattern;
    UnicodeString ns = properties.negativeSuffix;
    UnicodeString nsp = properties.negativeSuffixPattern;

    // Prefixes
    if (!ppp.isBogus()) {
        sb.append(ppp);
    }
    sb.append(AffixUtils::escape(UnicodeStringCharSequence(pp)));
    int afterPrefixPos = sb.length();

    // Figure out the grouping sizes.
    int grouping1, grouping2, grouping;
    if (groupingSize != uprv_min(dosMax, -1) && firstGroupingSize != uprv_min(dosMax, -1) &&
        groupingSize != firstGroupingSize) {
        grouping = groupingSize;
        grouping1 = groupingSize;
        grouping2 = firstGroupingSize;
    } else if (groupingSize != uprv_min(dosMax, -1)) {
        grouping = groupingSize;
        grouping1 = 0;
        grouping2 = groupingSize;
    } else if (firstGroupingSize != uprv_min(dosMax, -1)) {
        grouping = groupingSize;
        grouping1 = 0;
        grouping2 = firstGroupingSize;
    } else {
        grouping = 0;
        grouping1 = 0;
        grouping2 = 0;
    }
    int groupingLength = grouping1 + grouping2 + 1;

    // Figure out the digits we need to put in the pattern.
    double roundingInterval = properties.roundingIncrement;
    UnicodeString digitsString;
    int digitsStringScale = 0;
    if (maxSig != uprv_min(dosMax, -1)) {
        // Significant Digits.
        while (digitsString.length() < minSig) {
            digitsString.append(u'@');
        }
        while (digitsString.length() < maxSig) {
            digitsString.append(u'#');
        }
    } else if (roundingInterval != 0.0) {
        // Rounding Interval.
        digitsStringScale = minFrac;
        // TODO: Check for DoS here?
        DecimalQuantity incrementQuantity;
        incrementQuantity.setToDouble(roundingInterval);
        incrementQuantity.adjustMagnitude(minFrac);
        incrementQuantity.roundToMagnitude(0, kDefaultMode, status);
        UnicodeString str = incrementQuantity.toPlainString();
        if (str.charAt(0) == u'-') {
            // TODO: Unsupported operation exception or fail silently?
            digitsString.append(str, 1, str.length() - 1);
        } else {
            digitsString.append(str);
        }
    }
    while (digitsString.length() + digitsStringScale < minInt) {
        digitsString.insert(0, u'0');
    }
    while (-digitsStringScale < minFrac) {
        digitsString.append(u'0');
        digitsStringScale--;
    }

    // Write the digits to the string builder
    int m0 = uprv_max(groupingLength, digitsString.length() + digitsStringScale);
    m0 = (maxInt != dosMax) ? uprv_max(maxInt, m0) - 1 : m0 - 1;
    int mN = (maxFrac != dosMax) ? uprv_min(-maxFrac, digitsStringScale) : digitsStringScale;
    for (int magnitude = m0; magnitude >= mN; magnitude--) {
        int di = digitsString.length() + digitsStringScale - magnitude - 1;
        if (di < 0 || di >= digitsString.length()) {
            sb.append(u'#');
        } else {
            sb.append(digitsString.charAt(di));
        }
        if (magnitude > grouping2 && grouping > 0 && (magnitude - grouping2) % grouping == 0) {
            sb.append(u',');
        } else if (magnitude > 0 && magnitude == grouping2) {
            sb.append(u',');
        } else if (magnitude == 0 && (alwaysShowDecimal || mN < 0)) {
            sb.append(u'.');
        }
    }

    // Exponential notation
    if (exponentDigits != uprv_min(dosMax, -1)) {
        sb.append(u'E');
        if (exponentShowPlusSign) {
            sb.append(u'+');
        }
        for (int i = 0; i < exponentDigits; i++) {
            sb.append(u'0');
        }
    }

    // Suffixes
    int beforeSuffixPos = sb.length();
    if (!psp.isBogus()) {
        sb.append(psp);
    }
    sb.append(AffixUtils::escape(UnicodeStringCharSequence(ps)));

    // Resolve Padding
    if (paddingWidth != -1 && !paddingLocation.isNull()) {
        while (paddingWidth - sb.length() > 0) {
            sb.insert(afterPrefixPos, u'#');
            beforeSuffixPos++;
        }
        int addedLength;
        switch (paddingLocation.get(status)) {
            case PadPosition::UNUM_PAD_BEFORE_PREFIX:
                addedLength = escapePaddingString(paddingString, sb, 0, status);
                sb.insert(0, u'*');
                afterPrefixPos += addedLength + 1;
                beforeSuffixPos += addedLength + 1;
                break;
            case PadPosition::UNUM_PAD_AFTER_PREFIX:
                addedLength = escapePaddingString(paddingString, sb, afterPrefixPos, status);
                sb.insert(afterPrefixPos, u'*');
                afterPrefixPos += addedLength + 1;
                beforeSuffixPos += addedLength + 1;
                break;
            case PadPosition::UNUM_PAD_BEFORE_SUFFIX:
                escapePaddingString(paddingString, sb, beforeSuffixPos, status);
                sb.insert(beforeSuffixPos, u'*');
                break;
            case PadPosition::UNUM_PAD_AFTER_SUFFIX:
                sb.append(u'*');
                escapePaddingString(paddingString, sb, sb.length(), status);
                break;
        }
        if (U_FAILURE(status)) { return sb; }
    }

    // Negative affixes
    // Ignore if the negative prefix pattern is "-" and the negative suffix is empty
    if (!np.isBogus() || !ns.isBogus() || (npp.isBogus() && !nsp.isBogus()) ||
        (!npp.isBogus() && (npp.length() != 1 || npp.charAt(0) != u'-' || nsp.length() != 0))) {
        sb.append(u';');
        if (!npp.isBogus()) {
            sb.append(npp);
        }
        sb.append(AffixUtils::escape(UnicodeStringCharSequence(np)));
        // Copy the positive digit format into the negative.
        // This is optional; the pattern is the same as if '#' were appended here instead.
        sb.append(sb, afterPrefixPos, beforeSuffixPos);
        if (!nsp.isBogus()) {
            sb.append(nsp);
        }
        sb.append(AffixUtils::escape(UnicodeStringCharSequence(ns)));
    }

    return sb;
}

int PatternStringUtils::escapePaddingString(UnicodeString input, UnicodeString& output, int startIndex,
                                            UErrorCode &status) {
    (void)status;
    if (input.length() == 0) {
        input.setTo(kFallbackPaddingString, -1);
    }
    int startLength = output.length();
    if (input.length() == 1) {
        if (input.compare(u"'", -1) == 0) {
            output.insert(startIndex, u"''", -1);
        } else {
            output.insert(startIndex, input);
        }
    } else {
        output.insert(startIndex, u'\'');
        int offset = 1;
        for (int i = 0; i < input.length(); i++) {
            // it's okay to deal in chars here because the quote mark is the only interesting thing.
            char16_t ch = input.charAt(i);
            if (ch == u'\'') {
                output.insert(startIndex + offset, u"''", -1);
                offset += 2;
            } else {
                output.insert(startIndex + offset, ch);
                offset += 1;
            }
        }
        output.insert(startIndex + offset, u'\'');
    }
    return output.length() - startLength;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
