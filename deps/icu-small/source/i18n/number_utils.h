// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT
#ifndef __NUMBER_UTILS_H__
#define __NUMBER_UTILS_H__

#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"
#include "number_scientific.h"
#include "number_patternstring.h"
#include "number_modifiers.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

class UnicodeStringCharSequence : public CharSequence {
  public:
    explicit UnicodeStringCharSequence(const UnicodeString &other) {
        fStr = other;
    }

    ~UnicodeStringCharSequence() U_OVERRIDE = default;

    int32_t length() const U_OVERRIDE {
        return fStr.length();
    }

    char16_t charAt(int32_t index) const U_OVERRIDE {
        return fStr.charAt(index);
    }

    UChar32 codePointAt(int32_t index) const U_OVERRIDE {
        return fStr.char32At(index);
    }

    UnicodeString toUnicodeString() const U_OVERRIDE {
        // Allocate a UnicodeString of the correct length
        UnicodeString output(length(), 0, -1);
        for (int32_t i = 0; i < length(); i++) {
            output.append(charAt(i));
        }
        return output;
    }

  private:
    UnicodeString fStr;
};

struct MicroProps : public MicroPropsGenerator {

    // NOTE: All of these fields are properly initialized in NumberFormatterImpl.
    Rounder rounding;
    Grouper grouping;
    Padder padding;
    IntegerWidth integerWidth;
    UNumberSignDisplay sign;
    UNumberDecimalSeparatorDisplay decimal;
    bool useCurrency;

    // Note: This struct has no direct ownership of the following pointers.
    const DecimalFormatSymbols *symbols;
    const Modifier *modOuter;
    const Modifier *modMiddle;
    const Modifier *modInner;

    // The following "helper" fields may optionally be used during the MicroPropsGenerator.
    // They live here to retain memory.
    struct {
        ScientificModifier scientificModifier;
        EmptyModifier emptyWeakModifier{false};
        EmptyModifier emptyStrongModifier{true};
    } helpers;


    MicroProps() = default;

    MicroProps(const MicroProps &other) = default;

    MicroProps &operator=(const MicroProps &other) = default;

    void processQuantity(DecimalQuantity &, MicroProps &micros, UErrorCode &status) const U_OVERRIDE {
        (void)status;
        if (this == &micros) {
            // Unsafe path: no need to perform a copy.
            U_ASSERT(!exhausted);
            micros.exhausted = true;
            U_ASSERT(exhausted);
        } else {
            // Safe path: copy self into the output micros.
            micros = *this;
        }
    }

  private:
    // Internal fields:
    bool exhausted = false;
};

/**
 * This struct provides the result of the number formatting pipeline to FormattedNumber.
 *
 * The DecimalQuantity is not currently being used by FormattedNumber, but at some point it could be used
 * to add a toDecNumber() or similar method.
 */
struct NumberFormatterResults : public UMemory {
    DecimalQuantity quantity;
    NumberStringBuilder string;
};

inline const UnicodeString getDigitFromSymbols(int8_t digit, const DecimalFormatSymbols &symbols) {
    // TODO: Implement DecimalFormatSymbols.getCodePointZero()?
    if (digit == 0) {
        return symbols.getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kZeroDigitSymbol);
    } else {
        return symbols.getSymbol(static_cast<DecimalFormatSymbols::ENumberFormatSymbol>(
                                         DecimalFormatSymbols::ENumberFormatSymbol::kOneDigitSymbol + digit - 1));
    }
}

} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__NUMBER_UTILS_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
