// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "unicode/numberformatter.h"
#include "number_patternstring.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

Grouper Grouper::defaults() {
    return {-2, -2, false};
}

Grouper Grouper::minTwoDigits() {
    return {-2, -2, true};
}

Grouper Grouper::none() {
    return {-1, -1, false};
}

void Grouper::setLocaleData(const impl::ParsedPatternInfo &patternInfo) {
    if (fGrouping1 != -2) {
        return;
    }
    auto grouping1 = static_cast<int8_t> (patternInfo.positive.groupingSizes & 0xffff);
    auto grouping2 = static_cast<int8_t> ((patternInfo.positive.groupingSizes >> 16) & 0xffff);
    auto grouping3 = static_cast<int8_t> ((patternInfo.positive.groupingSizes >> 32) & 0xffff);
    if (grouping2 == -1) {
        grouping1 = -1;
    }
    if (grouping3 == -1) {
        grouping2 = grouping1;
    }
    fGrouping1 = grouping1;
    fGrouping2 = grouping2;
}

bool Grouper::groupAtPosition(int32_t position, const impl::DecimalQuantity &value) const {
    U_ASSERT(fGrouping1 > -2);
    if (fGrouping1 == -1 || fGrouping1 == 0) {
        // Either -1 or 0 means "no grouping"
        return false;
    }
    position -= fGrouping1;
    return position >= 0 && (position % fGrouping2) == 0
           && value.getUpperDisplayMagnitude() - fGrouping1 + 1 >= (fMin2 ? 2 : 1);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
