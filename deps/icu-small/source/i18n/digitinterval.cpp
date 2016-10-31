// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitinterval.cpp
 */

#include "unicode/utypes.h"

#include "digitinterval.h"

U_NAMESPACE_BEGIN

void DigitInterval::expandToContain(const DigitInterval &rhs) {
    if (fSmallestInclusive > rhs.fSmallestInclusive) {
        fSmallestInclusive = rhs.fSmallestInclusive;
    }
    if (fLargestExclusive < rhs.fLargestExclusive) {
        fLargestExclusive = rhs.fLargestExclusive;
    }
}

void DigitInterval::shrinkToFitWithin(const DigitInterval &rhs) {
    if (fSmallestInclusive < rhs.fSmallestInclusive) {
        fSmallestInclusive = rhs.fSmallestInclusive;
    }
    if (fLargestExclusive > rhs.fLargestExclusive) {
        fLargestExclusive = rhs.fLargestExclusive;
    }
}

void DigitInterval::setIntDigitCount(int32_t count) {
    fLargestExclusive = count < 0 ? INT32_MAX : count;
}

void DigitInterval::setFracDigitCount(int32_t count) {
    fSmallestInclusive = count < 0 ? INT32_MIN : -count;
}

void DigitInterval::expandToContainDigit(int32_t digitExponent) {
  if (fLargestExclusive <= digitExponent) {
      fLargestExclusive = digitExponent + 1;
  } else if (fSmallestInclusive > digitExponent) {
      fSmallestInclusive = digitExponent;
  }
}

UBool DigitInterval::contains(int32_t x) const {
    return (x < fLargestExclusive && x >= fSmallestInclusive);
}


U_NAMESPACE_END

