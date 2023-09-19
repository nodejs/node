// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/measunit.h"
#include "unicode/numberformatter.h"
#include "number_utypes.h"
#include "util.h"
#include "number_decimalquantity.h"
#include "number_decnum.h"
#include "numrange_impl.h"

U_NAMESPACE_BEGIN
namespace number {


UPRV_FORMATTED_VALUE_SUBCLASS_AUTO_IMPL(FormattedNumber)

#define UPRV_NOARG

void FormattedNumber::toDecimalNumber(ByteSink& sink, UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(UPRV_NOARG)
    impl::DecNum decnum;
    fData->quantity.toDecNum(decnum, status);
    decnum.toString(sink, status);
}

void FormattedNumber::getAllFieldPositionsImpl(FieldPositionIteratorHandler& fpih,
                                               UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(UPRV_NOARG)
    fData->getAllFieldPositions(fpih, status);
}

MeasureUnit FormattedNumber::getOutputUnit(UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(MeasureUnit())
    return fData->outputUnit;
}

UDisplayOptionsNounClass FormattedNumber::getNounClass(UErrorCode &status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(UDISPOPT_NOUN_CLASS_UNDEFINED);
    const char *nounClass = fData->gender;
    return udispopt_fromNounClassIdentifier(nounClass);
}

void FormattedNumber::getDecimalQuantity(impl::DecimalQuantity& output, UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(UPRV_NOARG)
    output = fData->quantity;
}


impl::UFormattedNumberData::~UFormattedNumberData() = default;


UPRV_FORMATTED_VALUE_SUBCLASS_AUTO_IMPL(FormattedNumberRange)

#define UPRV_NOARG

void FormattedNumberRange::getDecimalNumbers(ByteSink& sink1, ByteSink& sink2, UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(UPRV_NOARG)
    impl::DecNum decnum1;
    impl::DecNum decnum2;
    fData->quantity1.toDecNum(decnum1, status).toString(sink1, status);
    fData->quantity2.toDecNum(decnum2, status).toString(sink2, status);
}

UNumberRangeIdentityResult FormattedNumberRange::getIdentityResult(UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(UNUM_IDENTITY_RESULT_NOT_EQUAL)
    return fData->identityResult;
}

const impl::UFormattedNumberRangeData* FormattedNumberRange::getData(UErrorCode& status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(nullptr)
    return fData;
}


impl::UFormattedNumberRangeData::~UFormattedNumberRangeData() = default;


} // namespace number
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
