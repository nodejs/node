// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_stringbuilder.h"
#include "number_decimfmtprops.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

int32_t
addPaddingHelper(UChar32 paddingCp, int32_t requiredPadding, NumberStringBuilder &string, int32_t index,
                 UErrorCode &status) {
    for (int32_t i = 0; i < requiredPadding; i++) {
        // TODO: If appending to the end, this will cause actual insertion operations. Improve.
        string.insertCodePoint(index, paddingCp, UNUM_FIELD_COUNT, status);
    }
    return U16_LENGTH(paddingCp) * requiredPadding;
}

}

Padder::Padder(UChar32 cp, int32_t width, UNumberFormatPadPosition position) : fWidth(width) {
    // TODO(13034): Consider making this a string instead of code point.
    fUnion.padding.fCp = cp;
    fUnion.padding.fPosition = position;
}

Padder::Padder(int32_t width) : fWidth(width) {}

Padder Padder::none() {
    return {-1};
}

Padder Padder::codePoints(UChar32 cp, int32_t targetWidth, UNumberFormatPadPosition position) {
    // TODO: Validate the code point?
    if (targetWidth >= 0) {
        return {cp, targetWidth, position};
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

Padder Padder::forProperties(const DecimalFormatProperties& properties) {
    UChar32 padCp;
    if (properties.padString.length() > 0) {
        padCp = properties.padString.char32At(0);
    } else {
        padCp = kFallbackPaddingString[0];
    }
    return {padCp, properties.formatWidth, properties.padPosition.getOrDefault(UNUM_PAD_BEFORE_PREFIX)};
}

int32_t Padder::padAndApply(const Modifier &mod1, const Modifier &mod2,
                            NumberStringBuilder &string, int32_t leftIndex, int32_t rightIndex,
                            UErrorCode &status) const {
    int32_t modLength = mod1.getCodePointCount() + mod2.getCodePointCount();
    int32_t requiredPadding = fWidth - modLength - string.codePointCount();
    U_ASSERT(leftIndex == 0 &&
             rightIndex == string.length()); // fix the previous line to remove this assertion

    int length = 0;
    if (requiredPadding <= 0) {
        // Padding is not required.
        length += mod1.apply(string, leftIndex, rightIndex, status);
        length += mod2.apply(string, leftIndex, rightIndex + length, status);
        return length;
    }

    PadPosition position = fUnion.padding.fPosition;
    UChar32 paddingCp = fUnion.padding.fCp;
    if (position == UNUM_PAD_AFTER_PREFIX) {
        length += addPaddingHelper(paddingCp, requiredPadding, string, leftIndex, status);
    } else if (position == UNUM_PAD_BEFORE_SUFFIX) {
        length += addPaddingHelper(paddingCp, requiredPadding, string, rightIndex + length, status);
    }
    length += mod1.apply(string, leftIndex, rightIndex + length, status);
    length += mod2.apply(string, leftIndex, rightIndex + length, status);
    if (position == UNUM_PAD_BEFORE_PREFIX) {
        length += addPaddingHelper(paddingCp, requiredPadding, string, leftIndex, status);
    } else if (position == UNUM_PAD_AFTER_SUFFIX) {
        length += addPaddingHelper(paddingCp, requiredPadding, string, rightIndex + length, status);
    }

    return length;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
