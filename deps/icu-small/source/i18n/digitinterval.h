// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitinterval.h
*
* created on: 2015jan6
* created by: Travis Keep
*/

#ifndef __DIGITINTERVAL_H__
#define __DIGITINTERVAL_H__

#include "unicode/uobject.h"
#include "unicode/utypes.h"

U_NAMESPACE_BEGIN

/**
 * An interval of digits.
 * DigitIntervals are for fixed point formatting. A DigitInterval specifies
 * zero or more integer digits and zero or more fractional digits. This class
 * specifies particular digits in a number by their power of 10. For example,
 * the digit position just to the left of the decimal is 0, and the digit
 * position just left of that is 1. The digit position just to the right of
 * the decimal is -1. The digit position just to the right of that is -2.
 */
class U_I18N_API DigitInterval : public UMemory {
public:

    /**
     * Spans all integer and fraction digits
     */
    DigitInterval()
            : fLargestExclusive(INT32_MAX), fSmallestInclusive(INT32_MIN) { }


    /**
     * Makes this instance span all digits.
     */
    void clear() {
        fLargestExclusive = INT32_MAX;
        fSmallestInclusive = INT32_MIN;
    }

    /**
     * Returns TRUE if this interval contains this digit position.
     */
    UBool contains(int32_t digitPosition) const;

    /**
     * Returns true if this object is the same as rhs.
     */
    UBool equals(const DigitInterval &rhs) const {
        return ((fLargestExclusive == rhs.fLargestExclusive) &&
                (fSmallestInclusive == rhs.fSmallestInclusive));
    }

    /**
     * Expand this interval so that it contains all of rhs.
     */
    void expandToContain(const DigitInterval &rhs);

    /**
     * Shrink this interval so that it contains no more than rhs.
     */
    void shrinkToFitWithin(const DigitInterval &rhs);

    /**
     * Expand this interval as necessary to contain digit with given exponent
     * After this method returns, this interval is guaranteed to contain
     * digitExponent.
     */
    void expandToContainDigit(int32_t digitExponent);

    /**
     * Changes the number of digits to the left of the decimal point that
     * this interval spans. If count is negative, it means span all digits
     * to the left of the decimal point.
     */
    void setIntDigitCount(int32_t count);

    /**
     * Changes the number of digits to the right of the decimal point that
     * this interval spans. If count is negative, it means span all digits
     * to the right of the decimal point.
     */
    void setFracDigitCount(int32_t count);

    /**
     * Sets the least significant inclusive value to smallest. If smallest >= 0
     * then least significant inclusive value becomes 0.
     */
    void setLeastSignificantInclusive(int32_t smallest) {
        fSmallestInclusive = smallest < 0 ? smallest : 0;
    }

    /**
     * Sets the most significant exclusive value to largest.
     * If largest <= 0 then most significant exclusive value becomes 0.
     */
    void setMostSignificantExclusive(int32_t largest) {
        fLargestExclusive = largest > 0 ? largest : 0;
    }

    /**
     * If returns 8, the most significant digit in interval is the 10^7 digit.
     * Returns INT32_MAX if this interval spans all digits to left of
     * decimal point.
     */
    int32_t getMostSignificantExclusive() const {
        return fLargestExclusive;
    }

    /**
     * Returns number of digits to the left of the decimal that this
     * interval includes. This is a synonym for getMostSignificantExclusive().
     */
    int32_t getIntDigitCount() const {
        return fLargestExclusive;
    }

    /**
     * Returns number of digits to the right of the decimal that this
     * interval includes.
     */
    int32_t getFracDigitCount() const {
        return fSmallestInclusive == INT32_MIN ? INT32_MAX : -fSmallestInclusive;
    }

    /**
     * Returns the total number of digits that this interval spans.
     * Caution: If this interval spans all digits to the left or right of
     * decimal point instead of some fixed number, then what length()
     * returns is undefined.
     */
    int32_t length() const {
        return fLargestExclusive - fSmallestInclusive;
     }

    /**
     * If returns -3, the least significant digit in interval is the 10^-3
     * digit. Returns INT32_MIN if this interval spans all digits to right of
     * decimal point.
     */
    int32_t getLeastSignificantInclusive() const {
        return fSmallestInclusive;
    }
private:
    int32_t fLargestExclusive;
    int32_t fSmallestInclusive;
};

U_NAMESPACE_END

#endif  // __DIGITINTERVAL_H__
