// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_SCIENTIFIC_H__
#define __NUMBER_SCIENTIFIC_H__

#include "number_types.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

// Forward-declare
class ScientificHandler;

class U_I18N_API ScientificModifier : public UMemory, public Modifier {
  public:
    ScientificModifier();

    void set(int32_t exponent, const ScientificHandler *handler);

    int32_t apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                  UErrorCode &status) const U_OVERRIDE;

    int32_t getPrefixLength(UErrorCode &status) const U_OVERRIDE;

    int32_t getCodePointCount(UErrorCode &status) const U_OVERRIDE;

    bool isStrong() const U_OVERRIDE;

  private:
    int32_t fExponent;
    const ScientificHandler *fHandler;
};

class ScientificHandler : public UMemory, public MicroPropsGenerator, public MultiplierProducer {
  public:
    ScientificHandler(const Notation *notation, const DecimalFormatSymbols *symbols,
                      const MicroPropsGenerator *parent);

    void
    processQuantity(DecimalQuantity &quantity, MicroProps &micros, UErrorCode &status) const U_OVERRIDE;

    int32_t getMultiplier(int32_t magnitude) const U_OVERRIDE;

  private:
    const Notation::ScientificSettings& fSettings;
    const DecimalFormatSymbols *fSymbols;
    const MicroPropsGenerator *fParent;

    friend class ScientificModifier;
};

} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__NUMBER_SCIENTIFIC_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
