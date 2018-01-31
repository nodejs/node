// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT
#ifndef __NUMBER_TYPES_H__
#define __NUMBER_TYPES_H__

#include <cstdint>
#include "unicode/decimfmt.h"
#include "unicode/unum.h"
#include "unicode/numsys.h"
#include "unicode/numberformatter.h"
#include "unicode/utf16.h"
#include "uassert.h"
#include "unicode/platform.h"

U_NAMESPACE_BEGIN
namespace number {
namespace impl {

// Typedef several enums for brevity and for easier comparison to Java.

typedef UNumberFormatFields Field;

typedef UNumberFormatRoundingMode RoundingMode;

typedef UNumberFormatPadPosition PadPosition;

typedef UNumberCompactStyle CompactStyle;

// ICU4J Equivalent: RoundingUtils.MAX_INT_FRAC_SIG
static constexpr int32_t kMaxIntFracSig = 100;

// ICU4J Equivalent: RoundingUtils.DEFAULT_ROUNDING_MODE
static constexpr RoundingMode kDefaultMode = RoundingMode::UNUM_FOUND_HALFEVEN;

// ICU4J Equivalent: Padder.FALLBACK_PADDING_STRING
static constexpr char16_t kFallbackPaddingString[] = u" ";

// ICU4J Equivalent: NumberFormatterImpl.DEFAULT_CURRENCY
static constexpr char16_t kDefaultCurrency[] = u"XXX";

// FIXME: New error codes:
static constexpr UErrorCode U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR = U_ILLEGAL_ARGUMENT_ERROR;
static constexpr UErrorCode U_NUMBER_PADDING_WIDTH_OUTOFBOUNDS_ERROR = U_ILLEGAL_ARGUMENT_ERROR;

// Forward declarations:

class Modifier;
class MutablePatternModifier;
class DecimalQuantity;
class NumberStringBuilder;
struct MicroProps;


enum AffixPatternType {
    // Represents a literal character; the value is stored in the code point field.
            TYPE_CODEPOINT = 0,

    // Represents a minus sign symbol '-'.
            TYPE_MINUS_SIGN = -1,

    // Represents a plus sign symbol '+'.
            TYPE_PLUS_SIGN = -2,

    // Represents a percent sign symbol '%'.
            TYPE_PERCENT = -3,

    // Represents a permille sign symbol '‰'.
            TYPE_PERMILLE = -4,

    // Represents a single currency symbol '¤'.
            TYPE_CURRENCY_SINGLE = -5,

    // Represents a double currency symbol '¤¤'.
            TYPE_CURRENCY_DOUBLE = -6,

    // Represents a triple currency symbol '¤¤¤'.
            TYPE_CURRENCY_TRIPLE = -7,

    // Represents a quadruple currency symbol '¤¤¤¤'.
            TYPE_CURRENCY_QUAD = -8,

    // Represents a quintuple currency symbol '¤¤¤¤¤'.
            TYPE_CURRENCY_QUINT = -9,

    // Represents a sequence of six or more currency symbols.
            TYPE_CURRENCY_OVERFLOW = -15
};

enum CompactType {
    TYPE_DECIMAL,
    TYPE_CURRENCY
};


// TODO: Should this be moved somewhere else, maybe where other ICU classes can use it?
// Exported as U_I18N_API because it is a base class for other exported types
class U_I18N_API CharSequence {
public:
    virtual ~CharSequence() = default;

    virtual int32_t length() const = 0;

    virtual char16_t charAt(int32_t index) const = 0;

    virtual UChar32 codePointAt(int32_t index) const {
        // Default implementation; can be overridden with a more efficient version
        char16_t leading = charAt(index);
        if (U16_IS_LEAD(leading) && length() > index + 1) {
            char16_t trailing = charAt(index + 1);
            return U16_GET_SUPPLEMENTARY(leading, trailing);
        } else {
            return leading;
        }
    }

    virtual UnicodeString toUnicodeString() const = 0;
};

class U_I18N_API AffixPatternProvider {
  public:
    static const int32_t AFFIX_PLURAL_MASK = 0xff;
    static const int32_t AFFIX_PREFIX = 0x100;
    static const int32_t AFFIX_NEGATIVE_SUBPATTERN = 0x200;
    static const int32_t AFFIX_PADDING = 0x400;

    virtual ~AffixPatternProvider() = default;

    virtual char16_t charAt(int flags, int i) const = 0;

    virtual int length(int flags) const = 0;

    virtual bool hasCurrencySign() const = 0;

    virtual bool positiveHasPlusSign() const = 0;

    virtual bool hasNegativeSubpattern() const = 0;

    virtual bool negativeHasMinusSign() const = 0;

    virtual bool containsSymbolType(AffixPatternType, UErrorCode &) const = 0;
};

/**
 * A Modifier is an object that can be passed through the formatting pipeline until it is finally applied to the string
 * builder. A Modifier usually contains a prefix and a suffix that are applied, but it could contain something else,
 * like a {@link com.ibm.icu.text.SimpleFormatter} pattern.
 *
 * A Modifier is usually immutable, except in cases such as {@link MurkyModifier}, which are mutable for performance
 * reasons.
 *
 * Exported as U_I18N_API because it is a base class for other exported types
 */
class U_I18N_API Modifier {
  public:
    virtual ~Modifier() = default;

    /**
     * Apply this Modifier to the string builder.
     *
     * @param output
     *            The string builder to which to apply this modifier.
     * @param leftIndex
     *            The left index of the string within the builder. Equal to 0 when only one number is being formatted.
     * @param rightIndex
     *            The right index of the string within the string builder. Equal to length when only one number is being
     *            formatted.
     * @return The number of characters (UTF-16 code units) that were added to the string builder.
     */
    virtual int32_t
    apply(NumberStringBuilder &output, int leftIndex, int rightIndex, UErrorCode &status) const = 0;

    /**
     * Gets the length of the prefix. This information can be used in combination with {@link #apply} to extract the
     * prefix and suffix strings.
     *
     * @return The number of characters (UTF-16 code units) in the prefix.
     */
    virtual int32_t getPrefixLength(UErrorCode& status) const = 0;

    /**
     * Returns the number of code points in the modifier, prefix plus suffix.
     */
    virtual int32_t getCodePointCount(UErrorCode &status) const = 0;

    /**
     * Whether this modifier is strong. If a modifier is strong, it should always be applied immediately and not allowed
     * to bubble up. With regard to padding, strong modifiers are considered to be on the inside of the prefix and
     * suffix.
     *
     * @return Whether the modifier is strong.
     */
    virtual bool isStrong() const = 0;
};

/**
 * This interface is used when all number formatting settings, including the locale, are known, except for the quantity
 * itself. The {@link #processQuantity} method performs the final step in the number processing pipeline: it uses the
 * quantity to generate a finalized {@link MicroProps}, which can be used to render the number to output.
 *
 * <p>
 * In other words, this interface is used for the parts of number processing that are <em>quantity-dependent</em>.
 *
 * <p>
 * In order to allow for multiple different objects to all mutate the same MicroProps, a "chain" of MicroPropsGenerators
 * are linked together, and each one is responsible for manipulating a certain quantity-dependent part of the
 * MicroProps. At the tail of the linked list is a base instance of {@link MicroProps} with properties that are not
 * quantity-dependent. Each element in the linked list calls {@link #processQuantity} on its "parent", then does its
 * work, and then returns the result.
 *
 * Exported as U_I18N_API because it is a base class for other exported types
 *
 */
class U_I18N_API MicroPropsGenerator {
  public:
    virtual ~MicroPropsGenerator() = default;

    /**
     * Considers the given {@link DecimalQuantity}, optionally mutates it, and returns a {@link MicroProps}.
     *
     * @param quantity
     *            The quantity for consideration and optional mutation.
     * @param micros
     *            The MicroProps instance to populate.
     * @return A MicroProps instance resolved for the quantity.
     */
    virtual void processQuantity(DecimalQuantity& quantity, MicroProps& micros, UErrorCode& status) const = 0;
};

class MultiplierProducer {
  public:
    virtual ~MultiplierProducer() = default;

    virtual int32_t getMultiplier(int32_t magnitude) const = 0;
};

// Exported as U_I18N_API because it is a public member field of exported DecimalFormatProperties
template<typename T>
class U_I18N_API NullableValue {
  public:
    NullableValue() : fNull(true) {}

    NullableValue(const NullableValue<T> &other) = default;

    explicit NullableValue(const T &other) {
        fValue = other;
        fNull = false;
    }

    NullableValue<T> &operator=(const NullableValue<T> &other) = default;

    NullableValue<T> &operator=(const T &other) {
        fValue = other;
        fNull = false;
        return *this;
    }

    bool operator==(const NullableValue &other) const {
        // "fValue == other.fValue" returns UBool, not bool (causes compiler warnings)
        return fNull ? other.fNull : (other.fNull ? false : static_cast<bool>(fValue == other.fValue));
    }

    void nullify() {
        // TODO: It might be nice to call the destructor here.
        fNull = true;
    }

    bool isNull() const {
        return fNull;
    }

    T get(UErrorCode &status) const {
        if (fNull) {
            status = U_UNDEFINED_VARIABLE;
        }
        return fValue;
    }

  private:
    bool fNull;
    T fValue;
};

} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__NUMBER_TYPES_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
