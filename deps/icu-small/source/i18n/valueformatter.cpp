/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

#include "unicode/plurrule.h"
#include "unicode/unistr.h"
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "digitformatter.h"
#include "digitgrouping.h"
#include "digitinterval.h"
#include "digitlst.h"
#include "precision.h"
#include "plurrule_impl.h"
#include "smallintformatter.h"
#include "uassert.h"
#include "valueformatter.h"
#include "visibledigits.h"

U_NAMESPACE_BEGIN

ValueFormatter::~ValueFormatter() {}

VisibleDigitsWithExponent &
ValueFormatter::toVisibleDigitsWithExponent(
        int64_t value,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    switch (fType) {
    case kFixedDecimal:
        return fFixedPrecision->initVisibleDigitsWithExponent(
                value, digits, status);
        break;
    case kScientificNotation:
        return fScientificPrecision->initVisibleDigitsWithExponent(
                value, digits, status);
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return digits;
}

VisibleDigitsWithExponent &
ValueFormatter::toVisibleDigitsWithExponent(
        DigitList &value,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    switch (fType) {
    case kFixedDecimal:
        return fFixedPrecision->initVisibleDigitsWithExponent(
                value, digits, status);
        break;
    case kScientificNotation:
        return fScientificPrecision->initVisibleDigitsWithExponent(
                value, digits, status);
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return digits;
}

static UBool isNoGrouping(
        const DigitGrouping &grouping,
        int32_t value,
        const FixedPrecision &precision) {
    IntDigitCountRange range(
            precision.fMin.getIntDigitCount(),
            precision.fMax.getIntDigitCount());
    return grouping.isNoGrouping(value, range);
}

UBool
ValueFormatter::isFastFormattable(int32_t value) const {
    switch (fType) {
    case kFixedDecimal:
        {
            if (value == INT32_MIN) {
                return FALSE;
            }
            if (value < 0) {
                value = -value;
            }
            return fFixedPrecision->isFastFormattable() && fFixedOptions->isFastFormattable() && isNoGrouping(*fGrouping, value, *fFixedPrecision);
        }
    case kScientificNotation:
        return FALSE;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return FALSE;
}

DigitList &
ValueFormatter::round(DigitList &value, UErrorCode &status) const {
    if (value.isNaN() || value.isInfinite()) {
        return value;
    }
    switch (fType) {
    case kFixedDecimal:
        return fFixedPrecision->round(value, 0, status);
    case kScientificNotation:
        return fScientificPrecision->round(value, status);
    default:
        U_ASSERT(FALSE);
        break;
    }
    return value;
}

UnicodeString &
ValueFormatter::formatInt32(
        int32_t value,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    switch (fType) {
    case kFixedDecimal:
        {
            IntDigitCountRange range(
                    fFixedPrecision->fMin.getIntDigitCount(),
                    fFixedPrecision->fMax.getIntDigitCount());
            return fDigitFormatter->formatPositiveInt32(
                    value,
                    range,
                    handler,
                    appendTo);
        }
        break;
    case kScientificNotation:
    default:
        U_ASSERT(FALSE);
        break;
    }
    return appendTo;
}

UnicodeString &
ValueFormatter::format(
        const VisibleDigitsWithExponent &value,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    switch (fType) {
    case kFixedDecimal:
        return fDigitFormatter->format(
                value.getMantissa(),
                *fGrouping,
                *fFixedOptions,
                handler,
                appendTo);
        break;
    case kScientificNotation:
        return fDigitFormatter->format(
                value,
                *fScientificOptions,
                handler,
                appendTo);
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return appendTo;
}

int32_t
ValueFormatter::countChar32(const VisibleDigitsWithExponent &value) const {
    switch (fType) {
    case kFixedDecimal:
        return fDigitFormatter->countChar32(
                value.getMantissa(),
                *fGrouping,
                *fFixedOptions);
        break;
    case kScientificNotation:
        return fDigitFormatter->countChar32(
                value,
                *fScientificOptions);
        break;
    default:
        U_ASSERT(FALSE);
        break;
    }
    return 0;
}

void
ValueFormatter::prepareFixedDecimalFormatting(
        const DigitFormatter &formatter,
        const DigitGrouping &grouping,
        const FixedPrecision &precision,
        const DigitFormatterOptions &options) {
    fType = kFixedDecimal;
    fDigitFormatter = &formatter;
    fGrouping = &grouping;
    fFixedPrecision = &precision;
    fFixedOptions = &options;
}

void
ValueFormatter::prepareScientificFormatting(
        const DigitFormatter &formatter,
        const ScientificPrecision &precision,
        const SciFormatterOptions &options) {
    fType = kScientificNotation;
    fDigitFormatter = &formatter;
    fScientificPrecision = &precision;
    fScientificOptions = &options;
}

U_NAMESPACE_END

#endif /* !UCONFIG_NO_FORMATTING */
