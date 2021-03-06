// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include <stdlib.h>
#include <cmath>
#include "number_decnum.h"
#include "number_types.h"
#include "number_utils.h"
#include "charstr.h"
#include "decContext.h"
#include "decNumber.h"
#include "double-conversion.h"
#include "fphdlimp.h"
#include "uresimp.h"
#include "ureslocs.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

using icu::double_conversion::DoubleToStringConverter;


namespace {

const char16_t*
doGetPattern(UResourceBundle* res, const char* nsName, const char* patternKey, UErrorCode& publicStatus,
             UErrorCode& localStatus) {
    // Construct the path into the resource bundle
    CharString key;
    key.append("NumberElements/", publicStatus);
    key.append(nsName, publicStatus);
    key.append("/patterns/", publicStatus);
    key.append(patternKey, publicStatus);
    if (U_FAILURE(publicStatus)) {
        return u"";
    }
    return ures_getStringByKeyWithFallback(res, key.data(), nullptr, &localStatus);
}

}


const char16_t* utils::getPatternForStyle(const Locale& locale, const char* nsName, CldrPatternStyle style,
                                          UErrorCode& status) {
    const char* patternKey;
    switch (style) {
        case CLDR_PATTERN_STYLE_DECIMAL:
            patternKey = "decimalFormat";
            break;
        case CLDR_PATTERN_STYLE_CURRENCY:
            patternKey = "currencyFormat";
            break;
        case CLDR_PATTERN_STYLE_ACCOUNTING:
            patternKey = "accountingFormat";
            break;
        case CLDR_PATTERN_STYLE_PERCENT:
            patternKey = "percentFormat";
            break;
        case CLDR_PATTERN_STYLE_SCIENTIFIC:
            patternKey = "scientificFormat";
            break;
        default:
            patternKey = "decimalFormat"; // silence compiler error
            UPRV_UNREACHABLE;
    }
    LocalUResourceBundlePointer res(ures_open(nullptr, locale.getName(), &status));
    if (U_FAILURE(status)) { return u""; }

    // Attempt to get the pattern with the native numbering system.
    UErrorCode localStatus = U_ZERO_ERROR;
    const char16_t* pattern;
    pattern = doGetPattern(res.getAlias(), nsName, patternKey, status, localStatus);
    if (U_FAILURE(status)) { return u""; }

    // Fall back to latn if native numbering system does not have the right pattern
    if (U_FAILURE(localStatus) && uprv_strcmp("latn", nsName) != 0) {
        localStatus = U_ZERO_ERROR;
        pattern = doGetPattern(res.getAlias(), "latn", patternKey, status, localStatus);
        if (U_FAILURE(status)) { return u""; }
    }

    return pattern;
}


DecNum::DecNum() {
    uprv_decContextDefault(&fContext, DEC_INIT_BASE);
    uprv_decContextSetRounding(&fContext, DEC_ROUND_HALF_EVEN);
    fContext.traps = 0; // no traps, thank you (what does this even mean?)
}

DecNum::DecNum(const DecNum& other, UErrorCode& status)
        : fContext(other.fContext) {
    // Allocate memory for the new DecNum.
    U_ASSERT(fContext.digits == other.fData.getCapacity());
    if (fContext.digits > kDefaultDigits) {
        void* p = fData.resize(fContext.digits, 0);
        if (p == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
    }

    // Copy the data from the old DecNum to the new one.
    uprv_memcpy(fData.getAlias(), other.fData.getAlias(), sizeof(decNumber));
    uprv_memcpy(fData.getArrayStart(),
            other.fData.getArrayStart(),
            other.fData.getArrayLimit() - other.fData.getArrayStart());
}

void DecNum::setTo(StringPiece str, UErrorCode& status) {
    // We need NUL-terminated for decNumber; CharString guarantees this, but not StringPiece.
    CharString cstr(str, status);
    if (U_FAILURE(status)) { return; }
    _setTo(cstr.data(), str.length(), status);
}

void DecNum::setTo(const char* str, UErrorCode& status) {
    _setTo(str, static_cast<int32_t>(uprv_strlen(str)), status);
}

void DecNum::setTo(double d, UErrorCode& status) {
    // Need to check for NaN and Infinity before going into DoubleToStringConverter
    if (std::isnan(d) != 0 || std::isfinite(d) == 0) {
        status = U_UNSUPPORTED_ERROR;
        return;
    }

    // First convert from double to string, then string to DecNum.
    // Allocate enough room for: all digits, "E-324", and NUL-terminator.
    char buffer[DoubleToStringConverter::kBase10MaximalLength + 6];
    bool sign; // unused; always positive
    int32_t length;
    int32_t point;
    DoubleToStringConverter::DoubleToAscii(
            d,
            DoubleToStringConverter::DtoaMode::SHORTEST,
            0,
            buffer,
            sizeof(buffer),
            &sign,
            &length,
            &point
    );

    // Read initial result as a string.
    _setTo(buffer, length, status);

    // Set exponent and bitmask. Note that DoubleToStringConverter does not do negatives.
    fData.getAlias()->exponent += point - length;
    fData.getAlias()->bits |= static_cast<uint8_t>(std::signbit(d) ? DECNEG : 0);
}

void DecNum::_setTo(const char* str, int32_t maxDigits, UErrorCode& status) {
    if (maxDigits > kDefaultDigits) {
        fData.resize(maxDigits, 0);
        fContext.digits = maxDigits;
    } else {
        fContext.digits = kDefaultDigits;
    }

    static_assert(DECDPUN == 1, "Assumes that DECDPUN is set to 1");
    uprv_decNumberFromString(fData.getAlias(), str, &fContext);

    // Check for invalid syntax and set the corresponding error code.
    if ((fContext.status & DEC_Conversion_syntax) != 0) {
        status = U_DECIMAL_NUMBER_SYNTAX_ERROR;
        return;
    } else if (fContext.status != 0) {
        // Not a syntax error, but some other error, like an exponent that is too large.
        status = U_UNSUPPORTED_ERROR;
        return;
    }

    // For consistency with Java BigDecimal, no support for DecNum that is NaN or Infinity!
    if (decNumberIsSpecial(fData.getAlias())) {
        status = U_UNSUPPORTED_ERROR;
        return;
    }
}

void
DecNum::setTo(const uint8_t* bcd, int32_t length, int32_t scale, bool isNegative, UErrorCode& status) {
    if (length > kDefaultDigits) {
        fData.resize(length, 0);
        fContext.digits = length;
    } else {
        fContext.digits = kDefaultDigits;
    }

    // "digits is of type int32_t, and must have a value in the range 1 through 999,999,999."
    if (length < 1 || length > 999999999) {
        // Too large for decNumber
        status = U_UNSUPPORTED_ERROR;
        return;
    }
    // "The exponent field holds the exponent of the number. Its range is limited by the requirement that
    // "the range of the adjusted exponent of the number be balanced and fit within a whole number of
    // "decimal digits (in this implementation, be –999,999,999 through +999,999,999). The adjusted
    // "exponent is the exponent that would result if the number were expressed with a single digit before
    // "the decimal point, and is therefore given by exponent+digits-1."
    if (scale > 999999999 - length + 1 || scale < -999999999 - length + 1) {
        // Too large for decNumber
        status = U_UNSUPPORTED_ERROR;
        return;
    }

    fData.getAlias()->digits = length;
    fData.getAlias()->exponent = scale;
    fData.getAlias()->bits = static_cast<uint8_t>(isNegative ? DECNEG : 0);
    uprv_decNumberSetBCD(fData, bcd, static_cast<uint32_t>(length));
    if (fContext.status != 0) {
        // Some error occurred while constructing the decNumber.
        status = U_INTERNAL_PROGRAM_ERROR;
    }
}

void DecNum::normalize() {
    uprv_decNumberReduce(fData, fData, &fContext);
}

void DecNum::multiplyBy(const DecNum& rhs, UErrorCode& status) {
    uprv_decNumberMultiply(fData, fData, rhs.fData, &fContext);
    if (fContext.status != 0) {
        status = U_INTERNAL_PROGRAM_ERROR;
    }
}

void DecNum::divideBy(const DecNum& rhs, UErrorCode& status) {
    uprv_decNumberDivide(fData, fData, rhs.fData, &fContext);
    if ((fContext.status & DEC_Inexact) != 0) {
        // Ignore.
    } else if (fContext.status != 0) {
        status = U_INTERNAL_PROGRAM_ERROR;
    }
}

bool DecNum::isNegative() const {
    return decNumberIsNegative(fData.getAlias());
}

bool DecNum::isZero() const {
    return decNumberIsZero(fData.getAlias());
}

void DecNum::toString(ByteSink& output, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return;
    }
    // "string must be at least dn->digits+14 characters long"
    int32_t minCapacity = fData.getAlias()->digits + 14;
    MaybeStackArray<char, 30> buffer(minCapacity, status);
    if (U_FAILURE(status)) {
        return;
    }
    uprv_decNumberToString(fData, buffer.getAlias());
    output.Append(buffer.getAlias(), static_cast<int32_t>(uprv_strlen(buffer.getAlias())));
}

#endif /* #if !UCONFIG_NO_FORMATTING */
