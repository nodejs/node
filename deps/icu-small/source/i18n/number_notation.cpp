// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "unicode/numberformatter.h"
#include "number_types.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;


ScientificNotation Notation::scientific() {
    // NOTE: ISO C++ does not allow C99 designated initializers.
    ScientificSettings settings;
    settings.fEngineeringInterval = 1;
    settings.fRequireMinInt = false;
    settings.fMinExponentDigits = 1;
    settings.fExponentSignDisplay = UNUM_SIGN_AUTO;
    NotationUnion union_;
    union_.scientific = settings;
    return {NTN_SCIENTIFIC, union_};
}

ScientificNotation Notation::engineering() {
    ScientificSettings settings;
    settings.fEngineeringInterval = 3;
    settings.fRequireMinInt = false;
    settings.fMinExponentDigits = 1;
    settings.fExponentSignDisplay = UNUM_SIGN_AUTO;
    NotationUnion union_;
    union_.scientific = settings;
    return {NTN_SCIENTIFIC, union_};
}

Notation Notation::compactShort() {
    NotationUnion union_;
    union_.compactStyle = CompactStyle::UNUM_SHORT;
    return {NTN_COMPACT, union_};
}

Notation Notation::compactLong() {
    NotationUnion union_;
    union_.compactStyle = CompactStyle::UNUM_LONG;
    return {NTN_COMPACT, union_};
}

Notation Notation::simple() {
    return {};
}

ScientificNotation
ScientificNotation::withMinExponentDigits(int32_t minExponentDigits) const {
    if (minExponentDigits >= 0 && minExponentDigits < kMaxIntFracSig) {
        ScientificSettings settings = fUnion.scientific;
        settings.fMinExponentDigits = (int8_t) minExponentDigits;
        NotationUnion union_ = {settings};
        return {NTN_SCIENTIFIC, union_};
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

ScientificNotation
ScientificNotation::withExponentSignDisplay(UNumberSignDisplay exponentSignDisplay) const {
    ScientificSettings settings = fUnion.scientific;
    settings.fExponentSignDisplay = exponentSignDisplay;
    NotationUnion union_ = {settings};
    return {NTN_SCIENTIFIC, union_};
}

#endif /* #if !UCONFIG_NO_FORMATTING */
