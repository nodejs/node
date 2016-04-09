/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: visibledigits.cpp
 */

#include <math.h>

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "cstring.h"
#include "decNumber.h"
#include "digitlst.h"
#include "uassert.h"
#include "visibledigits.h"

static const int32_t kNegative = 1;
static const int32_t kInfinite = 2;
static const int32_t kNaN = 4;

U_NAMESPACE_BEGIN

void VisibleDigits::setNegative() {
    fFlags |= kNegative;
}

void VisibleDigits::setNaN() {
    fFlags |= kNaN;
}

void VisibleDigits::setInfinite() {
    fFlags |= kInfinite;
}

void VisibleDigits::clear() {
    fInterval.clear();
    fDigits.clear();
    fExponent = 0;
    fFlags = 0;
    fAbsIntValue = 0LL;
    fAbsIntValueSet = FALSE;
    fAbsDoubleValue = 0.0;
    fAbsDoubleValueSet = FALSE;
}

UBool VisibleDigits::isNegative() const {
    return (fFlags & kNegative);
}

UBool VisibleDigits::isNaN() const {
    return (fFlags & kNaN);
}

UBool VisibleDigits::isInfinite() const {
    return (fFlags & kInfinite);
}

int32_t VisibleDigits::getDigitByExponent(int32_t digitPos) const {
    if (digitPos < fExponent || digitPos >= fExponent + fDigits.length()) {
        return 0;
    }
    const char *ptr = fDigits.data();
    return ptr[digitPos - fExponent];
}

UBool VisibleDigits::isOverMaxDigits() const {
    return (fExponent + fDigits.length() > fInterval.getMostSignificantExclusive());
}

UBool VisibleDigits::isNaNOrInfinity() const {
    return (fFlags & (kInfinite | kNaN)) != 0;
}

double VisibleDigits::computeAbsDoubleValue() const {
    // Take care of NaN and infinity
    if (isNaN()) {
        return uprv_getNaN();
    }
    if (isInfinite()) {
        return uprv_getInfinity();
    }

    // stack allocate a decNumber to hold MAX_DBL_DIGITS+3 significant digits
    char rawNumber[sizeof(decNumber) + MAX_DBL_DIGITS+3];
    decNumber *numberPtr = (decNumber *) rawNumber;

    int32_t mostSig = fInterval.getMostSignificantExclusive();
    int32_t mostSigNonZero = fExponent + fDigits.length();
    int32_t end = mostSig > mostSigNonZero ? mostSigNonZero : mostSig;
    int32_t leastSig = fInterval.getLeastSignificantInclusive();
    int32_t start = leastSig > fExponent ? leastSig : fExponent;
    if (end <= start) {
        return 0.0;
    }
    if (start < end - (MAX_DBL_DIGITS+3)) {
        start = end - (MAX_DBL_DIGITS+3);
    }
    uint8_t *pos = numberPtr->lsu;
    const char *src = &(fDigits.data()[start - fExponent]);
    for (int32_t i = start; i < end; ++i) {
        *pos++ = (uint8_t) (*src++);
    }
    numberPtr->exponent = start;
    numberPtr->bits = 0;
    numberPtr->digits = end - start;
    char str[MAX_DBL_DIGITS+18];
    uprv_decNumberToString(numberPtr, str);
    U_ASSERT(uprv_strlen(str) < MAX_DBL_DIGITS+18);
    char decimalSeparator = DigitList::getStrtodDecimalSeparator();
    if (decimalSeparator != '.') {
        char *decimalPt = strchr(str, '.');
        if (decimalPt != NULL) {
            *decimalPt = decimalSeparator;
        }
    }
    char *unused = NULL;
    return uprv_strtod(str, &unused);
}

void VisibleDigits::getFixedDecimal(
    double &source, int64_t &intValue, int64_t &f, int64_t &t, int32_t &v, UBool &hasIntValue) const {
    source = 0.0;
    intValue = 0;
    f = 0;
    t = 0;
    v = 0;
    hasIntValue = FALSE;
    if (isNaNOrInfinity()) {
        return;
    }

    // source
    if (fAbsDoubleValueSet) {
        source = fAbsDoubleValue;
    } else {
        source = computeAbsDoubleValue();
    }

    // visible decimal digits
    v = fInterval.getFracDigitCount();

    // intValue

    // If we initialized from an int64 just use that instead of
    // calculating
    if (fAbsIntValueSet) {
        intValue = fAbsIntValue;
    } else {
        int32_t startPos = fInterval.getMostSignificantExclusive();
        if (startPos > 18) {
            startPos = 18;
        }
        // process the integer digits
        for (int32_t i = startPos - 1; i >= 0; --i) {
            intValue = intValue * 10LL + getDigitByExponent(i);
        }
        if (intValue == 0LL && startPos > 0) {
            intValue = 100000000000000000LL;
        }
    }

    // f (decimal digits)
    // skip over any leading 0's in fraction digits.
    int32_t idx = -1;
    for (; idx >= -v && getDigitByExponent(idx) == 0; --idx);

    // Only process up to first 18 non zero fraction digits for decimalDigits
    // since that is all we can fit into an int64.
    for (int32_t i = idx; i >= -v && i > idx - 18; --i) {
        f = f * 10LL + getDigitByExponent(i);
    }

    // If we have no decimal digits, we don't have an integer value
    hasIntValue = (f == 0LL);

    // t (decimal digits without trailing zeros)
   t = f;
    while (t > 0 && t % 10LL == 0) {
        t /= 10;
    }
}

U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
