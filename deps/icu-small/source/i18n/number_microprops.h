// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_MICROPROPS_H__
#define __NUMBER_MICROPROPS_H__

// TODO: minimize includes
#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"
#include "number_scientific.h"
#include "number_patternstring.h"
#include "number_modifiers.h"
#include "number_multiplier.h"
#include "number_roundingutils.h"
#include "decNumber.h"
#include "charstr.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

struct MicroProps : public MicroPropsGenerator {

    // NOTE: All of these fields are properly initialized in NumberFormatterImpl.
    RoundingImpl rounder;
    Grouper grouping;
    Padder padding;
    IntegerWidth integerWidth;
    UNumberSignDisplay sign;
    UNumberDecimalSeparatorDisplay decimal;
    bool useCurrency;
    char nsName[9];

    // Note: This struct has no direct ownership of the following pointers.
    const DecimalFormatSymbols* symbols;
    const Modifier* modOuter;
    const Modifier* modMiddle = nullptr;
    const Modifier* modInner;

    // The following "helper" fields may optionally be used during the MicroPropsGenerator.
    // They live here to retain memory.
    struct {
        ScientificModifier scientificModifier;
        EmptyModifier emptyWeakModifier{false};
        EmptyModifier emptyStrongModifier{true};
        MultiplierFormatHandler multiplier;
    } helpers;


    MicroProps() = default;

    MicroProps(const MicroProps& other) = default;

    MicroProps& operator=(const MicroProps& other) = default;

    void processQuantity(DecimalQuantity&, MicroProps& micros, UErrorCode& status) const U_OVERRIDE {
        (void) status;
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

} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif // __NUMBER_MICROPROPS_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
