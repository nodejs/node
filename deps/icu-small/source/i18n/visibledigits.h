/*
******************************************************************************* * Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* visibledigits.h
*
* created on: 2015jun20
* created by: Travis Keep
*/

#ifndef __VISIBLEDIGITS_H__
#define __VISIBLEDIGITS_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uobject.h"

#include "charstr.h"
#include "digitinterval.h"

U_NAMESPACE_BEGIN

class DigitList;

/**
 * VisibleDigits represents the digits visible for formatting.
 * Once initialized using a FixedPrecision instance, VisibleDigits instances
 * remain unchanged until they are initialized again. A VisibleDigits with
 * a numeric value equal to 3.0 could be "3", "3.0", "3.00" or even "003.0"
 * depending on settings of the FixedPrecision instance used to initialize it.
 */
class U_I18N_API VisibleDigits : public UMemory {
public:
    VisibleDigits() : fExponent(0), fFlags(0), fAbsIntValue(0), fAbsIntValueSet(FALSE), fAbsDoubleValue(0.0), fAbsDoubleValueSet(FALSE) { }

    UBool isNegative() const;
    UBool isNaN() const;
    UBool isInfinite() const;
    UBool isNaNOrInfinity() const;

    /**
     * Gets the digit at particular exponent, if number is 987.6, then
     * getDigit(2) == 9 and gitDigit(0) == 7 and gitDigit(-1) == 6.
     * If isNaN() or isInfinity() return TRUE, then the result of this
     * function is undefined.
     */
    int32_t getDigitByExponent(int32_t digitPos) const;

    /**
     * Returns the digit interval which indicates the leftmost and rightmost
     * position of this instance. 
     * If isNaN() or isInfinity() return TRUE, then the result of this
     * function is undefined.
     */
    const DigitInterval &getInterval() const { return fInterval; }

    /**
     * Gets the parameters needed to create a FixedDecimal.
     */
    void getFixedDecimal(double &source, int64_t &intValue, int64_t &f, int64_t &t, int32_t &v, UBool &hasIntValue) const;


private:
    /**
     * The digits, least significant first. Both the least and most
     * significant digit in this list are non-zero; however, digits in the
     * middle may be zero. This field contains values between (char) 0, and
     * (char) 9 inclusive.
     */
    CharString fDigits;

    /**
     * The range of displayable digits. This field is needed to account for
     * any leading and trailing zeros which are not stored in fDigits.
     */
    DigitInterval fInterval;

    /**
     * The exponent value of the least significant digit in fDigits. For
     * example, fExponent = 2 and fDigits = {7, 8, 5} represents 58700.
     */
    int32_t fExponent;

    /**
     * Contains flags such as NaN, Inf, and negative.
     */
    int32_t fFlags;

    /**
     * Contains the absolute value of the digits left of the decimal place
     * if fAbsIntValueSet is TRUE
     */
    int64_t fAbsIntValue;

    /**
     * Indicates whether or not fAbsIntValue is set.
     */
    UBool fAbsIntValueSet;

    /**
     * Contains the absolute value of the value this instance represents
     * if fAbsDoubleValueSet is TRUE
     */
    double fAbsDoubleValue;

    /**
     * Indicates whether or not fAbsDoubleValue is set.
     */
    UBool fAbsDoubleValueSet;

    void setNegative();
    void setNaN();
    void setInfinite();
    void clear();
    double computeAbsDoubleValue() const;
    UBool isOverMaxDigits() const;

    VisibleDigits(const VisibleDigits &);
    VisibleDigits &operator=(const VisibleDigits &);

    friend class FixedPrecision;
    friend class VisibleDigitsWithExponent;
};

/**
 * A VisibleDigits with a possible exponent.
 */
class U_I18N_API VisibleDigitsWithExponent : public UMemory {
public:
    VisibleDigitsWithExponent() : fHasExponent(FALSE) { }
    const VisibleDigits &getMantissa() const { return fMantissa; }
    const VisibleDigits *getExponent() const {
        return fHasExponent ? &fExponent : NULL;
    }
    void clear() {
        fMantissa.clear();
        fExponent.clear();
        fHasExponent = FALSE;
    }
    UBool isNegative() const { return fMantissa.isNegative(); }
    UBool isNaN() const { return fMantissa.isNaN(); }
    UBool isInfinite() const { return fMantissa.isInfinite(); }
private:
    VisibleDigitsWithExponent(const VisibleDigitsWithExponent &);
    VisibleDigitsWithExponent &operator=(
        const VisibleDigitsWithExponent &);
    VisibleDigits fMantissa;
    VisibleDigits fExponent;
    UBool fHasExponent;

    friend class ScientificPrecision;
    friend class FixedPrecision;
};


U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif  // __VISIBLEDIGITS_H__
