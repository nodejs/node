// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT
#ifndef __NUMBER_LONGNAMES_H__
#define __NUMBER_LONGNAMES_H__

#include "unicode/uversion.h"
#include "number_utils.h"
#include "number_modifiers.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {

class LongNameHandler : public MicroPropsGenerator, public UMemory {
  public:
    static LongNameHandler
    forCurrencyLongNames(const Locale &loc, const CurrencyUnit &currency, const PluralRules *rules,
                         const MicroPropsGenerator *parent, UErrorCode &status);

    static LongNameHandler
    forMeasureUnit(const Locale &loc, const MeasureUnit &unit, const UNumberUnitWidth &width,
                   const PluralRules *rules, const MicroPropsGenerator *parent, UErrorCode &status);

    void
    processQuantity(DecimalQuantity &quantity, MicroProps &micros, UErrorCode &status) const U_OVERRIDE;

  private:
    SimpleModifier fModifiers[StandardPlural::Form::COUNT];
    const PluralRules *rules;
    const MicroPropsGenerator *parent;

    LongNameHandler(const PluralRules *rules, const MicroPropsGenerator *parent)
            : rules(rules), parent(parent) {}

    static void simpleFormatsToModifiers(const UnicodeString *simpleFormats, Field field,
                                         SimpleModifier *output, UErrorCode &status);
};

}  // namespace impl
}  // namespace number
U_NAMESPACE_END

#endif //__NUMBER_LONGNAMES_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
