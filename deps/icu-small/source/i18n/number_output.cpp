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

NounClass FormattedNumber::getNounClass(UErrorCode &status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD(NounClass::OTHER);
    const char *nounClass = fData->gender;

    // if it is not exist, return `OTHER`
    if (uprv_strcmp(nounClass, "") == 0) {
        return NounClass::OTHER;
    }

    if (uprv_strcmp(nounClass, "neuter") == 0) {
        return NounClass::NEUTER;
    }

    if (uprv_strcmp(nounClass, "feminine") == 0) {
        return NounClass::FEMININE;
    }

    if (uprv_strcmp(nounClass, "masculine") == 0) {
        return NounClass::MASCULINE;
    }

    if (uprv_strcmp(nounClass, "animate") == 0) {
        return NounClass::ANIMATE;
    }

    if (uprv_strcmp(nounClass, "inanimate") == 0) {
        return NounClass::INANIMATE;
    }

    if (uprv_strcmp(nounClass, "personal") == 0) {
        return NounClass::PERSONAL;
    }

    if (uprv_strcmp(nounClass, "common") == 0) {
        return NounClass::COMMON;
    }

    // In case there is no matching, this means there are noun classes
    // that are not supported yet.
    status = U_INTERNAL_PROGRAM_ERROR;
    return NounClass::OTHER;
}

const char *FormattedNumber::getGender(UErrorCode &status) const {
    UPRV_FORMATTED_VALUE_METHOD_GUARD("")
    return fData->gender;
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
