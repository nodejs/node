// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* precision.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __PRECISION_H__
#define __PRECISION_H__

#include "unicode/uobject.h"

#if !UCONFIG_NO_FORMATTING
#include "unicode/utypes.h"

#include "digitinterval.h"
#include "digitlst.h"
#include "significantdigitinterval.h"

U_NAMESPACE_BEGIN

class VisibleDigits;
class VisibleDigitsWithExponent;


/**
 * A precision manager for values to be formatted as fixed point.
 * Handles rounding of number to prepare it for formatting.
 */
class U_I18N_API FixedPrecision : public UMemory {
public:

    /**
     * The smallest format interval allowed. Default is 1 integer digit and no
     * fraction digits.
     */
    DigitInterval fMin;

    /**
     * The largest format interval allowed. Must contain fMin.
     *  Default is all digits.
     */
    DigitInterval fMax;

    /**
     * Min and max significant digits allowed. The default is no constraints.
     */
    SignificantDigitInterval fSignificant;

    /**
     * The rounding increment or zero if there is no rounding increment.
     * Default is zero.
     */
    DigitList fRoundingIncrement;
    
    /**
     * If set, causes round() to set status to U_FORMAT_INEXACT_ERROR if
     * any rounding is done. Default is FALSE.
     */
    UBool fExactOnly;
    
    /**
     * If set, causes round() to set status to U_ILLEGAL_ARGUMENT_ERROR if
     * rounded number has more than maximum integer digits. Default is FALSE.
     */
    UBool fFailIfOverMax;
    
    /**
     * Controls the rounding mode that initVisibleDigits uses.
     * Default is DecimalFormat::kRoundHalfEven
     */
    DecimalFormat::ERoundingMode fRoundingMode;
    
    FixedPrecision();
    
    /**
     * Returns TRUE if this object equals rhs.
     */
    UBool equals(const FixedPrecision &rhs) const {
        return (fMin.equals(rhs.fMin) &&
                fMax.equals(rhs.fMax) &&
                fSignificant.equals(rhs.fSignificant) &&
                (fRoundingIncrement == rhs.fRoundingIncrement) &&
                fExactOnly == rhs.fExactOnly &&
                fFailIfOverMax == rhs.fFailIfOverMax &&
                fRoundingMode == rhs.fRoundingMode);
    }
    
    /**
     * Rounds value in place to prepare it for formatting.
     * @param value The value to be rounded. It is rounded in place.
     * @param exponent Always pass 0 for fixed decimal formatting. scientific
     *  precision passes the exponent value.  Essentially, it divides value by
     *  10^exponent, rounds and then multiplies by 10^exponent.
     * @param status error returned here.
     * @return reference to value.
     */
    DigitList &round(DigitList &value, int32_t exponent, UErrorCode &status) const;
    
    /**
     * Returns the interval to use to format the rounded value.
     * @param roundedValue the already rounded value to format.
     * @param interval modified in place to be the interval to use to format
     *   the rounded value.
     * @return a reference to interval.
     */
    DigitInterval &getInterval(
            const DigitList &roundedValue, DigitInterval &interval) const;
    
    /**
     * Returns TRUE if this instance allows for fast formatting of integers.
     */
    UBool isFastFormattable() const;
    
    /**
     * Initializes a VisibleDigits.
     * @param value value for VisibleDigits
     *    Caller must not assume that the value of this parameter will remain
     *    unchanged.
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigits &initVisibleDigits(
            DigitList &value,
            VisibleDigits &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigits.
     * @param value value for VisibleDigits
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigits &initVisibleDigits(
            double value,
            VisibleDigits &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigits.
     * @param value value for VisibleDigits
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigits &initVisibleDigits(
            int64_t value,
            VisibleDigits &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigitsWithExponent.
     * @param value value for VisibleDigits
     *    Caller must not assume that the value of this parameter will remain
     *    unchanged.
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigitsWithExponent &initVisibleDigitsWithExponent(
            DigitList &value,
            VisibleDigitsWithExponent &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigitsWithExponent.
     * @param value value for VisibleDigits
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigitsWithExponent &initVisibleDigitsWithExponent(
            double value,
            VisibleDigitsWithExponent &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigitsWithExponent.
     * @param value value for VisibleDigits
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigitsWithExponent &initVisibleDigitsWithExponent(
            int64_t value,
            VisibleDigitsWithExponent &digits,
            UErrorCode &status) const;
    
private:
    /**
     * Attempts to initialize 'digits' using simple mod 10 arithmetic.
     * Returns FALSE if this is not possible such as when rounding
     * would change the value. Otherwise returns TRUE.
     *
     * If the method returns FALSE, caller should create a DigitList
     * and use it to initialize 'digits'. If this method returns TRUE,
     * caller should accept the value stored in 'digits'. If this
     * method returns TRUE along with a non zero error, caller must accept
     * the error and not try again with a DigitList.
     *
     * Before calling this method, caller must verify that this object
     * has no rounding increment set.
     *
     * The value that 'digits' is initialized to is mantissa * 10^exponent.
     * For example mantissa = 54700 and exponent = -3 means 54.7. The
     * properties of this object (such as min and max fraction digits),
     * not the number of trailing zeros in the mantissa, determine whether or
     * not the result contains any trailing 0's after the decimal point.
     *
     * @param mantissa the digits. May be positive or negative. May contain
     *  trailing zeros.
     * @param exponent must always be zero or negative. An exponent > 0
     *  yields undefined results! 
     * @param digits result stored here.
     * @param status any error returned here.
     */
    UBool
    initVisibleDigits(
            int64_t mantissa,
            int32_t exponent,
            VisibleDigits &digits,
            UErrorCode &status) const;
    UBool isRoundingRequired(
            int32_t upperExponent, int32_t lowerExponent) const;
    DigitInterval &getIntervalForZero(DigitInterval &interval) const;
    DigitInterval &getInterval(
            int32_t upperExponent, DigitInterval &interval) const;
    static UBool handleNonNumeric(DigitList &value, VisibleDigits &digits);
    
    friend class ScientificPrecision;
};

/**
 * A precision manager for values to be expressed as scientific notation.
 */
class U_I18N_API ScientificPrecision : public UMemory {
public:
    FixedPrecision fMantissa;
    int32_t fMinExponentDigits;

    ScientificPrecision();

    /**
     * rounds value in place to prepare it for formatting.
     * @param value The value to be rounded. It is rounded in place.
     * @param status error returned here.
     * @return reference to value.
     */
    DigitList &round(DigitList &value, UErrorCode &status) const;

    /**
     * Converts value to a mantissa and exponent.
     *
     * @param value modified in place to be the mantissa. Depending on
     *   the precision settings, the resulting mantissa may not fall
     *   between 1.0 and 10.0.
     * @return the exponent of value.
     */
    int32_t toScientific(DigitList &value) const;

    /**
     * Returns TRUE if this object equals rhs.
     */
    UBool equals(const ScientificPrecision &rhs) const {
        return fMantissa.equals(rhs.fMantissa) && fMinExponentDigits == rhs.fMinExponentDigits;
    }

    /**
     * Initializes a VisibleDigitsWithExponent.
     * @param value the value
     *    Caller must not assume that the value of this parameter will remain
     *    unchanged.
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigitsWithExponent &initVisibleDigitsWithExponent(
            DigitList &value,
            VisibleDigitsWithExponent &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigitsWithExponent.
     * @param value the value
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigitsWithExponent &initVisibleDigitsWithExponent(
            double value,
            VisibleDigitsWithExponent &digits,
            UErrorCode &status) const;
    
    /**
     * Initializes a VisibleDigitsWithExponent.
     * @param value the value
     * @param digits This is the value that is initialized.
     * @param status any error returned here.
     * @return digits
     */
    VisibleDigitsWithExponent &initVisibleDigitsWithExponent(
            int64_t value,
            VisibleDigitsWithExponent &digits,
            UErrorCode &status) const;
    
private:
    int32_t getMultiplier() const;

};



U_NAMESPACE_END
#endif // #if !UCONFIG_NO_FORMATTING
#endif  // __PRECISION_H__
