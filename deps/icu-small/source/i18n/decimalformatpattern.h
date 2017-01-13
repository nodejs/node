// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 1997-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/
#ifndef _DECIMAL_FORMAT_PATTERN
#define _DECIMAL_FORMAT_PATTERN

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"
#include "unicode/unistr.h"
#include "digitlst.h"
#include "affixpatternparser.h"

U_NAMESPACE_BEGIN

// currency sign count
enum CurrencySignCount {
    fgCurrencySignCountZero,
    fgCurrencySignCountInSymbolFormat,
    fgCurrencySignCountInISOFormat,
    fgCurrencySignCountInPluralFormat
};

class DecimalFormatSymbols;

struct DecimalFormatPattern : public UMemory {
  enum EPadPosition {
      kPadBeforePrefix,
      kPadAfterPrefix,
      kPadBeforeSuffix,
      kPadAfterSuffix
  };

  DecimalFormatPattern();

  int32_t fMinimumIntegerDigits;
  int32_t fMaximumIntegerDigits;
  int32_t fMinimumFractionDigits;
  int32_t fMaximumFractionDigits;
  UBool fUseSignificantDigits;
  int32_t fMinimumSignificantDigits;
  int32_t fMaximumSignificantDigits;
  UBool fUseExponentialNotation;
  int32_t fMinExponentDigits;
  UBool fExponentSignAlwaysShown;
  int32_t fCurrencySignCount;
  UBool fGroupingUsed;
  int32_t fGroupingSize;
  int32_t fGroupingSize2;
  int32_t fMultiplier;
  UBool fDecimalSeparatorAlwaysShown;
  int32_t fFormatWidth;
  UBool fRoundingIncrementUsed;
  DigitList fRoundingIncrement;
  UChar32 fPad;
  UBool fNegPatternsBogus;
  UBool fPosPatternsBogus;
  UnicodeString fNegPrefixPattern;
  UnicodeString fNegSuffixPattern;
  UnicodeString fPosPrefixPattern;
  UnicodeString fPosSuffixPattern;
  AffixPattern fNegPrefixAffix;
  AffixPattern fNegSuffixAffix;
  AffixPattern fPosPrefixAffix;
  AffixPattern fPosSuffixAffix;
  EPadPosition fPadPosition;
};

class DecimalFormatPatternParser : public UMemory {
  public:
    DecimalFormatPatternParser();
    void useSymbols(const DecimalFormatSymbols& symbols);

    void applyPatternWithoutExpandAffix(
        const UnicodeString& pattern,
        DecimalFormatPattern& out,
        UParseError& parseError,
        UErrorCode& status);
  private:
    DecimalFormatPatternParser(const DecimalFormatPatternParser&);
    DecimalFormatPatternParser& operator=(DecimalFormatPatternParser& rhs);
    UChar32 fZeroDigit;
    UChar32 fSigDigit;
    UnicodeString fGroupingSeparator;
    UnicodeString fDecimalSeparator;
    UnicodeString fPercent;
    UnicodeString fPerMill;
    UnicodeString fDigit;
    UnicodeString fSeparator;
    UnicodeString fExponent;
    UnicodeString fPlus;
    UnicodeString fMinus;
    UnicodeString fPadEscape;
};

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
#endif
