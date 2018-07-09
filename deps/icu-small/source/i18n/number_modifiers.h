// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_MODIFIERS_H__
#define __NUMBER_MODIFIERS_H__

#include <algorithm>
#include <cstdint>
#include "unicode/uniset.h"
#include "unicode/simpleformatter.h"
#include "standardplural.h"
#include "number_stringbuilder.h"
#include "number_types.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

/**
 * The canonical implementation of {@link Modifier}, containing a prefix and suffix string.
 * TODO: This is not currently being used by real code and could be removed.
 */
class U_I18N_API ConstantAffixModifier : public Modifier, public UObject {
  public:
    ConstantAffixModifier(const UnicodeString &prefix, const UnicodeString &suffix, Field field,
                          bool strong)
            : fPrefix(prefix), fSuffix(suffix), fField(field), fStrong(strong) {}

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE;

    int32_t getPrefixLength(UErrorCode &status) const U_OVERRIDE;

    int32_t getCodePointCount(UErrorCode &status) const U_OVERRIDE;

    bool isStrong() const U_OVERRIDE;

  private:
    UnicodeString fPrefix;
    UnicodeString fSuffix;
    Field fField;
    bool fStrong;
};

/**
 * The second primary implementation of {@link Modifier}, this one consuming a {@link SimpleFormatter}
 * pattern.
 */
class U_I18N_API SimpleModifier : public Modifier, public UMemory {
  public:
    SimpleModifier(const SimpleFormatter &simpleFormatter, Field field, bool strong);

    // Default constructor for LongNameHandler.h
    SimpleModifier();

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE;

    int32_t getPrefixLength(UErrorCode &status) const U_OVERRIDE;

    int32_t getCodePointCount(UErrorCode &status) const U_OVERRIDE;

    bool isStrong() const U_OVERRIDE;

    /**
     * TODO: This belongs in SimpleFormatterImpl. The only reason I haven't moved it there yet is because
     * DoubleSidedStringBuilder is an internal class and SimpleFormatterImpl feels like it should not depend on it.
     *
     * <p>
     * Formats a value that is already stored inside the StringBuilder <code>result</code> between the indices
     * <code>startIndex</code> and <code>endIndex</code> by inserting characters before the start index and after the
     * end index.
     *
     * <p>
     * This is well-defined only for patterns with exactly one argument.
     *
     * @param result
     *            The StringBuilder containing the value argument.
     * @param startIndex
     *            The left index of the value within the string builder.
     * @param endIndex
     *            The right index of the value within the string builder.
     * @return The number of characters (UTF-16 code points) that were added to the StringBuilder.
     */
    int32_t
    formatAsPrefixSuffix(NumberStringBuilder &result, int32_t startIndex, int32_t endIndex, Field field,
                         UErrorCode &status) const;

  private:
    UnicodeString fCompiledPattern;
    Field fField;
    bool fStrong;
    int32_t fPrefixLength;
    int32_t fSuffixOffset;
    int32_t fSuffixLength;
};

/**
 * An implementation of {@link Modifier} that allows for multiple types of fields in the same modifier. Constructed
 * based on the contents of two {@link NumberStringBuilder} instances (one for the prefix, one for the suffix).
 */
class U_I18N_API ConstantMultiFieldModifier : public Modifier, public UMemory {
  public:
    ConstantMultiFieldModifier(
            const NumberStringBuilder &prefix,
            const NumberStringBuilder &suffix,
            bool overwrite,
            bool strong)
      : fPrefix(prefix),
        fSuffix(suffix),
        fOverwrite(overwrite),
        fStrong(strong) {}

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE;

    int32_t getPrefixLength(UErrorCode &status) const U_OVERRIDE;

    int32_t getCodePointCount(UErrorCode &status) const U_OVERRIDE;

    bool isStrong() const U_OVERRIDE;

  protected:
    // NOTE: In Java, these are stored as array pointers. In C++, the NumberStringBuilder is stored by
    // value and is treated internally as immutable.
    NumberStringBuilder fPrefix;
    NumberStringBuilder fSuffix;
    bool fOverwrite;
    bool fStrong;
};

/** Identical to {@link ConstantMultiFieldModifier}, but supports currency spacing. */
class U_I18N_API CurrencySpacingEnabledModifier : public ConstantMultiFieldModifier {
  public:
    /** Safe code path */
    CurrencySpacingEnabledModifier(
            const NumberStringBuilder &prefix,
            const NumberStringBuilder &suffix,
            bool overwrite,
            bool strong,
            const DecimalFormatSymbols &symbols,
            UErrorCode &status);

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE;

    /** Unsafe code path */
    static int32_t
    applyCurrencySpacing(NumberStringBuilder &output, int32_t prefixStart, int32_t prefixLen,
                         int32_t suffixStart, int32_t suffixLen, const DecimalFormatSymbols &symbols,
                         UErrorCode &status);

  private:
    UnicodeSet fAfterPrefixUnicodeSet;
    UnicodeString fAfterPrefixInsert;
    UnicodeSet fBeforeSuffixUnicodeSet;
    UnicodeString fBeforeSuffixInsert;

    enum EAffix {
        PREFIX, SUFFIX
    };

    enum EPosition {
        IN_CURRENCY, IN_NUMBER
    };

    /** Unsafe code path */
    static int32_t applyCurrencySpacingAffix(NumberStringBuilder &output, int32_t index, EAffix affix,
                                             const DecimalFormatSymbols &symbols, UErrorCode &status);

    static UnicodeSet
    getUnicodeSet(const DecimalFormatSymbols &symbols, EPosition position, EAffix affix,
                  UErrorCode &status);

    static UnicodeString
    getInsertString(const DecimalFormatSymbols &symbols, EAffix affix, UErrorCode &status);
};

/** A Modifier that does not do anything. */
class U_I18N_API EmptyModifier : public Modifier, public UMemory {
  public:
    explicit EmptyModifier(bool isStrong) : fStrong(isStrong) {}

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE {
        (void)output;
        (void)leftIndex;
        (void)rightIndex;
        (void)status;
        return 0;
    }

    int32_t getPrefixLength(UErrorCode &status) const U_OVERRIDE {
        (void)status;
        return 0;
    }

    int32_t getCodePointCount(UErrorCode &status) const U_OVERRIDE {
        (void)status;
        return 0;
    }

    bool isStrong() const U_OVERRIDE {
        return fStrong;
    }

  private:
    bool fStrong;
};

/**
 * A ParameterizedModifier by itself is NOT a Modifier. Rather, it wraps a data structure containing two or more
 * Modifiers and returns the modifier appropriate for the current situation.
 */
class U_I18N_API ParameterizedModifier : public UMemory {
  public:
    // NOTE: mods is zero-initialized (to nullptr)
    ParameterizedModifier() : mods() {
    }

    // No copying!
    ParameterizedModifier(const ParameterizedModifier &other) = delete;

    ~ParameterizedModifier() {
        for (const Modifier *mod : mods) {
            delete mod;
        }
    }

    void adoptPositiveNegativeModifiers(
            const Modifier *positive, const Modifier *zero, const Modifier *negative) {
        mods[2] = positive;
        mods[1] = zero;
        mods[0] = negative;
    }

    /** The modifier is ADOPTED. */
    void adoptSignPluralModifier(int8_t signum, StandardPlural::Form plural, const Modifier *mod) {
        mods[getModIndex(signum, plural)] = mod;
    }

    /** Returns a reference to the modifier; no ownership change. */
    const Modifier *getModifier(int8_t signum) const {
        return mods[signum + 1];
    }

    /** Returns a reference to the modifier; no ownership change. */
    const Modifier *getModifier(int8_t signum, StandardPlural::Form plural) const {
        return mods[getModIndex(signum, plural)];
    }

  private:
    const Modifier *mods[3 * StandardPlural::COUNT];

    inline static int32_t getModIndex(int8_t signum, StandardPlural::Form plural) {
        return static_cast<int32_t>(plural) * 3 + (signum + 1);
    }
};

} // namespace impl
} // namespace number
U_NAMESPACE_END


#endif //__NUMBER_MODIFIERS_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
