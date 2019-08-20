// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __SOURCE_NUMRANGE_TYPES_H__
#define __SOURCE_NUMRANGE_TYPES_H__

#include "unicode/numberformatter.h"
#include "unicode/numberrangeformatter.h"
#include "unicode/simpleformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"
#include "number_formatimpl.h"
#include "number_stringbuilder.h"
#include "formattedval_impl.h"

U_NAMESPACE_BEGIN namespace number {
namespace impl {


/**
 * Class similar to UFormattedNumberData.
 *
 * Has incomplete magic number logic that will need to be finished
 * if this is to be exposed as C API in the future.
 *
 * Possible magic number: 0x46445200
 * Reads in ASCII as "FDR" (FormatteDnumberRange with room at the end)
 */
class UFormattedNumberRangeData : public FormattedValueNumberStringBuilderImpl {
public:
    UFormattedNumberRangeData() : FormattedValueNumberStringBuilderImpl(0) {}
    virtual ~UFormattedNumberRangeData();

    DecimalQuantity quantity1;
    DecimalQuantity quantity2;
    UNumberRangeIdentityResult identityResult = UNUM_IDENTITY_RESULT_COUNT;
};


class StandardPluralRanges : public UMemory {
  public:
    void initialize(const Locale& locale, UErrorCode& status);
    StandardPlural::Form resolve(StandardPlural::Form first, StandardPlural::Form second) const;

    /** Used for data loading. */
    void addPluralRange(
        StandardPlural::Form first,
        StandardPlural::Form second,
        StandardPlural::Form result);

    /** Used for data loading. */
    void setCapacity(int32_t length);

  private:
    struct StandardPluralRangeTriple {
        StandardPlural::Form first;
        StandardPlural::Form second;
        StandardPlural::Form result;
    };

    // TODO: An array is simple here, but it results in linear lookup time.
    // Certain locales have 20-30 entries in this list.
    // Consider changing to a smarter data structure.
    typedef MaybeStackArray<StandardPluralRangeTriple, 3> PluralRangeTriples;
    PluralRangeTriples fTriples;
    int32_t fTriplesLen = 0;
};


class NumberRangeFormatterImpl : public UMemory {
  public:
    NumberRangeFormatterImpl(const RangeMacroProps& macros, UErrorCode& status);

    void format(UFormattedNumberRangeData& data, bool equalBeforeRounding, UErrorCode& status) const;

  private:
    NumberFormatterImpl formatterImpl1;
    NumberFormatterImpl formatterImpl2;
    bool fSameFormatters;

    UNumberRangeCollapse fCollapse;
    UNumberRangeIdentityFallback fIdentityFallback;

    SimpleFormatter fRangeFormatter;
    SimpleModifier fApproximatelyModifier;

    StandardPluralRanges fPluralRanges;

    void formatSingleValue(UFormattedNumberRangeData& data,
                           MicroProps& micros1, MicroProps& micros2,
                           UErrorCode& status) const;

    void formatApproximately(UFormattedNumberRangeData& data,
                             MicroProps& micros1, MicroProps& micros2,
                             UErrorCode& status) const;

    void formatRange(UFormattedNumberRangeData& data,
                     MicroProps& micros1, MicroProps& micros2,
                     UErrorCode& status) const;

    const Modifier& resolveModifierPlurals(const Modifier& first, const Modifier& second) const;
};


} // namespace impl
} // namespace number
U_NAMESPACE_END

#endif //__SOURCE_NUMRANGE_TYPES_H__
#endif /* #if !UCONFIG_NO_FORMATTING */
