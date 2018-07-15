// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_PATTERNMODIFIER_H__
#define __NUMBER_PATTERNMODIFIER_H__

#include "standardplural.h"
#include "unicode/numberformatter.h"
#include "number_patternstring.h"
#include "number_types.h"
#include "number_modifiers.h"
#include "number_utils.h"
#include "number_currencysymbols.h"

U_NAMESPACE_BEGIN

// Export an explicit template instantiation of the LocalPointer that is used as a
// data member of ParameterizedModifier.
// (When building DLLs for Windows this is required.)
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN
// Ignore warning 4661 as LocalPointerBase does not use operator== or operator!=
#pragma warning(suppress: 4661)
template class U_I18N_API LocalPointerBase<number::impl::ParameterizedModifier>;
template class U_I18N_API LocalPointer<number::impl::ParameterizedModifier>;
#endif

namespace number {
namespace impl {

// Forward declaration
class MutablePatternModifier;

// Exported as U_I18N_API because it is needed for the unit test PatternModifierTest
class U_I18N_API ImmutablePatternModifier : public MicroPropsGenerator, public UMemory {
  public:
    ~ImmutablePatternModifier() U_OVERRIDE = default;

    void processQuantity(DecimalQuantity&, MicroProps& micros, UErrorCode& status) const U_OVERRIDE;

    void applyToMicros(MicroProps& micros, DecimalQuantity& quantity) const;

    const Modifier* getModifier(int8_t signum, StandardPlural::Form plural) const;

  private:
    ImmutablePatternModifier(ParameterizedModifier* pm, const PluralRules* rules,
                             const MicroPropsGenerator* parent);

    const LocalPointer<ParameterizedModifier> pm;
    const PluralRules* rules;
    const MicroPropsGenerator* parent;

    friend class MutablePatternModifier;
};

/**
 * This class is a {@link Modifier} that wraps a decimal format pattern. It applies the pattern's affixes in
 * {@link Modifier#apply}.
 *
 * <p>
 * In addition to being a Modifier, this class contains the business logic for substituting the correct locale symbols
 * into the affixes of the decimal format pattern.
 *
 * <p>
 * In order to use this class, create a new instance and call the following four setters: {@link #setPatternInfo},
 * {@link #setPatternAttributes}, {@link #setSymbols}, and {@link #setNumberProperties}. After calling these four
 * setters, the instance will be ready for use as a Modifier.
 *
 * <p>
 * This is a MUTABLE, NON-THREAD-SAFE class designed for performance. Do NOT save references to this or attempt to use
 * it from multiple threads! Instead, you can obtain a safe, immutable decimal format pattern modifier by calling
 * {@link MutablePatternModifier#createImmutable}, in effect treating this instance as a builder for the immutable
 * variant.
 */
class U_I18N_API MutablePatternModifier
        : public MicroPropsGenerator,
          public Modifier,
          public SymbolProvider,
          public UMemory {
  public:

    ~MutablePatternModifier() U_OVERRIDE = default;

    /**
     * @param isStrong
     *            Whether the modifier should be considered strong. For more information, see
     *            {@link Modifier#isStrong()}. Most of the time, decimal format pattern modifiers should be considered
     *            as non-strong.
     */
    explicit MutablePatternModifier(bool isStrong);

    /**
     * Sets a reference to the parsed decimal format pattern, usually obtained from
     * {@link PatternStringParser#parseToPatternInfo(String)}, but any implementation of {@link AffixPatternProvider} is
     * accepted.
     */
    void setPatternInfo(const AffixPatternProvider *patternInfo);

    /**
     * Sets attributes that imply changes to the literal interpretation of the pattern string affixes.
     *
     * @param signDisplay
     *            Whether to force a plus sign on positive numbers.
     * @param perMille
     *            Whether to substitute the percent sign in the pattern with a permille sign.
     */
    void setPatternAttributes(UNumberSignDisplay signDisplay, bool perMille);

    /**
     * Sets locale-specific details that affect the symbols substituted into the pattern string affixes.
     *
     * @param symbols
     *            The desired instance of DecimalFormatSymbols.
     * @param currencySymbols
     *            The currency symbols to be used when substituting currency values into the affixes.
     * @param unitWidth
     *            The width used to render currencies.
     * @param rules
     *            Required if the triple currency sign, "¤¤¤", appears in the pattern, which can be determined from the
     *            convenience method {@link #needsPlurals()}.
     */
    void setSymbols(const DecimalFormatSymbols* symbols, const CurrencySymbols* currencySymbols,
                    UNumberUnitWidth unitWidth, const PluralRules* rules);

    /**
     * Sets attributes of the current number being processed.
     *
     * @param signum
     *            -1 if negative; +1 if positive; or 0 if zero.
     * @param plural
     *            The plural form of the number, required only if the pattern contains the triple
     *            currency sign, "¤¤¤" (and as indicated by {@link #needsPlurals()}).
     */
    void setNumberProperties(int8_t signum, StandardPlural::Form plural);

    /**
     * Returns true if the pattern represented by this MurkyModifier requires a plural keyword in order to localize.
     * This is currently true only if there is a currency long name placeholder in the pattern ("¤¤¤").
     */
    bool needsPlurals() const;

    /**
     * Creates a new quantity-dependent Modifier that behaves the same as the current instance, but which is immutable
     * and can be saved for future use. The number properties in the current instance are mutated; all other properties
     * are left untouched.
     *
     * <p>
     * The resulting modifier cannot be used in a QuantityChain.
     *
     * <p>
     * CREATES A NEW HEAP OBJECT; THE CALLER GETS OWNERSHIP.
     *
     * @return An immutable that supports both positive and negative numbers.
     */
    ImmutablePatternModifier *createImmutable(UErrorCode &status);

    /**
     * Creates a new quantity-dependent Modifier that behaves the same as the current instance, but which is immutable
     * and can be saved for future use. The number properties in the current instance are mutated; all other properties
     * are left untouched.
     *
     * <p>
     * CREATES A NEW HEAP OBJECT; THE CALLER GETS OWNERSHIP.
     *
     * @param parent
     *            The QuantityChain to which to chain this immutable.
     * @return An immutable that supports both positive and negative numbers.
     */
    ImmutablePatternModifier *
    createImmutableAndChain(const MicroPropsGenerator *parent, UErrorCode &status);

    MicroPropsGenerator &addToChain(const MicroPropsGenerator *parent);

    void processQuantity(DecimalQuantity &, MicroProps &micros, UErrorCode &status) const U_OVERRIDE;

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE;

    int32_t getPrefixLength(UErrorCode &status) const U_OVERRIDE;

    int32_t getCodePointCount(UErrorCode &status) const U_OVERRIDE;

    bool isStrong() const U_OVERRIDE;

    /**
     * Returns the string that substitutes a given symbol type in a pattern.
     */
    UnicodeString getSymbol(AffixPatternType type) const U_OVERRIDE;

    UnicodeString toUnicodeString() const;

  private:
    // Modifier details (initialized in constructor)
    const bool fStrong;

    // Pattern details (initialized in setPatternInfo and setPatternAttributes)
    const AffixPatternProvider *patternInfo;
    UNumberSignDisplay signDisplay;
    bool perMilleReplacesPercent;

    // Symbol details (initialized in setSymbols)
    const DecimalFormatSymbols *symbols;
    UNumberUnitWidth unitWidth;
    const CurrencySymbols *currencySymbols;
    const PluralRules *rules;

    // Number details (initialized in setNumberProperties)
    int8_t signum;
    StandardPlural::Form plural;

    // QuantityChain details (initialized in addToChain)
    const MicroPropsGenerator *parent;

    // Transient fields for rendering
    UnicodeString currentAffix;

    /**
     * Uses the current properties to create a single {@link ConstantMultiFieldModifier} with currency spacing support
     * if required.
     *
     * <p>
     * CREATES A NEW HEAP OBJECT; THE CALLER GETS OWNERSHIP.
     *
     * @param a
     *            A working NumberStringBuilder object; passed from the outside to prevent the need to create many new
     *            instances if this method is called in a loop.
     * @param b
     *            Another working NumberStringBuilder object.
     * @return The constant modifier object.
     */
    ConstantMultiFieldModifier *createConstantModifier(UErrorCode &status);

    int32_t insertPrefix(NumberStringBuilder &sb, int position, UErrorCode &status);

    int32_t insertSuffix(NumberStringBuilder &sb, int position, UErrorCode &status);

    void prepareAffix(bool isPrefix);
};


}  // namespace impl
}  // namespace number
U_NAMESPACE_END

#endif //__NUMBER_PATTERNMODIFIER_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
