/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitgrouping.cpp
 */

#include "unicode/utypes.h"

#include "digitgrouping.h"
#include "smallintformatter.h"

U_NAMESPACE_BEGIN

UBool DigitGrouping::isSeparatorAt(
        int32_t digitsLeftOfDecimal, int32_t digitPos) const {
    if (!isGroupingEnabled(digitsLeftOfDecimal) || digitPos < fGrouping) {
        return FALSE;
    }
    return ((digitPos - fGrouping) % getGrouping2() == 0);
}

int32_t DigitGrouping::getSeparatorCount(int32_t digitsLeftOfDecimal) const {
    if (!isGroupingEnabled(digitsLeftOfDecimal)) {
        return 0;
    }
    return (digitsLeftOfDecimal - 1 - fGrouping) / getGrouping2() + 1;
}

UBool DigitGrouping::isGroupingEnabled(int32_t digitsLeftOfDecimal) const {
    return (isGroupingUsed()
            && digitsLeftOfDecimal >= fGrouping + getMinGrouping());
}

UBool DigitGrouping::isNoGrouping(
        int32_t positiveValue, const IntDigitCountRange &range) const {
    return getSeparatorCount(
            SmallIntFormatter::estimateDigitCount(positiveValue, range)) == 0;
}

int32_t DigitGrouping::getGrouping2() const {
    return (fGrouping2 > 0 ? fGrouping2 : fGrouping);
}

int32_t DigitGrouping::getMinGrouping() const {
    return (fMinGrouping > 0 ? fMinGrouping : 1);
}

void
DigitGrouping::clear() {
    fMinGrouping = 0;
    fGrouping = 0;
    fGrouping2 = 0;
}

U_NAMESPACE_END

