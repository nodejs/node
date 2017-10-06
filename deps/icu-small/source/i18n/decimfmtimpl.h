// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File decimfmtimpl.h
********************************************************************************
*/

#ifndef DECIMFMTIMPL_H
#define DECIMFMTIMPL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/decimfmt.h"
#include "unicode/uobject.h"
#include "affixpatternparser.h"
#include "digitaffixesandpadding.h"
#include "digitformatter.h"
#include "digitgrouping.h"
#include "precision.h"

U_NAMESPACE_BEGIN

class UnicodeString;
class FieldPosition;
class ValueFormatter;
class FieldPositionHandler;
class FixedDecimal;

/**
 * DecimalFormatImpl is the glue code between the legacy DecimalFormat class
 * and the new decimal formatting classes. DecimalFormat still handles
 * parsing directly. However, DecimalFormat uses attributes of this class
 * for parsing when possible.
 *
 * The public API of this class closely mirrors the legacy API of the
 * legacy DecimalFormat deviating only when the legacy API does not make
 * sense. For example, although DecimalFormat has a
 * getPadCharacterString() method, DecimalFormatImpl has a getPadCharacter()
 * method because formatting uses only a single pad character for padding.
 *
 * Each legacy DecimalFormat instance heap allocates its own instance of
 * this class. Most DecimalFormat methods that deal with formatting simply
 * delegate to the DecimalFormat's DecimalFormatImpl method.
 *
 * Because DecimalFormat extends NumberFormat, Each instance of this class
 * "borrows" a pointer to the NumberFormat part of its enclosing DecimalFormat
 * instance. This way each DecimalFormatImpl instance can read or even modify
 * the NumberFormat portion of its enclosing DecimalFormat instance.
 *
 * Directed acyclic graph (DAG):
 *
 * This class can be represented as a directed acyclic graph (DAG) where each
 * vertex is an attribute, and each directed edge indicates that the value
 * of the destination attribute is calculated from the value of the source
 * attribute. Attributes with setter methods reside at the bottom of the
 * DAG. That is, no edges point to them. We call these independent attributes
 * because their values can be set independently of one another. The rest of
 * the attributes are derived attributes because their values depend on the
 * independent attributes. DecimalFormatImpl often uses the derived
 * attributes, not the independent attributes, when formatting numbers.
 *
 * The independent attributes at the bottom of the DAG correspond to the legacy
 * attributes of DecimalFormat while the attributes at the top of the DAG
 * correspond to the attributes of the new code. The edges of the DAG
 * correspond to the code that handles the complex interaction among all the
 * legacy attributes of the DecimalFormat API.
 *
 * We use a DAG for three reasons.
 *
 * First, the DAG preserves backward compatibility. Clients of the legacy
 * DecimalFormat expect existing getters and setters of each attribute to be
 * consistent. That means if a client sets a particular attribute to a new
 * value, the attribute should retain that value until the client sets it to
 * a new value. The DAG allows these attributes to remain consistent even
 * though the new code may not use them when formatting.
 *
 * Second, the DAG obviates the need to recalculate derived attributes with
 * each format. Instead, the DAG "remembers" the values of all derived
 * attributes. Only setting an independent attribute requires a recalculation.
 * Moreover, setting an independent attribute recalculates only the affected
 * dependent attributes rather than all dependent attributes.
 *
 * Third, the DAG abstracts away the complex interaction among the legacy
 * attributes of the DecimalFormat API.
 *
 * Only the independent attributes of the DAG have setters and getters.
 * Derived attributes have no setters (and often no getters either).
 *
 * Copy and assign:
 *
 * For copy and assign, DecimalFormatImpl copies and assigns every attribute
 * regardless of whether or not it is independent. We do this for simplicity.
 *
 * Implementation of the DAG:
 *
 * The DAG consists of three smaller DAGs:
 * 1. Grouping attributes
 * 2. Precision attributes
 * 3. Formatting attributes.
 *
 * The first two DAGs are simple in that setting any independent attribute
 * in the DAG recalculates all the dependent attributes in that DAG.
 * The updateGrouping() and updatePrecision() perform the respective
 * recalculations.
 *
 * Because some of the derived formatting attributes are expensive to
 * calculate, the formatting attributes DAG is more complex. The
 * updateFormatting() method is composed of many updateFormattingXXX()
 * methods, each of which recalculates a single derived attribute. The
 * updateFormatting() method accepts a bitfield of recently changed
 * attributes and passes this bitfield by reference to each of the
 * updateFormattingXXX() methods. Each updateFormattingXXX() method checks
 * the bitfield to see if any of the attributes it uses to compute the XXX
 * attribute changed. If none of them changed, it exists immediately. However,
 * if at least one of them changed, it recalculates the XXX attribute and
 * sets the corresponding bit in the bitfield. In this way, each
 * updateFormattingXXX() method encodes the directed edges in the formatting
 * DAG that point to the attribute its calculating.
 *
 * Maintenance of the updateFormatting() method.
 *
 * Use care when changing the updateFormatting() method.
 * The updateFormatting() method must call each updateFormattingXXX() in the
 * same partial order that the formatting DAG prescribes. That is, the
 * attributes near the bottom of the DAG must be calculated before attributes
 * further up. As we mentioned in the prvious paragraph, the directed edges of
 * the formatting DAG are encoded within each updateFormattingXXX() method.
 * Finally, adding new attributes may involve adding to the bitmap that the
 * updateFormatting() method uses. The top most attributes in the DAG,
 * those that do not point to any attributes but only have attributes
 * pointing to it, need not have a slot in the bitmap.
 *
 * Keep in mind that most of the code that makes the legacy DecimalFormat API
 * work the way it always has before can be found in these various updateXXX()
 * methods. For example the updatePrecisionForScientific() method
 * handles the complex interactions amoung the various precision attributes
 * when formatting in scientific notation. Changing the way attributes
 * interract, often means changing one of these updateXXX() methods.
 *
 * Conclusion:
 *
 * The DecimFmtImpl class is the glue code between the legacy and new
 * number formatting code. It uses a direct acyclic graph (DAG) to
 * maintain backward compatibility, to make the code efficient, and to
 * abstract away the complex interraction among legacy attributs.
 */


class DecimalFormatImpl : public UObject {
public:

DecimalFormatImpl(
        NumberFormat *super,
        const Locale &locale,
        const UnicodeString &pattern,
        UErrorCode &status);
DecimalFormatImpl(
        NumberFormat *super,
        const UnicodeString &pattern,
        DecimalFormatSymbols *symbolsToAdopt,
        UParseError &parseError,
        UErrorCode &status);
DecimalFormatImpl(
        NumberFormat *super,
        const DecimalFormatImpl &other,
        UErrorCode &status);
DecimalFormatImpl &assign(
        const DecimalFormatImpl &other, UErrorCode &status);
virtual ~DecimalFormatImpl();
void adoptDecimalFormatSymbols(DecimalFormatSymbols *symbolsToAdopt);
const DecimalFormatSymbols &getDecimalFormatSymbols() const {
    return *fSymbols;
}
UnicodeString &format(
        int32_t number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const;
UnicodeString &format(
        int32_t number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const;
UnicodeString &format(
        int64_t number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const;
UnicodeString &format(
        double number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const;
UnicodeString &format(
        const DigitList &number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const;
UnicodeString &format(
        int64_t number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const;
UnicodeString &format(
        double number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const;
UnicodeString &format(
        const DigitList &number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const;
UnicodeString &format(
        StringPiece number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const;
UnicodeString &format(
        const VisibleDigitsWithExponent &digits,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const;
UnicodeString &format(
        const VisibleDigitsWithExponent &digits,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const;

UBool operator==(const DecimalFormatImpl &) const;

UBool operator!=(const DecimalFormatImpl &other) const {
    return !(*this == other);
}

void setRoundingMode(DecimalFormat::ERoundingMode mode) {
    fRoundingMode = mode;
    fEffPrecision.fMantissa.fExactOnly = (fRoundingMode == DecimalFormat::kRoundUnnecessary);
    fEffPrecision.fMantissa.fRoundingMode = mode;
}
DecimalFormat::ERoundingMode getRoundingMode() const {
    return fRoundingMode;
}
void setFailIfMoreThanMaxDigits(UBool b) {
    fEffPrecision.fMantissa.fFailIfOverMax = b;
}
UBool isFailIfMoreThanMaxDigits() const { return fEffPrecision.fMantissa.fFailIfOverMax; }
void setMinimumSignificantDigits(int32_t newValue);
void setMaximumSignificantDigits(int32_t newValue);
void setMinMaxSignificantDigits(int32_t min, int32_t max);
void setScientificNotation(UBool newValue);
void setSignificantDigitsUsed(UBool newValue);

int32_t getMinimumSignificantDigits() const {
        return fMinSigDigits; }
int32_t getMaximumSignificantDigits() const {
        return fMaxSigDigits; }
UBool isScientificNotation() const { return fUseScientific; }
UBool areSignificantDigitsUsed() const { return fUseSigDigits; }
void setGroupingSize(int32_t newValue);
void setSecondaryGroupingSize(int32_t newValue);
void setMinimumGroupingDigits(int32_t newValue);
int32_t getGroupingSize() const { return fGrouping.fGrouping; }
int32_t getSecondaryGroupingSize() const { return fGrouping.fGrouping2; }
int32_t getMinimumGroupingDigits() const { return fGrouping.fMinGrouping; }
void applyPattern(const UnicodeString &pattern, UErrorCode &status);
void applyPatternFavorCurrencyPrecision(
        const UnicodeString &pattern, UErrorCode &status);
void applyPattern(
        const UnicodeString &pattern, UParseError &perror, UErrorCode &status);
void applyLocalizedPattern(const UnicodeString &pattern, UErrorCode &status);
void applyLocalizedPattern(
        const UnicodeString &pattern, UParseError &perror, UErrorCode &status);
void setCurrencyUsage(UCurrencyUsage usage, UErrorCode &status);
UCurrencyUsage getCurrencyUsage() const { return fCurrencyUsage; }
void setRoundingIncrement(double d);
double getRoundingIncrement() const;
int32_t getMultiplier() const;
void setMultiplier(int32_t m);
UChar32 getPadCharacter() const { return fAffixes.fPadChar; }
void setPadCharacter(UChar32 c) { fAffixes.fPadChar = c; }
int32_t getFormatWidth() const { return fAffixes.fWidth; }
void setFormatWidth(int32_t x) { fAffixes.fWidth = x; }
DigitAffixesAndPadding::EPadPosition getPadPosition() const {
    return fAffixes.fPadPosition;
}
void setPadPosition(DigitAffixesAndPadding::EPadPosition x) {
    fAffixes.fPadPosition = x;
}
int32_t getMinimumExponentDigits() const {
    return fEffPrecision.fMinExponentDigits;
}
void setMinimumExponentDigits(int32_t x) {
    fEffPrecision.fMinExponentDigits = x;
}
UBool isExponentSignAlwaysShown() const {
    return fOptions.fExponent.fAlwaysShowSign;
}
void setExponentSignAlwaysShown(UBool x) {
    fOptions.fExponent.fAlwaysShowSign = x;
}
UBool isDecimalSeparatorAlwaysShown() const {
    return fOptions.fMantissa.fAlwaysShowDecimal;
}
void setDecimalSeparatorAlwaysShown(UBool x) {
    fOptions.fMantissa.fAlwaysShowDecimal = x;
}
UnicodeString &getPositivePrefix(UnicodeString &result) const;
UnicodeString &getPositiveSuffix(UnicodeString &result) const;
UnicodeString &getNegativePrefix(UnicodeString &result) const;
UnicodeString &getNegativeSuffix(UnicodeString &result) const;
void setPositivePrefix(const UnicodeString &str);
void setPositiveSuffix(const UnicodeString &str);
void setNegativePrefix(const UnicodeString &str);
void setNegativeSuffix(const UnicodeString &str);
UnicodeString &toPattern(UnicodeString& result) const;
FixedDecimal &getFixedDecimal(double value, FixedDecimal &result, UErrorCode &status) const;
FixedDecimal &getFixedDecimal(DigitList &number, FixedDecimal &result, UErrorCode &status) const;
DigitList &round(DigitList &number, UErrorCode &status) const;

VisibleDigitsWithExponent &
initVisibleDigitsWithExponent(
        int64_t number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const;
VisibleDigitsWithExponent &
initVisibleDigitsWithExponent(
        double number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const;
VisibleDigitsWithExponent &
initVisibleDigitsWithExponent(
        DigitList &number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const;

void updatePrecision();
void updateGrouping();
void updateCurrency(UErrorCode &status);


private:
// Disallow copy and assign
DecimalFormatImpl(const DecimalFormatImpl &other);
DecimalFormatImpl &operator=(const DecimalFormatImpl &other);
NumberFormat *fSuper;
DigitList fMultiplier;
int32_t fScale;

DecimalFormat::ERoundingMode fRoundingMode;

// These fields include what the user can see and set.
// When the user updates these fields, it triggers automatic updates of
// other fields that may be invisible to user

// Updating any of the following fields triggers an update to
// fEffPrecision.fMantissa.fMin,
// fEffPrecision.fMantissa.fMax,
// fEffPrecision.fMantissa.fSignificant fields
// We have this two phase update because of backward compatibility.
// DecimalFormat has to remember all settings even if those settings are
// invalid or disabled.
int32_t fMinSigDigits;
int32_t fMaxSigDigits;
UBool fUseScientific;
UBool fUseSigDigits;
// In addition to these listed above, changes to min/max int digits and
// min/max frac digits from fSuper also trigger an update.

// Updating any of the following fields triggers an update to
// fEffGrouping field Again we do it this way because original
// grouping settings have to be retained if grouping is turned off.
DigitGrouping fGrouping;
// In addition to these listed above, changes to isGroupingUsed in
// fSuper also triggers an update to fEffGrouping.

// Updating any of the following fields triggers updates on the following:
// fMonetary, fRules, fAffixParser, fCurrencyAffixInfo,
// fFormatter, fAffixes.fPositivePrefiix, fAffixes.fPositiveSuffix,
// fAffixes.fNegativePrefiix, fAffixes.fNegativeSuffix
// We do this two phase update because localizing the affix patterns
// and formatters can be expensive. Better to do it once with the setters
// than each time within format.
AffixPattern fPositivePrefixPattern;
AffixPattern fNegativePrefixPattern;
AffixPattern fPositiveSuffixPattern;
AffixPattern fNegativeSuffixPattern;
DecimalFormatSymbols *fSymbols;
UCurrencyUsage fCurrencyUsage;
// In addition to these listed above, changes to getCurrency() in
// fSuper also triggers an update.

// Optional may be NULL
PluralRules *fRules;

// These fields are totally hidden from user and are used to derive the affixes
// in fAffixes below from the four affix patterns above.
UBool fMonetary;
AffixPatternParser fAffixParser;
CurrencyAffixInfo fCurrencyAffixInfo;

// The actual precision used when formatting
ScientificPrecision fEffPrecision;

// The actual grouping used when formatting
DigitGrouping fEffGrouping;
SciFormatterOptions fOptions;   // Encapsulates fixed precision options
DigitFormatter fFormatter;
DigitAffixesAndPadding fAffixes;

UnicodeString &formatInt32(
        int32_t number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

UnicodeString &formatInt64(
        int64_t number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

UnicodeString &formatDouble(
        double number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

// Scales for precent or permille symbols
UnicodeString &formatDigitList(
        DigitList &number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

// Does not scale for precent or permille symbols
UnicodeString &formatAdjustedDigitList(
        DigitList &number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

UnicodeString &formatVisibleDigitsWithExponent(
        const VisibleDigitsWithExponent &number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

VisibleDigitsWithExponent &
initVisibleDigitsFromAdjusted(
        DigitList &number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const;

template<class T>
UBool maybeFormatWithDigitList(
        T number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const;

template<class T>
UBool maybeInitVisibleDigitsFromDigitList(
        T number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const;

DigitList &adjustDigitList(DigitList &number, UErrorCode &status) const;

void applyPattern(
        const UnicodeString &pattern,
        UBool localized, UParseError &perror, UErrorCode &status);

ValueFormatter &prepareValueFormatter(ValueFormatter &vf) const;
void setMultiplierScale(int32_t s);
int32_t getPatternScale() const;
void setScale(int32_t s) { fScale = s; }
int32_t getScale() const { return fScale; }

// Updates everything
void updateAll(UErrorCode &status);
void updateAll(
        int32_t formattingFlags,
        UBool updatePrecisionBasedOnCurrency,
        UErrorCode &status);

// Updates from formatting pattern changes
void updateForApplyPattern(UErrorCode &status);
void updateForApplyPatternFavorCurrencyPrecision(UErrorCode &status);

// Updates from changes to third group of attributes
void updateFormatting(int32_t changedFormattingFields, UErrorCode &status);
void updateFormatting(
        int32_t changedFormattingFields,
        UBool updatePrecisionBasedOnCurrency,
        UErrorCode &status);

// Helper functions for updatePrecision
void updatePrecisionForScientific();
void updatePrecisionForFixed();
void extractMinMaxDigits(DigitInterval &min, DigitInterval &max) const;
void extractSigDigits(SignificantDigitInterval &sig) const;

// Helper functions for updateFormatting
void updateFormattingUsesCurrency(int32_t &changedFormattingFields);
void updateFormattingPluralRules(
        int32_t &changedFormattingFields, UErrorCode &status);
void updateFormattingAffixParser(int32_t &changedFormattingFields);
void updateFormattingCurrencyAffixInfo(
        int32_t &changedFormattingFields,
        UBool updatePrecisionBasedOnCurrency,
        UErrorCode &status);
void updateFormattingFixedPointFormatter(
        int32_t &changedFormattingFields);
void updateFormattingLocalizedPositivePrefix(
        int32_t &changedFormattingFields, UErrorCode &status);
void updateFormattingLocalizedPositiveSuffix(
        int32_t &changedFormattingFields, UErrorCode &status);
void updateFormattingLocalizedNegativePrefix(
        int32_t &changedFormattingFields, UErrorCode &status);
void updateFormattingLocalizedNegativeSuffix(
        int32_t &changedFormattingFields, UErrorCode &status);

int32_t computeExponentPatternLength() const;
int32_t countFractionDigitAndDecimalPatternLength(int32_t fracDigitCount) const;
UnicodeString &toNumberPattern(
        UBool hasPadding, int32_t minimumLength, UnicodeString& result) const;

int32_t getOldFormatWidth() const;
const UnicodeString &getConstSymbol(
        DecimalFormatSymbols::ENumberFormatSymbol symbol) const;
UBool isParseFastpath() const;

friend class DecimalFormat;

};


U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif // DECIMFMTIMPL_H
//eof
