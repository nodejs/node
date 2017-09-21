// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

IntegerWidth::IntegerWidth(int8_t minInt, int8_t maxInt) {
    fUnion.minMaxInt.fMinInt = minInt;
    fUnion.minMaxInt.fMaxInt = maxInt;
}

IntegerWidth IntegerWidth::zeroFillTo(int32_t minInt) {
    if (minInt >= 0 && minInt <= kMaxIntFracSig) {
        return {static_cast<int8_t>(minInt), -1};
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

IntegerWidth IntegerWidth::truncateAt(int32_t maxInt) {
    if (fHasError) { return *this; }  // No-op on error
    if (maxInt >= 0 && maxInt <= kMaxIntFracSig) {
        return {fUnion.minMaxInt.fMinInt, static_cast<int8_t>(maxInt)};
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

void IntegerWidth::apply(impl::DecimalQuantity &quantity, UErrorCode &status) const {
    if (fHasError) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
    } else if (fUnion.minMaxInt.fMaxInt == -1) {
        quantity.setIntegerLength(fUnion.minMaxInt.fMinInt, INT32_MAX);
    } else {
        quantity.setIntegerLength(fUnion.minMaxInt.fMinInt, fUnion.minMaxInt.fMaxInt);
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
