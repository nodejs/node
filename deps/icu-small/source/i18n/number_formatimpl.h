// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_FORMATIMPL_H__
#define __NUMBER_FORMATIMPL_H__

#include "number_types.h"
#include "number_stringbuilder.h"
#include "number_patternstring.h"
#include "number_utils.h"
#include "number_patternmodifier.h"
#include "number_longnames.h"
#include "number_compact.h"
#include "number_microprops.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

/**
 * This is the "brain" of the number formatting pipeline. It ties all the pieces together, taking in a MacroProps and a
 * DecimalQuantity and outputting a properly formatted number string.
 */
class NumberFormatterImpl : public UMemory {
  public:
    /**
     * Builds a "safe" MicroPropsGenerator, which is thread-safe and can be used repeatedly.
     * The caller owns the returned NumberFormatterImpl.
     */
    NumberFormatterImpl(const MacroProps &macros, UErrorCode &status);

    /**
     * Builds and evaluates an "unsafe" MicroPropsGenerator, which is cheaper but can be used only once.
     */
    static int32_t
    formatStatic(const MacroProps &macros, DecimalQuantity &inValue, NumberStringBuilder &outString,
                 UErrorCode &status);

    /**
     * Prints only the prefix and suffix; used for DecimalFormat getters.
     *
     * @return The index into the output at which the prefix ends and the suffix starts; in other words,
     *         the prefix length.
     */
    static int32_t getPrefixSuffixStatic(const MacroProps& macros, int8_t signum,
                                         StandardPlural::Form plural, NumberStringBuilder& outString,
                                         UErrorCode& status);

    /**
     * Evaluates the "safe" MicroPropsGenerator created by "fromMacros".
     */
    int32_t format(DecimalQuantity& inValue, NumberStringBuilder& outString, UErrorCode& status) const;

    /**
     * Like format(), but saves the result into an output MicroProps without additional processing.
     */
    void preProcess(DecimalQuantity& inValue, MicroProps& microsOut, UErrorCode& status) const;

    /**
     * Like getPrefixSuffixStatic() but uses the safe compiled object.
     */
    int32_t getPrefixSuffix(int8_t signum, StandardPlural::Form plural, NumberStringBuilder& outString,
                            UErrorCode& status) const;

    const MicroProps& getRawMicroProps() const {
        return fMicros;
    }

    /**
     * Synthesizes the output string from a MicroProps and DecimalQuantity.
     * This method formats only the main number, not affixes.
     */
    static int32_t writeNumber(const MicroProps& micros, DecimalQuantity& quantity,
                               NumberStringBuilder& string, int32_t index, UErrorCode& status);

    /**
     * Adds the affixes.  Intended to be called immediately after formatNumber.
     */
    static int32_t writeAffixes(const MicroProps& micros, NumberStringBuilder& string, int32_t start,
                                int32_t end, UErrorCode& status);

  private:
    // Head of the MicroPropsGenerator linked list:
    const MicroPropsGenerator *fMicroPropsGenerator = nullptr;

    // Tail of the list:
    MicroProps fMicros;

    // Other fields possibly used by the number formatting pipeline:
    // TODO: Convert more of these LocalPointers to value objects to reduce the number of news?
    LocalPointer<const DecimalFormatSymbols> fSymbols;
    LocalPointer<const PluralRules> fRules;
    LocalPointer<const ParsedPatternInfo> fPatternInfo;
    LocalPointer<const ScientificHandler> fScientificHandler;
    LocalPointer<MutablePatternModifier> fPatternModifier;
    LocalPointer<const ImmutablePatternModifier> fImmutablePatternModifier;
    LocalPointer<const LongNameHandler> fLongNameHandler;
    LocalPointer<const CompactHandler> fCompactHandler;

    // Value objects possibly used by the number formatting pipeline:
    struct Warehouse {
        CurrencySymbols fCurrencySymbols;
    } fWarehouse;


    NumberFormatterImpl(const MacroProps &macros, bool safe, UErrorCode &status);

    MicroProps& preProcessUnsafe(DecimalQuantity &inValue, UErrorCode &status);

    int32_t getPrefixSuffixUnsafe(int8_t signum, StandardPlural::Form plural,
                                  NumberStringBuilder& outString, UErrorCode& status);

    /**
     * If rulesPtr is non-null, return it.  Otherwise, return a PluralRules owned by this object for the
     * specified locale, creating it if necessary.
     */
    const PluralRules *
    resolvePluralRules(const PluralRules *rulesPtr, const Locale &locale, UErrorCode &status);

    /**
     * Synthesizes the MacroProps into a MicroPropsGenerator. All information, including the locale, is encoded into the
     * MicroPropsGenerator, except for the quantity itself, which is left abstract and must be provided to the returned
     * MicroPropsGenerator instance.
     *
     * @see MicroPropsGenerator
     * @param macros
     *            The {@link MacroProps} to consume. This method does not mutate the MacroProps instance.
     * @param safe
     *            If true, the returned MicroPropsGenerator will be thread-safe. If false, the returned value will
     *            <em>not</em> be thread-safe, intended for a single "one-shot" use only. Building the thread-safe
     *            object is more expensive.
     */
    const MicroPropsGenerator *
    macrosToMicroGenerator(const MacroProps &macros, bool safe, UErrorCode &status);

    static int32_t
    writeIntegerDigits(const MicroProps &micros, DecimalQuantity &quantity, NumberStringBuilder &string,
                       int32_t index, UErrorCode &status);

    static int32_t
    writeFractionDigits(const MicroProps &micros, DecimalQuantity &quantity, NumberStringBuilder &string,
                        int32_t index, UErrorCode &status);
};

}  // namespace impl
}  // namespace number
U_NAMESPACE_END


#endif //__NUMBER_FORMATIMPL_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
