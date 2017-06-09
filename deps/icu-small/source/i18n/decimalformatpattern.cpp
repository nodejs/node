// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "uassert.h"
#include "decimalformatpattern.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"
#include "unicode/format.h"
#include "unicode/utf16.h"
#include "decimalformatpatternimpl.h"


#ifdef FMT_DEBUG
#define debug(x) printf("%s:%d: %s\n", __FILE__,__LINE__, x);
#else
#define debug(x)
#endif

U_NAMESPACE_BEGIN

// TODO: Travis Keep: Copied from numfmt.cpp
static int32_t kDoubleIntegerDigits  = 309;
static int32_t kDoubleFractionDigits = 340;


// TODO: Travis Keep: Copied from numfmt.cpp
static int32_t gDefaultMaxIntegerDigits = 2000000000;

// TODO: Travis Keep: This function was copied from format.cpp
static void syntaxError(const UnicodeString& pattern,
                         int32_t pos,
                         UParseError& parseError) {
    parseError.offset = pos;
    parseError.line=0;  // we are not using line number

    // for pre-context
    int32_t start = (pos < U_PARSE_CONTEXT_LEN)? 0 : (pos - (U_PARSE_CONTEXT_LEN-1
                                                             /* subtract 1 so that we have room for null*/));
    int32_t stop  = pos;
    pattern.extract(start,stop-start,parseError.preContext,0);
    //null terminate the buffer
    parseError.preContext[stop-start] = 0;

    //for post-context
    start = pos+1;
    stop  = ((pos+U_PARSE_CONTEXT_LEN)<=pattern.length()) ? (pos+(U_PARSE_CONTEXT_LEN-1)) :
        pattern.length();
    pattern.extract(start,stop-start,parseError.postContext,0);
    //null terminate the buffer
    parseError.postContext[stop-start]= 0;
}

DecimalFormatPattern::DecimalFormatPattern()
        : fMinimumIntegerDigits(1),
          fMaximumIntegerDigits(gDefaultMaxIntegerDigits),
          fMinimumFractionDigits(0),
          fMaximumFractionDigits(3),
          fUseSignificantDigits(FALSE),
          fMinimumSignificantDigits(1),
          fMaximumSignificantDigits(6),
          fUseExponentialNotation(FALSE),
          fMinExponentDigits(0),
          fExponentSignAlwaysShown(FALSE),
          fCurrencySignCount(fgCurrencySignCountZero),
          fGroupingUsed(TRUE),
          fGroupingSize(0),
          fGroupingSize2(0),
          fMultiplier(1),
          fDecimalSeparatorAlwaysShown(FALSE),
          fFormatWidth(0),
          fRoundingIncrementUsed(FALSE),
          fRoundingIncrement(),
          fPad(kDefaultPad),
          fNegPatternsBogus(TRUE),
          fPosPatternsBogus(TRUE),
          fNegPrefixPattern(),
          fNegSuffixPattern(),
          fPosPrefixPattern(),
          fPosSuffixPattern(),
          fPadPosition(DecimalFormatPattern::kPadBeforePrefix) {
}


DecimalFormatPatternParser::DecimalFormatPatternParser() :
    fZeroDigit(kPatternZeroDigit),
    fSigDigit(kPatternSignificantDigit),
    fGroupingSeparator((UChar)kPatternGroupingSeparator),
    fDecimalSeparator((UChar)kPatternDecimalSeparator),
    fPercent((UChar)kPatternPercent),
    fPerMill((UChar)kPatternPerMill),
    fDigit((UChar)kPatternDigit),
    fSeparator((UChar)kPatternSeparator),
    fExponent((UChar)kPatternExponent),
    fPlus((UChar)kPatternPlus),
    fMinus((UChar)kPatternMinus),
    fPadEscape((UChar)kPatternPadEscape) {
}

void DecimalFormatPatternParser::useSymbols(
        const DecimalFormatSymbols& symbols) {
    fZeroDigit = symbols.getConstSymbol(
            DecimalFormatSymbols::kZeroDigitSymbol).char32At(0);
    fSigDigit = symbols.getConstSymbol(
            DecimalFormatSymbols::kSignificantDigitSymbol).char32At(0);
    fGroupingSeparator = symbols.getConstSymbol(
            DecimalFormatSymbols::kGroupingSeparatorSymbol);
    fDecimalSeparator = symbols.getConstSymbol(
            DecimalFormatSymbols::kDecimalSeparatorSymbol);
    fPercent = symbols.getConstSymbol(
            DecimalFormatSymbols::kPercentSymbol);
    fPerMill = symbols.getConstSymbol(
            DecimalFormatSymbols::kPerMillSymbol);
    fDigit = symbols.getConstSymbol(
            DecimalFormatSymbols::kDigitSymbol);
    fSeparator = symbols.getConstSymbol(
            DecimalFormatSymbols::kPatternSeparatorSymbol);
    fExponent = symbols.getConstSymbol(
            DecimalFormatSymbols::kExponentialSymbol);
    fPlus = symbols.getConstSymbol(
            DecimalFormatSymbols::kPlusSignSymbol);
    fMinus = symbols.getConstSymbol(
            DecimalFormatSymbols::kMinusSignSymbol);
    fPadEscape = symbols.getConstSymbol(
            DecimalFormatSymbols::kPadEscapeSymbol);
}

void
DecimalFormatPatternParser::applyPatternWithoutExpandAffix(
        const UnicodeString& pattern,
        DecimalFormatPattern& out,
        UParseError& parseError,
        UErrorCode& status) {
    if (U_FAILURE(status))
    {
        return;
    }
    out = DecimalFormatPattern();

    // Clear error struct
    parseError.offset = -1;
    parseError.preContext[0] = parseError.postContext[0] = (UChar)0;

    // TODO: Travis Keep: This won't always work.
    UChar nineDigit = (UChar)(fZeroDigit + 9);
    int32_t digitLen = fDigit.length();
    int32_t groupSepLen = fGroupingSeparator.length();
    int32_t decimalSepLen = fDecimalSeparator.length();

    int32_t pos = 0;
    int32_t patLen = pattern.length();
    // Part 0 is the positive pattern.  Part 1, if present, is the negative
    // pattern.
    for (int32_t part=0; part<2 && pos<patLen; ++part) {
        // The subpart ranges from 0 to 4: 0=pattern proper, 1=prefix,
        // 2=suffix, 3=prefix in quote, 4=suffix in quote.  Subpart 0 is
        // between the prefix and suffix, and consists of pattern
        // characters.  In the prefix and suffix, percent, perMill, and
        // currency symbols are recognized and translated.
        int32_t subpart = 1, sub0Start = 0, sub0Limit = 0, sub2Limit = 0;

        // It's important that we don't change any fields of this object
        // prematurely.  We set the following variables for the multiplier,
        // grouping, etc., and then only change the actual object fields if
        // everything parses correctly.  This also lets us register
        // the data from part 0 and ignore the part 1, except for the
        // prefix and suffix.
        UnicodeString prefix;
        UnicodeString suffix;
        int32_t decimalPos = -1;
        int32_t multiplier = 1;
        int32_t digitLeftCount = 0, zeroDigitCount = 0, digitRightCount = 0, sigDigitCount = 0;
        int8_t groupingCount = -1;
        int8_t groupingCount2 = -1;
        int32_t padPos = -1;
        UChar32 padChar = 0;
        int32_t roundingPos = -1;
        DigitList roundingInc;
        int8_t expDigits = -1;
        UBool expSignAlways = FALSE;

        // The affix is either the prefix or the suffix.
        UnicodeString* affix = &prefix;

        int32_t start = pos;
        UBool isPartDone = FALSE;
        UChar32 ch;

        for (; !isPartDone && pos < patLen; ) {
            // Todo: account for surrogate pairs
            ch = pattern.char32At(pos);
            switch (subpart) {
            case 0: // Pattern proper subpart (between prefix & suffix)
                // Process the digits, decimal, and grouping characters.  We
                // record five pieces of information.  We expect the digits
                // to occur in the pattern ####00.00####, and we record the
                // number of left digits, zero (central) digits, and right
                // digits.  The position of the last grouping character is
                // recorded (should be somewhere within the first two blocks
                // of characters), as is the position of the decimal point,
                // if any (should be in the zero digits).  If there is no
                // decimal point, then there should be no right digits.
                if (pattern.compare(pos, digitLen, fDigit) == 0) {
                    if (zeroDigitCount > 0 || sigDigitCount > 0) {
                        ++digitRightCount;
                    } else {
                        ++digitLeftCount;
                    }
                    if (groupingCount >= 0 && decimalPos < 0) {
                        ++groupingCount;
                    }
                    pos += digitLen;
                } else if ((ch >= fZeroDigit && ch <= nineDigit) ||
                           ch == fSigDigit) {
                    if (digitRightCount > 0) {
                        // Unexpected '0'
                        debug("Unexpected '0'")
                        status = U_UNEXPECTED_TOKEN;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    if (ch == fSigDigit) {
                        ++sigDigitCount;
                    } else {
                        if (ch != fZeroDigit && roundingPos < 0) {
                            roundingPos = digitLeftCount + zeroDigitCount;
                        }
                        if (roundingPos >= 0) {
                            roundingInc.append((char)(ch - fZeroDigit + '0'));
                        }
                        ++zeroDigitCount;
                    }
                    if (groupingCount >= 0 && decimalPos < 0) {
                        ++groupingCount;
                    }
                    pos += U16_LENGTH(ch);
                } else if (pattern.compare(pos, groupSepLen, fGroupingSeparator) == 0) {
                    if (decimalPos >= 0) {
                        // Grouping separator after decimal
                        debug("Grouping separator after decimal")
                        status = U_UNEXPECTED_TOKEN;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    groupingCount2 = groupingCount;
                    groupingCount = 0;
                    pos += groupSepLen;
                } else if (pattern.compare(pos, decimalSepLen, fDecimalSeparator) == 0) {
                    if (decimalPos >= 0) {
                        // Multiple decimal separators
                        debug("Multiple decimal separators")
                        status = U_MULTIPLE_DECIMAL_SEPARATORS;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    // Intentionally incorporate the digitRightCount,
                    // even though it is illegal for this to be > 0
                    // at this point.  We check pattern syntax below.
                    decimalPos = digitLeftCount + zeroDigitCount + digitRightCount;
                    pos += decimalSepLen;
                } else {
                    if (pattern.compare(pos, fExponent.length(), fExponent) == 0) {
                        if (expDigits >= 0) {
                            // Multiple exponential symbols
                            debug("Multiple exponential symbols")
                            status = U_MULTIPLE_EXPONENTIAL_SYMBOLS;
                            syntaxError(pattern,pos,parseError);
                            return;
                        }
                        if (groupingCount >= 0) {
                            // Grouping separator in exponential pattern
                            debug("Grouping separator in exponential pattern")
                            status = U_MALFORMED_EXPONENTIAL_PATTERN;
                            syntaxError(pattern,pos,parseError);
                            return;
                        }
                        pos += fExponent.length();
                        // Check for positive prefix
                        if (pos < patLen
                            && pattern.compare(pos, fPlus.length(), fPlus) == 0) {
                            expSignAlways = TRUE;
                            pos += fPlus.length();
                        }
                        // Use lookahead to parse out the exponential part of the
                        // pattern, then jump into suffix subpart.
                        expDigits = 0;
                        while (pos < patLen &&
                               pattern.char32At(pos) == fZeroDigit) {
                            ++expDigits;
                            pos += U16_LENGTH(fZeroDigit);
                        }

                        // 1. Require at least one mantissa pattern digit
                        // 2. Disallow "#+ @" in mantissa
                        // 3. Require at least one exponent pattern digit
                        if (((digitLeftCount + zeroDigitCount) < 1 &&
                             (sigDigitCount + digitRightCount) < 1) ||
                            (sigDigitCount > 0 && digitLeftCount > 0) ||
                            expDigits < 1) {
                            // Malformed exponential pattern
                            debug("Malformed exponential pattern")
                            status = U_MALFORMED_EXPONENTIAL_PATTERN;
                            syntaxError(pattern,pos,parseError);
                            return;
                        }
                    }
                    // Transition to suffix subpart
                    subpart = 2; // suffix subpart
                    affix = &suffix;
                    sub0Limit = pos;
                    continue;
                }
                break;
            case 1: // Prefix subpart
            case 2: // Suffix subpart
                // Process the prefix / suffix characters
                // Process unquoted characters seen in prefix or suffix
                // subpart.

                // Several syntax characters implicitly begins the
                // next subpart if we are in the prefix; otherwise
                // they are illegal if unquoted.
                if (!pattern.compare(pos, digitLen, fDigit) ||
                    !pattern.compare(pos, groupSepLen, fGroupingSeparator) ||
                    !pattern.compare(pos, decimalSepLen, fDecimalSeparator) ||
                    (ch >= fZeroDigit && ch <= nineDigit) ||
                    ch == fSigDigit) {
                    if (subpart == 1) { // prefix subpart
                        subpart = 0; // pattern proper subpart
                        sub0Start = pos; // Reprocess this character
                        continue;
                    } else {
                        status = U_UNQUOTED_SPECIAL;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                } else if (ch == kCurrencySign) {
                    affix->append(kQuote); // Encode currency
                    // Use lookahead to determine if the currency sign is
                    // doubled or not.
                    U_ASSERT(U16_LENGTH(kCurrencySign) == 1);
                    if ((pos+1) < pattern.length() && pattern[pos+1] == kCurrencySign) {
                        affix->append(kCurrencySign);
                        ++pos; // Skip over the doubled character
                        if ((pos+1) < pattern.length() &&
                            pattern[pos+1] == kCurrencySign) {
                            affix->append(kCurrencySign);
                            ++pos; // Skip over the doubled character
                            out.fCurrencySignCount = fgCurrencySignCountInPluralFormat;
                        } else {
                            out.fCurrencySignCount = fgCurrencySignCountInISOFormat;
                        }
                    } else {
                        out.fCurrencySignCount = fgCurrencySignCountInSymbolFormat;
                    }
                    // Fall through to append(ch)
                } else if (ch == kQuote) {
                    // A quote outside quotes indicates either the opening
                    // quote or two quotes, which is a quote literal.  That is,
                    // we have the first quote in 'do' or o''clock.
                    U_ASSERT(U16_LENGTH(kQuote) == 1);
                    ++pos;
                    if (pos < pattern.length() && pattern[pos] == kQuote) {
                        affix->append(kQuote); // Encode quote
                        // Fall through to append(ch)
                    } else {
                        subpart += 2; // open quote
                        continue;
                    }
                } else if (pattern.compare(pos, fSeparator.length(), fSeparator) == 0) {
                    // Don't allow separators in the prefix, and don't allow
                    // separators in the second pattern (part == 1).
                    if (subpart == 1 || part == 1) {
                        // Unexpected separator
                        debug("Unexpected separator")
                        status = U_UNEXPECTED_TOKEN;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    sub2Limit = pos;
                    isPartDone = TRUE; // Go to next part
                    pos += fSeparator.length();
                    break;
                } else if (pattern.compare(pos, fPercent.length(), fPercent) == 0) {
                    // Next handle characters which are appended directly.
                    if (multiplier != 1) {
                        // Too many percent/perMill characters
                        debug("Too many percent characters")
                        status = U_MULTIPLE_PERCENT_SYMBOLS;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    affix->append(kQuote); // Encode percent/perMill
                    affix->append(kPatternPercent); // Use unlocalized pattern char
                    multiplier = 100;
                    pos += fPercent.length();
                    break;
                } else if (pattern.compare(pos, fPerMill.length(), fPerMill) == 0) {
                    // Next handle characters which are appended directly.
                    if (multiplier != 1) {
                        // Too many percent/perMill characters
                        debug("Too many perMill characters")
                        status = U_MULTIPLE_PERMILL_SYMBOLS;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    affix->append(kQuote); // Encode percent/perMill
                    affix->append(kPatternPerMill); // Use unlocalized pattern char
                    multiplier = 1000;
                    pos += fPerMill.length();
                    break;
                } else if (pattern.compare(pos, fPadEscape.length(), fPadEscape) == 0) {
                    if (padPos >= 0 ||               // Multiple pad specifiers
                        (pos+1) == pattern.length()) { // Nothing after padEscape
                        debug("Multiple pad specifiers")
                        status = U_MULTIPLE_PAD_SPECIFIERS;
                        syntaxError(pattern,pos,parseError);
                        return;
                    }
                    padPos = pos;
                    pos += fPadEscape.length();
                    padChar = pattern.char32At(pos);
                    pos += U16_LENGTH(padChar);
                    break;
                } else if (pattern.compare(pos, fMinus.length(), fMinus) == 0) {
                    affix->append(kQuote); // Encode minus
                    affix->append(kPatternMinus);
                    pos += fMinus.length();
                    break;
                } else if (pattern.compare(pos, fPlus.length(), fPlus) == 0) {
                    affix->append(kQuote); // Encode plus
                    affix->append(kPatternPlus);
                    pos += fPlus.length();
                    break;
                }
                // Unquoted, non-special characters fall through to here, as
                // well as other code which needs to append something to the
                // affix.
                affix->append(ch);
                pos += U16_LENGTH(ch);
                break;
            case 3: // Prefix subpart, in quote
            case 4: // Suffix subpart, in quote
                // A quote within quotes indicates either the closing
                // quote or two quotes, which is a quote literal.  That is,
                // we have the second quote in 'do' or 'don''t'.
                if (ch == kQuote) {
                    ++pos;
                    if (pos < pattern.length() && pattern[pos] == kQuote) {
                        affix->append(kQuote); // Encode quote
                        // Fall through to append(ch)
                    } else {
                        subpart -= 2; // close quote
                        continue;
                    }
                }
                affix->append(ch);
                pos += U16_LENGTH(ch);
                break;
            }
        }

        if (sub0Limit == 0) {
            sub0Limit = pattern.length();
        }

        if (sub2Limit == 0) {
            sub2Limit = pattern.length();
        }

        /* Handle patterns with no '0' pattern character.  These patterns
         * are legal, but must be recodified to make sense.  "##.###" ->
         * "#0.###".  ".###" -> ".0##".
         *
         * We allow patterns of the form "####" to produce a zeroDigitCount
         * of zero (got that?); although this seems like it might make it
         * possible for format() to produce empty strings, format() checks
         * for this condition and outputs a zero digit in this situation.
         * Having a zeroDigitCount of zero yields a minimum integer digits
         * of zero, which allows proper round-trip patterns.  We don't want
         * "#" to become "#0" when toPattern() is called (even though that's
         * what it really is, semantically).
         */
        if (zeroDigitCount == 0 && sigDigitCount == 0 &&
            digitLeftCount > 0 && decimalPos >= 0) {
            // Handle "###.###" and "###." and ".###"
            int n = decimalPos;
            if (n == 0)
                ++n; // Handle ".###"
            digitRightCount = digitLeftCount - n;
            digitLeftCount = n - 1;
            zeroDigitCount = 1;
        }

        // Do syntax checking on the digits, decimal points, and quotes.
        if ((decimalPos < 0 && digitRightCount > 0 && sigDigitCount == 0) ||
            (decimalPos >= 0 &&
             (sigDigitCount > 0 ||
              decimalPos < digitLeftCount ||
              decimalPos > (digitLeftCount + zeroDigitCount))) ||
            groupingCount == 0 || groupingCount2 == 0 ||
            (sigDigitCount > 0 && zeroDigitCount > 0) ||
            subpart > 2)
        { // subpart > 2 == unmatched quote
            debug("Syntax error")
            status = U_PATTERN_SYNTAX_ERROR;
            syntaxError(pattern,pos,parseError);
            return;
        }

        // Make sure pad is at legal position before or after affix.
        if (padPos >= 0) {
            if (padPos == start) {
                padPos = DecimalFormatPattern::kPadBeforePrefix;
            } else if (padPos+2 == sub0Start) {
                padPos = DecimalFormatPattern::kPadAfterPrefix;
            } else if (padPos == sub0Limit) {
                padPos = DecimalFormatPattern::kPadBeforeSuffix;
            } else if (padPos+2 == sub2Limit) {
                padPos = DecimalFormatPattern::kPadAfterSuffix;
            } else {
                // Illegal pad position
                debug("Illegal pad position")
                status = U_ILLEGAL_PAD_POSITION;
                syntaxError(pattern,pos,parseError);
                return;
            }
        }

        if (part == 0) {
            out.fPosPatternsBogus = FALSE;
            out.fPosPrefixPattern = prefix;
            out.fPosSuffixPattern = suffix;
            out.fNegPatternsBogus = TRUE;
            out.fNegPrefixPattern.remove();
            out.fNegSuffixPattern.remove();

            out.fUseExponentialNotation = (expDigits >= 0);
            if (out.fUseExponentialNotation) {
              out.fMinExponentDigits = expDigits;
            }
            out.fExponentSignAlwaysShown = expSignAlways;
            int32_t digitTotalCount = digitLeftCount + zeroDigitCount + digitRightCount;
            // The effectiveDecimalPos is the position the decimal is at or
            // would be at if there is no decimal.  Note that if
            // decimalPos<0, then digitTotalCount == digitLeftCount +
            // zeroDigitCount.
            int32_t effectiveDecimalPos = decimalPos >= 0 ? decimalPos : digitTotalCount;
            UBool isSigDig = (sigDigitCount > 0);
            out.fUseSignificantDigits = isSigDig;
            if (isSigDig) {
                out.fMinimumSignificantDigits = sigDigitCount;
                out.fMaximumSignificantDigits = sigDigitCount + digitRightCount;
            } else {
                int32_t minInt = effectiveDecimalPos - digitLeftCount;
                out.fMinimumIntegerDigits = minInt;
                out.fMaximumIntegerDigits = out.fUseExponentialNotation
                    ? digitLeftCount + out.fMinimumIntegerDigits
                    : gDefaultMaxIntegerDigits;
                out.fMaximumFractionDigits = decimalPos >= 0
                    ? (digitTotalCount - decimalPos) : 0;
                out.fMinimumFractionDigits = decimalPos >= 0
                    ? (digitLeftCount + zeroDigitCount - decimalPos) : 0;
            }
            out.fGroupingUsed = groupingCount > 0;
            out.fGroupingSize = (groupingCount > 0) ? groupingCount : 0;
            out.fGroupingSize2 = (groupingCount2 > 0 && groupingCount2 != groupingCount)
                ? groupingCount2 : 0;
            out.fMultiplier = multiplier;
            out.fDecimalSeparatorAlwaysShown = decimalPos == 0
                    || decimalPos == digitTotalCount;
            if (padPos >= 0) {
                out.fPadPosition = (DecimalFormatPattern::EPadPosition) padPos;
                // To compute the format width, first set up sub0Limit -
                // sub0Start.  Add in prefix/suffix length later.

                // fFormatWidth = prefix.length() + suffix.length() +
                //    sub0Limit - sub0Start;
                out.fFormatWidth = sub0Limit - sub0Start;
                out.fPad = padChar;
            } else {
                out.fFormatWidth = 0;
            }
            if (roundingPos >= 0) {
                out.fRoundingIncrementUsed = TRUE;
                roundingInc.setDecimalAt(effectiveDecimalPos - roundingPos);
                out.fRoundingIncrement = roundingInc;
            } else {
                out.fRoundingIncrementUsed = FALSE;
            }
        } else {
            out.fNegPatternsBogus = FALSE;
            out.fNegPrefixPattern = prefix;
            out.fNegSuffixPattern = suffix;
        }
    }

    if (pattern.length() == 0) {
        out.fNegPatternsBogus = TRUE;
        out.fNegPrefixPattern.remove();
        out.fNegSuffixPattern.remove();
        out.fPosPatternsBogus = FALSE;
        out.fPosPrefixPattern.remove();
        out.fPosSuffixPattern.remove();

        out.fMinimumIntegerDigits = 0;
        out.fMaximumIntegerDigits = kDoubleIntegerDigits;
        out.fMinimumFractionDigits = 0;
        out.fMaximumFractionDigits = kDoubleFractionDigits;

        out.fUseExponentialNotation = FALSE;
        out.fCurrencySignCount = fgCurrencySignCountZero;
        out.fGroupingUsed = FALSE;
        out.fGroupingSize = 0;
        out.fGroupingSize2 = 0;
        out.fMultiplier = 1;
        out.fDecimalSeparatorAlwaysShown = FALSE;
        out.fFormatWidth = 0;
        out.fRoundingIncrementUsed = FALSE;
    }

    // If there was no negative pattern, or if the negative pattern is
    // identical to the positive pattern, then prepend the minus sign to the
    // positive pattern to form the negative pattern.
    if (out.fNegPatternsBogus ||
        (out.fNegPrefixPattern == out.fPosPrefixPattern
         && out.fNegSuffixPattern == out.fPosSuffixPattern)) {
        out.fNegPatternsBogus = FALSE;
        out.fNegSuffixPattern = out.fPosSuffixPattern;
        out.fNegPrefixPattern.remove();
        out.fNegPrefixPattern.append(kQuote).append(kPatternMinus)
            .append(out.fPosPrefixPattern);
    }
    // TODO: Deprecate/Remove out.fNegSuffixPattern and 3 other fields.
    AffixPattern::parseAffixString(
            out.fNegSuffixPattern, out.fNegSuffixAffix, status);
    AffixPattern::parseAffixString(
            out.fPosSuffixPattern, out.fPosSuffixAffix, status);
    AffixPattern::parseAffixString(
            out.fNegPrefixPattern, out.fNegPrefixAffix, status);
    AffixPattern::parseAffixString(
            out.fPosPrefixPattern, out.fPosPrefixAffix, status);
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
