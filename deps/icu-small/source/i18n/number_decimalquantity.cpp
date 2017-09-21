// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "uassert.h"
#include <cmath>
#include "cmemory.h"
#include "decNumber.h"
#include <limits>
#include "number_decimalquantity.h"
#include "decContext.h"
#include "decNumber.h"
#include "number_roundingutils.h"
#include "unicode/plurrule.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

int8_t NEGATIVE_FLAG = 1;
int8_t INFINITY_FLAG = 2;
int8_t NAN_FLAG = 4;

static constexpr int32_t DEFAULT_DIGITS = 34;
typedef MaybeStackHeaderAndArray<decNumber, char, DEFAULT_DIGITS> DecNumberWithStorage;

/** Helper function to convert a decNumber-compatible string into a decNumber. */
void stringToDecNumber(StringPiece n, DecNumberWithStorage &dn) {
    decContext set;
    uprv_decContextDefault(&set, DEC_INIT_BASE);
    uprv_decContextSetRounding(&set, DEC_ROUND_HALF_EVEN);
    set.traps = 0; // no traps, thank you
    if (n.length() > DEFAULT_DIGITS) {
        dn.resize(n.length(), 0);
        set.digits = n.length();
    } else {
        set.digits = DEFAULT_DIGITS;
    }
    uprv_decNumberFromString(dn.getAlias(), n.data(), &set);
    U_ASSERT(DECDPUN == 1);
}

/** Helper function for safe subtraction (no overflow). */
inline int32_t safeSubtract(int32_t a, int32_t b) {
    // Note: In C++, signed integer subtraction is undefined behavior.
    int32_t diff = static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
    if (b < 0 && diff < a) { return INT32_MAX; }
    if (b > 0 && diff > a) { return INT32_MIN; }
    return diff;
}

static double DOUBLE_MULTIPLIERS[] = {
        1e0,
        1e1,
        1e2,
        1e3,
        1e4,
        1e5,
        1e6,
        1e7,
        1e8,
        1e9,
        1e10,
        1e11,
        1e12,
        1e13,
        1e14,
        1e15,
        1e16,
        1e17,
        1e18,
        1e19,
        1e20,
        1e21};

}  // namespace


DecimalQuantity::DecimalQuantity() {
    setBcdToZero();
    flags = 0;
}

DecimalQuantity::~DecimalQuantity() {
    if (usingBytes) {
        uprv_free(fBCD.bcdBytes.ptr);
        fBCD.bcdBytes.ptr = nullptr;
        usingBytes = false;
    }
}

DecimalQuantity::DecimalQuantity(const DecimalQuantity &other) {
    *this = other;
}

DecimalQuantity &DecimalQuantity::operator=(const DecimalQuantity &other) {
    if (this == &other) {
        return *this;
    }
    copyBcdFrom(other);
    lOptPos = other.lOptPos;
    lReqPos = other.lReqPos;
    rReqPos = other.rReqPos;
    rOptPos = other.rOptPos;
    scale = other.scale;
    precision = other.precision;
    flags = other.flags;
    origDouble = other.origDouble;
    origDelta = other.origDelta;
    isApproximate = other.isApproximate;
    return *this;
}

void DecimalQuantity::clear() {
    lOptPos = INT32_MAX;
    lReqPos = 0;
    rReqPos = 0;
    rOptPos = INT32_MIN;
    flags = 0;
    setBcdToZero(); // sets scale, precision, hasDouble, origDouble, origDelta, and BCD data
}

void DecimalQuantity::setIntegerLength(int32_t minInt, int32_t maxInt) {
    // Validation should happen outside of DecimalQuantity, e.g., in the Rounder class.
    U_ASSERT(minInt >= 0);
    U_ASSERT(maxInt >= minInt);

    // Save values into internal state
    // Negation is safe for minFrac/maxFrac because -Integer.MAX_VALUE > Integer.MIN_VALUE
    lOptPos = maxInt;
    lReqPos = minInt;
}

void DecimalQuantity::setFractionLength(int32_t minFrac, int32_t maxFrac) {
    // Validation should happen outside of DecimalQuantity, e.g., in the Rounder class.
    U_ASSERT(minFrac >= 0);
    U_ASSERT(maxFrac >= minFrac);

    // Save values into internal state
    // Negation is safe for minFrac/maxFrac because -Integer.MAX_VALUE > Integer.MIN_VALUE
    rReqPos = -minFrac;
    rOptPos = -maxFrac;
}

uint64_t DecimalQuantity::getPositionFingerprint() const {
    uint64_t fingerprint = 0;
    fingerprint ^= lOptPos;
    fingerprint ^= (lReqPos << 16);
    fingerprint ^= (static_cast<uint64_t>(rReqPos) << 32);
    fingerprint ^= (static_cast<uint64_t>(rOptPos) << 48);
    return fingerprint;
}

void DecimalQuantity::roundToIncrement(double roundingIncrement, RoundingMode roundingMode,
                                       int32_t minMaxFrac, UErrorCode& status) {
    // TODO: This is innefficient.  Improve?
    // TODO: Should we convert to decNumber instead?
    double temp = toDouble();
    temp /= roundingIncrement;
    setToDouble(temp);
    roundToMagnitude(0, roundingMode, status);
    temp = toDouble();
    temp *= roundingIncrement;
    setToDouble(temp);
    // Since we reset the value to a double, we need to specify the rounding boundary
    // in order to get the DecimalQuantity out of approximation mode.
    roundToMagnitude(-minMaxFrac, roundingMode, status);
}

void DecimalQuantity::multiplyBy(int32_t multiplicand) {
    if (isInfinite() || isZero() || isNaN()) {
        return;
    }
    // TODO: Should we convert to decNumber instead?
    double temp = toDouble();
    temp *= multiplicand;
    setToDouble(temp);
}

int32_t DecimalQuantity::getMagnitude() const {
    U_ASSERT(precision != 0);
    return scale + precision - 1;
}

void DecimalQuantity::adjustMagnitude(int32_t delta) {
    if (precision != 0) {
        scale += delta;
        origDelta += delta;
    }
}

StandardPlural::Form DecimalQuantity::getStandardPlural(const PluralRules *rules) const {
    if (rules == nullptr) {
        // Fail gracefully if the user didn't provide a PluralRules
        return StandardPlural::Form::OTHER;
    } else {
        UnicodeString ruleString = rules->select(*this);
        return StandardPlural::orOtherFromString(ruleString);
    }
}

double DecimalQuantity::getPluralOperand(PluralOperand operand) const {
    // If this assertion fails, you need to call roundToInfinity() or some other rounding method.
    // See the comment at the top of this file explaining the "isApproximate" field.
    U_ASSERT(!isApproximate);

    switch (operand) {
        case PLURAL_OPERAND_I:
            return static_cast<double>(toLong());
        case PLURAL_OPERAND_F:
            return static_cast<double>(toFractionLong(true));
        case PLURAL_OPERAND_T:
            return static_cast<double>(toFractionLong(false));
        case PLURAL_OPERAND_V:
            return fractionCount();
        case PLURAL_OPERAND_W:
            return fractionCountWithoutTrailingZeros();
        default:
            return std::abs(toDouble());
    }
}

int32_t DecimalQuantity::getUpperDisplayMagnitude() const {
    // If this assertion fails, you need to call roundToInfinity() or some other rounding method.
    // See the comment in the header file explaining the "isApproximate" field.
    U_ASSERT(!isApproximate);

    int32_t magnitude = scale + precision;
    int32_t result = (lReqPos > magnitude) ? lReqPos : (lOptPos < magnitude) ? lOptPos : magnitude;
    return result - 1;
}

int32_t DecimalQuantity::getLowerDisplayMagnitude() const {
    // If this assertion fails, you need to call roundToInfinity() or some other rounding method.
    // See the comment in the header file explaining the "isApproximate" field.
    U_ASSERT(!isApproximate);

    int32_t magnitude = scale;
    int32_t result = (rReqPos < magnitude) ? rReqPos : (rOptPos > magnitude) ? rOptPos : magnitude;
    return result;
}

int8_t DecimalQuantity::getDigit(int32_t magnitude) const {
    // If this assertion fails, you need to call roundToInfinity() or some other rounding method.
    // See the comment at the top of this file explaining the "isApproximate" field.
    U_ASSERT(!isApproximate);

    return getDigitPos(magnitude - scale);
}

int32_t DecimalQuantity::fractionCount() const {
    return -getLowerDisplayMagnitude();
}

int32_t DecimalQuantity::fractionCountWithoutTrailingZeros() const {
    return -scale > 0 ? -scale : 0;  // max(-scale, 0)
}

bool DecimalQuantity::isNegative() const {
    return (flags & NEGATIVE_FLAG) != 0;
}

bool DecimalQuantity::isInfinite() const {
    return (flags & INFINITY_FLAG) != 0;
}

bool DecimalQuantity::isNaN() const {
    return (flags & NAN_FLAG) != 0;
}

bool DecimalQuantity::isZero() const {
    return precision == 0;
}

DecimalQuantity &DecimalQuantity::setToInt(int32_t n) {
    setBcdToZero();
    flags = 0;
    if (n < 0) {
        flags |= NEGATIVE_FLAG;
        n = -n;
    }
    if (n != 0) {
        _setToInt(n);
        compact();
    }
    return *this;
}

void DecimalQuantity::_setToInt(int32_t n) {
    if (n == INT32_MIN) {
        readLongToBcd(-static_cast<int64_t>(n));
    } else {
        readIntToBcd(n);
    }
}

DecimalQuantity &DecimalQuantity::setToLong(int64_t n) {
    setBcdToZero();
    flags = 0;
    if (n < 0) {
        flags |= NEGATIVE_FLAG;
        n = -n;
    }
    if (n != 0) {
        _setToLong(n);
        compact();
    }
    return *this;
}

void DecimalQuantity::_setToLong(int64_t n) {
    if (n == INT64_MIN) {
        static const char *int64minStr = "9.223372036854775808E+18";
        DecNumberWithStorage dn;
        stringToDecNumber(int64minStr, dn);
        readDecNumberToBcd(dn.getAlias());
    } else if (n <= INT32_MAX) {
        readIntToBcd(static_cast<int32_t>(n));
    } else {
        readLongToBcd(n);
    }
}

DecimalQuantity &DecimalQuantity::setToDouble(double n) {
    setBcdToZero();
    flags = 0;
    // signbit() from <math.h> handles +0.0 vs -0.0
    if (std::signbit(n) != 0) {
        flags |= NEGATIVE_FLAG;
        n = -n;
    }
    if (std::isnan(n) != 0) {
        flags |= NAN_FLAG;
    } else if (std::isfinite(n) == 0) {
        flags |= INFINITY_FLAG;
    } else if (n != 0) {
        _setToDoubleFast(n);
        compact();
    }
    return *this;
}

void DecimalQuantity::_setToDoubleFast(double n) {
    isApproximate = true;
    origDouble = n;
    origDelta = 0;

    // Make sure the double is an IEEE 754 double.  If not, fall back to the slow path right now.
    // TODO: Make a fast path for other types of doubles.
    if (!std::numeric_limits<double>::is_iec559) {
        convertToAccurateDouble();
        // Turn off the approximate double flag, since the value is now exact.
        isApproximate = false;
        origDouble = 0.0;
        return;
    }

    // To get the bits from the double, use memcpy, which takes care of endianness.
    uint64_t ieeeBits;
    uprv_memcpy(&ieeeBits, &n, sizeof(n));
    int32_t exponent = static_cast<int32_t>((ieeeBits & 0x7ff0000000000000L) >> 52) - 0x3ff;

    // Not all integers can be represented exactly for exponent > 52
    if (exponent <= 52 && static_cast<int64_t>(n) == n) {
        _setToLong(static_cast<int64_t>(n));
        return;
    }

    // 3.3219... is log2(10)
    auto fracLength = static_cast<int32_t> ((52 - exponent) / 3.32192809489);
    if (fracLength >= 0) {
        int32_t i = fracLength;
        // 1e22 is the largest exact double.
        for (; i >= 22; i -= 22) n *= 1e22;
        n *= DOUBLE_MULTIPLIERS[i];
    } else {
        int32_t i = fracLength;
        // 1e22 is the largest exact double.
        for (; i <= -22; i += 22) n /= 1e22;
        n /= DOUBLE_MULTIPLIERS[-i];
    }
    auto result = static_cast<int64_t>(std::round(n));
    if (result != 0) {
        _setToLong(result);
        scale -= fracLength;
    }
}

void DecimalQuantity::convertToAccurateDouble() {
    double n = origDouble;
    U_ASSERT(n != 0);
    int32_t delta = origDelta;
    setBcdToZero();

    // Call the slow oracle function (Double.toString in Java, sprintf in C++).
    // The <float.h> constant DBL_DIG defines a platform-specific number of digits in a double.
    // However, this tends to be too low (see #11318). Instead, we always use 14 decimal places.
    static constexpr size_t CAP = 1 + 14 + 8; // Extra space for '+', '.', e+NNN, and '\0'
    char dstr[CAP];
    snprintf(dstr, CAP, "%+1.14e", n);

    // uprv_decNumberFromString() will parse the string expecting '.' as a
    // decimal separator, however sprintf() can use ',' in certain locales.
    // Overwrite a ',' with '.' here before proceeding.
    char *decimalSeparator = strchr(dstr, ',');
    if (decimalSeparator != nullptr) {
        *decimalSeparator = '.';
    }

    StringPiece sp(dstr);
    DecNumberWithStorage dn;
    stringToDecNumber(dstr, dn);
    _setToDecNumber(dn.getAlias());

    scale += delta;
    explicitExactDouble = true;
}

DecimalQuantity &DecimalQuantity::setToDecNumber(StringPiece n) {
    setBcdToZero();
    flags = 0;

    DecNumberWithStorage dn;
    stringToDecNumber(n, dn);

    // The code path for decNumber is modeled after BigDecimal in Java.
    if (decNumberIsNegative(dn.getAlias())) {
        flags |= NEGATIVE_FLAG;
    }
    if (!decNumberIsZero(dn.getAlias())) {
        _setToDecNumber(dn.getAlias());
    }
    return *this;
}

void DecimalQuantity::_setToDecNumber(decNumber *n) {
    // Java fastpaths for ints here. In C++, just always read directly from the decNumber.
    readDecNumberToBcd(n);
    compact();
}

int64_t DecimalQuantity::toLong() const {
    int64_t result = 0L;
    for (int32_t magnitude = scale + precision - 1; magnitude >= 0; magnitude--) {
        result = result * 10 + getDigitPos(magnitude - scale);
    }
    return result;
}

int64_t DecimalQuantity::toFractionLong(bool includeTrailingZeros) const {
    int64_t result = 0L;
    int32_t magnitude = -1;
    for (; (magnitude >= scale || (includeTrailingZeros && magnitude >= rReqPos)) &&
           magnitude >= rOptPos; magnitude--) {
        result = result * 10 + getDigitPos(magnitude - scale);
    }
    return result;
}

double DecimalQuantity::toDouble() const {
    if (isApproximate) {
        return toDoubleFromOriginal();
    }

    if (isNaN()) {
        return NAN;
    } else if (isInfinite()) {
        return isNegative() ? -INFINITY : INFINITY;
    }

    int64_t tempLong = 0L;
    int32_t lostDigits = precision - (precision < 17 ? precision : 17);
    for (int shift = precision - 1; shift >= lostDigits; shift--) {
        tempLong = tempLong * 10 + getDigitPos(shift);
    }
    double result = static_cast<double>(tempLong);
    int32_t _scale = scale + lostDigits;
    if (_scale >= 0) {
        // 1e22 is the largest exact double.
        int32_t i = _scale;
        for (; i >= 22; i -= 22) result *= 1e22;
        result *= DOUBLE_MULTIPLIERS[i];
    } else {
        // 1e22 is the largest exact double.
        int32_t i = _scale;
        for (; i <= -22; i += 22) result /= 1e22;
        result /= DOUBLE_MULTIPLIERS[-i];
    }
    if (isNegative()) { result = -result; }
    return result;
}

double DecimalQuantity::toDoubleFromOriginal() const {
    double result = origDouble;
    int32_t delta = origDelta;
    if (delta >= 0) {
        // 1e22 is the largest exact double.
        for (; delta >= 22; delta -= 22) result *= 1e22;
        result *= DOUBLE_MULTIPLIERS[delta];
    } else {
        // 1e22 is the largest exact double.
        for (; delta <= -22; delta += 22) result /= 1e22;
        result /= DOUBLE_MULTIPLIERS[-delta];
    }
    if (isNegative()) { result *= -1; }
    return result;
}

void DecimalQuantity::roundToMagnitude(int32_t magnitude, RoundingMode roundingMode, UErrorCode& status) {
    // The position in the BCD at which rounding will be performed; digits to the right of position
    // will be rounded away.
    // TODO: Andy: There was a test failure because of integer overflow here. Should I do
    // "safe subtraction" everywhere in the code?  What's the nicest way to do it?
    int position = safeSubtract(magnitude, scale);

    if (position <= 0 && !isApproximate) {
        // All digits are to the left of the rounding magnitude.
    } else if (precision == 0) {
        // No rounding for zero.
    } else {
        // Perform rounding logic.
        // "leading" = most significant digit to the right of rounding
        // "trailing" = least significant digit to the left of rounding
        int8_t leadingDigit = getDigitPos(safeSubtract(position, 1));
        int8_t trailingDigit = getDigitPos(position);

        // Compute which section of the number we are in.
        // EDGE means we are at the bottom or top edge, like 1.000 or 1.999 (used by doubles)
        // LOWER means we are between the bottom edge and the midpoint, like 1.391
        // MIDPOINT means we are exactly in the middle, like 1.500
        // UPPER means we are between the midpoint and the top edge, like 1.916
        roundingutils::Section section = roundingutils::SECTION_MIDPOINT;
        if (!isApproximate) {
            if (leadingDigit < 5) {
                section = roundingutils::SECTION_LOWER;
            } else if (leadingDigit > 5) {
                section = roundingutils::SECTION_UPPER;
            } else {
                for (int p = safeSubtract(position, 2); p >= 0; p--) {
                    if (getDigitPos(p) != 0) {
                        section = roundingutils::SECTION_UPPER;
                        break;
                    }
                }
            }
        } else {
            int32_t p = safeSubtract(position, 2);
            int32_t minP = uprv_max(0, precision - 14);
            if (leadingDigit == 0) {
                section = roundingutils::SECTION_LOWER_EDGE;
                for (; p >= minP; p--) {
                    if (getDigitPos(p) != 0) {
                        section = roundingutils::SECTION_LOWER;
                        break;
                    }
                }
            } else if (leadingDigit == 4) {
                for (; p >= minP; p--) {
                    if (getDigitPos(p) != 9) {
                        section = roundingutils::SECTION_LOWER;
                        break;
                    }
                }
            } else if (leadingDigit == 5) {
                for (; p >= minP; p--) {
                    if (getDigitPos(p) != 0) {
                        section = roundingutils::SECTION_UPPER;
                        break;
                    }
                }
            } else if (leadingDigit == 9) {
                section = roundingutils::SECTION_UPPER_EDGE;
                for (; p >= minP; p--) {
                    if (getDigitPos(p) != 9) {
                        section = roundingutils::SECTION_UPPER;
                        break;
                    }
                }
            } else if (leadingDigit < 5) {
                section = roundingutils::SECTION_LOWER;
            } else {
                section = roundingutils::SECTION_UPPER;
            }

            bool roundsAtMidpoint = roundingutils::roundsAtMidpoint(roundingMode);
            if (safeSubtract(position, 1) < precision - 14 ||
                (roundsAtMidpoint && section == roundingutils::SECTION_MIDPOINT) ||
                (!roundsAtMidpoint && section < 0 /* i.e. at upper or lower edge */)) {
                // Oops! This means that we have to get the exact representation of the double, because
                // the zone of uncertainty is along the rounding boundary.
                convertToAccurateDouble();
                roundToMagnitude(magnitude, roundingMode, status); // start over
                return;
            }

            // Turn off the approximate double flag, since the value is now confirmed to be exact.
            isApproximate = false;
            origDouble = 0.0;
            origDelta = 0;

            if (position <= 0) {
                // All digits are to the left of the rounding magnitude.
                return;
            }

            // Good to continue rounding.
            if (section == -1) { section = roundingutils::SECTION_LOWER; }
            if (section == -2) { section = roundingutils::SECTION_UPPER; }
        }

        bool roundDown = roundingutils::getRoundingDirection((trailingDigit % 2) == 0,
                isNegative(),
                section,
                roundingMode,
                status);
        if (U_FAILURE(status)) {
            return;
        }

        // Perform truncation
        if (position >= precision) {
            setBcdToZero();
            scale = magnitude;
        } else {
            shiftRight(position);
        }

        // Bubble the result to the higher digits
        if (!roundDown) {
            if (trailingDigit == 9) {
                int bubblePos = 0;
                // Note: in the long implementation, the most digits BCD can have at this point is 15,
                // so bubblePos <= 15 and getDigitPos(bubblePos) is safe.
                for (; getDigitPos(bubblePos) == 9; bubblePos++) {}
                shiftRight(bubblePos); // shift off the trailing 9s
            }
            int8_t digit0 = getDigitPos(0);
            U_ASSERT(digit0 != 9);
            setDigitPos(0, static_cast<int8_t>(digit0 + 1));
            precision += 1; // in case an extra digit got added
        }

        compact();
    }
}

void DecimalQuantity::roundToInfinity() {
    if (isApproximate) {
        convertToAccurateDouble();
    }
}

void DecimalQuantity::appendDigit(int8_t value, int32_t leadingZeros, bool appendAsInteger) {
    U_ASSERT(leadingZeros >= 0);

    // Zero requires special handling to maintain the invariant that the least-significant digit
    // in the BCD is nonzero.
    if (value == 0) {
        if (appendAsInteger && precision != 0) {
            scale += leadingZeros + 1;
        }
        return;
    }

    // Deal with trailing zeros
    if (scale > 0) {
        leadingZeros += scale;
        if (appendAsInteger) {
            scale = 0;
        }
    }

    // Append digit
    shiftLeft(leadingZeros + 1);
    setDigitPos(0, value);

    // Fix scale if in integer mode
    if (appendAsInteger) {
        scale += leadingZeros + 1;
    }
}

UnicodeString DecimalQuantity::toPlainString() const {
    UnicodeString sb;
    if (isNegative()) {
        sb.append(u'-');
    }
    for (int m = getUpperDisplayMagnitude(); m >= getLowerDisplayMagnitude(); m--) {
        sb.append(getDigit(m) + u'0');
        if (m == 0) { sb.append(u'.'); }
    }
    return sb;
}

////////////////////////////////////////////////////
/// End of DecimalQuantity_AbstractBCD.java      ///
/// Start of DecimalQuantity_DualStorageBCD.java ///
////////////////////////////////////////////////////

int8_t DecimalQuantity::getDigitPos(int32_t position) const {
    if (usingBytes) {
        if (position < 0 || position > precision) { return 0; }
        return fBCD.bcdBytes.ptr[position];
    } else {
        if (position < 0 || position >= 16) { return 0; }
        return (int8_t) ((fBCD.bcdLong >> (position * 4)) & 0xf);
    }
}

void DecimalQuantity::setDigitPos(int32_t position, int8_t value) {
    U_ASSERT(position >= 0);
    if (usingBytes) {
        ensureCapacity(position + 1);
        fBCD.bcdBytes.ptr[position] = value;
    } else if (position >= 16) {
        switchStorage();
        ensureCapacity(position + 1);
        fBCD.bcdBytes.ptr[position] = value;
    } else {
        int shift = position * 4;
        fBCD.bcdLong = (fBCD.bcdLong & ~(0xfL << shift)) | ((long) value << shift);
    }
}

void DecimalQuantity::shiftLeft(int32_t numDigits) {
    if (!usingBytes && precision + numDigits > 16) {
        switchStorage();
    }
    if (usingBytes) {
        ensureCapacity(precision + numDigits);
        int i = precision + numDigits - 1;
        for (; i >= numDigits; i--) {
            fBCD.bcdBytes.ptr[i] = fBCD.bcdBytes.ptr[i - numDigits];
        }
        for (; i >= 0; i--) {
            fBCD.bcdBytes.ptr[i] = 0;
        }
    } else {
        fBCD.bcdLong <<= (numDigits * 4);
    }
    scale -= numDigits;
    precision += numDigits;
}

void DecimalQuantity::shiftRight(int32_t numDigits) {
    if (usingBytes) {
        int i = 0;
        for (; i < precision - numDigits; i++) {
            fBCD.bcdBytes.ptr[i] = fBCD.bcdBytes.ptr[i + numDigits];
        }
        for (; i < precision; i++) {
            fBCD.bcdBytes.ptr[i] = 0;
        }
    } else {
        fBCD.bcdLong >>= (numDigits * 4);
    }
    scale += numDigits;
    precision -= numDigits;
}

void DecimalQuantity::setBcdToZero() {
    if (usingBytes) {
        uprv_free(fBCD.bcdBytes.ptr);
        fBCD.bcdBytes.ptr = nullptr;
        usingBytes = false;
    }
    fBCD.bcdLong = 0L;
    scale = 0;
    precision = 0;
    isApproximate = false;
    origDouble = 0;
    origDelta = 0;
}

void DecimalQuantity::readIntToBcd(int32_t n) {
    U_ASSERT(n != 0);
    // ints always fit inside the long implementation.
    uint64_t result = 0L;
    int i = 16;
    for (; n != 0; n /= 10, i--) {
        result = (result >> 4) + ((static_cast<uint64_t>(n) % 10) << 60);
    }
    U_ASSERT(!usingBytes);
    fBCD.bcdLong = result >> (i * 4);
    scale = 0;
    precision = 16 - i;
}

void DecimalQuantity::readLongToBcd(int64_t n) {
    U_ASSERT(n != 0);
    if (n >= 10000000000000000L) {
        ensureCapacity();
        int i = 0;
        for (; n != 0L; n /= 10L, i++) {
            fBCD.bcdBytes.ptr[i] = static_cast<int8_t>(n % 10);
        }
        U_ASSERT(usingBytes);
        scale = 0;
        precision = i;
    } else {
        uint64_t result = 0L;
        int i = 16;
        for (; n != 0L; n /= 10L, i--) {
            result = (result >> 4) + ((n % 10) << 60);
        }
        U_ASSERT(i >= 0);
        U_ASSERT(!usingBytes);
        fBCD.bcdLong = result >> (i * 4);
        scale = 0;
        precision = 16 - i;
    }
}

void DecimalQuantity::readDecNumberToBcd(decNumber *dn) {
    if (dn->digits > 16) {
        ensureCapacity(dn->digits);
        for (int32_t i = 0; i < dn->digits; i++) {
            fBCD.bcdBytes.ptr[i] = dn->lsu[i];
        }
    } else {
        uint64_t result = 0L;
        for (int32_t i = 0; i < dn->digits; i++) {
            result |= static_cast<uint64_t>(dn->lsu[i]) << (4 * i);
        }
        fBCD.bcdLong = result;
    }
    scale = dn->exponent;
    precision = dn->digits;
}

void DecimalQuantity::compact() {
    if (usingBytes) {
        int32_t delta = 0;
        for (; delta < precision && fBCD.bcdBytes.ptr[delta] == 0; delta++);
        if (delta == precision) {
            // Number is zero
            setBcdToZero();
            return;
        } else {
            // Remove trailing zeros
            shiftRight(delta);
        }

        // Compute precision
        int32_t leading = precision - 1;
        for (; leading >= 0 && fBCD.bcdBytes.ptr[leading] == 0; leading--);
        precision = leading + 1;

        // Switch storage mechanism if possible
        if (precision <= 16) {
            switchStorage();
        }

    } else {
        if (fBCD.bcdLong == 0L) {
            // Number is zero
            setBcdToZero();
            return;
        }

        // Compact the number (remove trailing zeros)
        // TODO: Use a more efficient algorithm here and below. There is a logarithmic one.
        int32_t delta = 0;
        for (; delta < precision && getDigitPos(delta) == 0; delta++);
        fBCD.bcdLong >>= delta * 4;
        scale += delta;

        // Compute precision
        int32_t leading = precision - 1;
        for (; leading >= 0 && getDigitPos(leading) == 0; leading--);
        precision = leading + 1;
    }
}

void DecimalQuantity::ensureCapacity() {
    ensureCapacity(40);
}

void DecimalQuantity::ensureCapacity(int32_t capacity) {
    if (capacity == 0) { return; }
    int32_t oldCapacity = usingBytes ? fBCD.bcdBytes.len : 0;
    if (!usingBytes) {
        // TODO: There is nothing being done to check for memory allocation failures.
        // TODO: Consider indexing by nybbles instead of bytes in C++, so that we can
        // make these arrays half the size.
        fBCD.bcdBytes.ptr = static_cast<int8_t*>(uprv_malloc(capacity * sizeof(int8_t)));
        fBCD.bcdBytes.len = capacity;
        // Initialize the byte array to zeros (this is done automatically in Java)
        uprv_memset(fBCD.bcdBytes.ptr, 0, capacity * sizeof(int8_t));
    } else if (oldCapacity < capacity) {
        auto bcd1 = static_cast<int8_t*>(uprv_malloc(capacity * 2 * sizeof(int8_t)));
        uprv_memcpy(bcd1, fBCD.bcdBytes.ptr, oldCapacity * sizeof(int8_t));
        // Initialize the rest of the byte array to zeros (this is done automatically in Java)
        uprv_memset(fBCD.bcdBytes.ptr + oldCapacity, 0, (capacity - oldCapacity) * sizeof(int8_t));
        uprv_free(fBCD.bcdBytes.ptr);
        fBCD.bcdBytes.ptr = bcd1;
        fBCD.bcdBytes.len = capacity * 2;
    }
    usingBytes = true;
}

void DecimalQuantity::switchStorage() {
    if (usingBytes) {
        // Change from bytes to long
        uint64_t bcdLong = 0L;
        for (int i = precision - 1; i >= 0; i--) {
            bcdLong <<= 4;
            bcdLong |= fBCD.bcdBytes.ptr[i];
        }
        uprv_free(fBCD.bcdBytes.ptr);
        fBCD.bcdBytes.ptr = nullptr;
        fBCD.bcdLong = bcdLong;
        usingBytes = false;
    } else {
        // Change from long to bytes
        // Copy the long into a local variable since it will get munged when we allocate the bytes
        uint64_t bcdLong = fBCD.bcdLong;
        ensureCapacity();
        for (int i = 0; i < precision; i++) {
            fBCD.bcdBytes.ptr[i] = static_cast<int8_t>(bcdLong & 0xf);
            bcdLong >>= 4;
        }
        U_ASSERT(usingBytes);
    }
}

void DecimalQuantity::copyBcdFrom(const DecimalQuantity &other) {
    setBcdToZero();
    if (other.usingBytes) {
        ensureCapacity(other.precision);
        uprv_memcpy(fBCD.bcdBytes.ptr, other.fBCD.bcdBytes.ptr, other.precision * sizeof(int8_t));
    } else {
        fBCD.bcdLong = other.fBCD.bcdLong;
    }
}

const char16_t* DecimalQuantity::checkHealth() const {
    if (usingBytes) {
        if (precision == 0) { return u"Zero precision but we are in byte mode"; }
        int32_t capacity = fBCD.bcdBytes.len;
        if (precision > capacity) { return u"Precision exceeds length of byte array"; }
        if (getDigitPos(precision - 1) == 0) { return u"Most significant digit is zero in byte mode"; }
        if (getDigitPos(0) == 0) { return u"Least significant digit is zero in long mode"; }
        for (int i = 0; i < precision; i++) {
            if (getDigitPos(i) >= 10) { return u"Digit exceeding 10 in byte array"; }
            if (getDigitPos(i) < 0) { return u"Digit below 0 in byte array"; }
        }
        for (int i = precision; i < capacity; i++) {
            if (getDigitPos(i) != 0) { return u"Nonzero digits outside of range in byte array"; }
        }
    } else {
        if (precision == 0 && fBCD.bcdLong != 0) {
            return u"Value in bcdLong even though precision is zero";
        }
        if (precision > 16) { return u"Precision exceeds length of long"; }
        if (precision != 0 && getDigitPos(precision - 1) == 0) {
            return u"Most significant digit is zero in long mode";
        }
        if (precision != 0 && getDigitPos(0) == 0) {
            return u"Least significant digit is zero in long mode";
        }
        for (int i = 0; i < precision; i++) {
            if (getDigitPos(i) >= 10) { return u"Digit exceeding 10 in long"; }
            if (getDigitPos(i) < 0) { return u"Digit below 0 in long (?!)"; }
        }
        for (int i = precision; i < 16; i++) {
            if (getDigitPos(i) != 0) { return u"Nonzero digits outside of range in long"; }
        }
    }

    // No error
    return nullptr;
}

UnicodeString DecimalQuantity::toString() const {
    MaybeStackArray<char, 30> digits(precision + 1);
    for (int32_t i = 0; i < precision; i++) {
        digits[i] = getDigitPos(precision - i - 1) + '0';
    }
    digits[precision] = 0; // terminate buffer
    char buffer8[100];
    snprintf(
            buffer8,
            sizeof(buffer8),
            "<DecimalQuantity %d:%d:%d:%d %s %s%s%d>",
            (lOptPos > 999 ? 999 : lOptPos),
            lReqPos,
            rReqPos,
            (rOptPos < -999 ? -999 : rOptPos),
            (usingBytes ? "bytes" : "long"),
            (precision == 0 ? "0" : digits.getAlias()),
            "E",
            scale);
    return UnicodeString(buffer8, -1, US_INV);
}

UnicodeString DecimalQuantity::toNumberString() const {
    MaybeStackArray<char, 30> digits(precision + 11);
    for (int32_t i = 0; i < precision; i++) {
        digits[i] = getDigitPos(precision - i - 1) + '0';
    }
    snprintf(digits.getAlias() + precision, 11, "E%d", scale);
    return UnicodeString(digits.getAlias(), -1, US_INV);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
