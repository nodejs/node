// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "uassert.h"
#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"
#include "double-conversion.h"
#include "number_roundingutils.h"
#include "putilimp.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;


using double_conversion::DoubleToStringConverter;

namespace {

int32_t getRoundingMagnitudeFraction(int maxFrac) {
    if (maxFrac == -1) {
        return INT32_MIN;
    }
    return -maxFrac;
}

int32_t getRoundingMagnitudeSignificant(const DecimalQuantity &value, int maxSig) {
    if (maxSig == -1) {
        return INT32_MIN;
    }
    int magnitude = value.isZero() ? 0 : value.getMagnitude();
    return magnitude - maxSig + 1;
}

int32_t getDisplayMagnitudeFraction(int minFrac) {
    if (minFrac == 0) {
        return INT32_MAX;
    }
    return -minFrac;
}

int32_t getDisplayMagnitudeSignificant(const DecimalQuantity &value, int minSig) {
    int magnitude = value.isZero() ? 0 : value.getMagnitude();
    return magnitude - minSig + 1;
}

}


MultiplierProducer::~MultiplierProducer() = default;


digits_t roundingutils::doubleFractionLength(double input) {
    char buffer[DoubleToStringConverter::kBase10MaximalLength + 1];
    bool sign; // unused; always positive
    int32_t length;
    int32_t point;
    DoubleToStringConverter::DoubleToAscii(
            input,
            DoubleToStringConverter::DtoaMode::SHORTEST,
            0,
            buffer,
            sizeof(buffer),
            &sign,
            &length,
            &point
    );

    return static_cast<digits_t>(length - point);
}


Precision Precision::unlimited() {
    return Precision(RND_NONE, {}, kDefaultMode);
}

FractionPrecision Precision::integer() {
    return constructFraction(0, 0);
}

FractionPrecision Precision::fixedFraction(int32_t minMaxFractionPlaces) {
    if (minMaxFractionPlaces >= 0 && minMaxFractionPlaces <= kMaxIntFracSig) {
        return constructFraction(minMaxFractionPlaces, minMaxFractionPlaces);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

FractionPrecision Precision::minFraction(int32_t minFractionPlaces) {
    if (minFractionPlaces >= 0 && minFractionPlaces <= kMaxIntFracSig) {
        return constructFraction(minFractionPlaces, -1);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

FractionPrecision Precision::maxFraction(int32_t maxFractionPlaces) {
    if (maxFractionPlaces >= 0 && maxFractionPlaces <= kMaxIntFracSig) {
        return constructFraction(0, maxFractionPlaces);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

FractionPrecision Precision::minMaxFraction(int32_t minFractionPlaces, int32_t maxFractionPlaces) {
    if (minFractionPlaces >= 0 && maxFractionPlaces <= kMaxIntFracSig &&
        minFractionPlaces <= maxFractionPlaces) {
        return constructFraction(minFractionPlaces, maxFractionPlaces);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

Precision Precision::fixedSignificantDigits(int32_t minMaxSignificantDigits) {
    if (minMaxSignificantDigits >= 1 && minMaxSignificantDigits <= kMaxIntFracSig) {
        return constructSignificant(minMaxSignificantDigits, minMaxSignificantDigits);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

Precision Precision::minSignificantDigits(int32_t minSignificantDigits) {
    if (minSignificantDigits >= 1 && minSignificantDigits <= kMaxIntFracSig) {
        return constructSignificant(minSignificantDigits, -1);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

Precision Precision::maxSignificantDigits(int32_t maxSignificantDigits) {
    if (maxSignificantDigits >= 1 && maxSignificantDigits <= kMaxIntFracSig) {
        return constructSignificant(1, maxSignificantDigits);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

Precision Precision::minMaxSignificantDigits(int32_t minSignificantDigits, int32_t maxSignificantDigits) {
    if (minSignificantDigits >= 1 && maxSignificantDigits <= kMaxIntFracSig &&
        minSignificantDigits <= maxSignificantDigits) {
        return constructSignificant(minSignificantDigits, maxSignificantDigits);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

IncrementPrecision Precision::increment(double roundingIncrement) {
    if (roundingIncrement > 0.0) {
        return constructIncrement(roundingIncrement, 0);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

CurrencyPrecision Precision::currency(UCurrencyUsage currencyUsage) {
    return constructCurrency(currencyUsage);
}

Precision Precision::withMode(RoundingMode roundingMode) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    Precision retval = *this;
    retval.fRoundingMode = roundingMode;
    return retval;
}

Precision FractionPrecision::withMinDigits(int32_t minSignificantDigits) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    if (minSignificantDigits >= 1 && minSignificantDigits <= kMaxIntFracSig) {
        return constructFractionSignificant(*this, minSignificantDigits, -1);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

Precision FractionPrecision::withMaxDigits(int32_t maxSignificantDigits) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    if (maxSignificantDigits >= 1 && maxSignificantDigits <= kMaxIntFracSig) {
        return constructFractionSignificant(*this, -1, maxSignificantDigits);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

// Private method on base class
Precision Precision::withCurrency(const CurrencyUnit &currency, UErrorCode &status) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    U_ASSERT(fType == RND_CURRENCY);
    const char16_t *isoCode = currency.getISOCurrency();
    double increment = ucurr_getRoundingIncrementForUsage(isoCode, fUnion.currencyUsage, &status);
    int32_t minMaxFrac = ucurr_getDefaultFractionDigitsForUsage(
            isoCode, fUnion.currencyUsage, &status);
    if (increment != 0.0) {
        return constructIncrement(increment, minMaxFrac);
    } else {
        return constructFraction(minMaxFrac, minMaxFrac);
    }
}

// Public method on CurrencyPrecision subclass
Precision CurrencyPrecision::withCurrency(const CurrencyUnit &currency) const {
    UErrorCode localStatus = U_ZERO_ERROR;
    Precision result = Precision::withCurrency(currency, localStatus);
    if (U_FAILURE(localStatus)) {
        return {localStatus};
    }
    return result;
}

Precision IncrementPrecision::withMinFraction(int32_t minFrac) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    if (minFrac >= 0 && minFrac <= kMaxIntFracSig) {
        return constructIncrement(fUnion.increment.fIncrement, minFrac);
    } else {
        return {U_NUMBER_ARG_OUTOFBOUNDS_ERROR};
    }
}

FractionPrecision Precision::constructFraction(int32_t minFrac, int32_t maxFrac) {
    FractionSignificantSettings settings;
    settings.fMinFrac = static_cast<digits_t>(minFrac);
    settings.fMaxFrac = static_cast<digits_t>(maxFrac);
    settings.fMinSig = -1;
    settings.fMaxSig = -1;
    PrecisionUnion union_;
    union_.fracSig = settings;
    return {RND_FRACTION, union_, kDefaultMode};
}

Precision Precision::constructSignificant(int32_t minSig, int32_t maxSig) {
    FractionSignificantSettings settings;
    settings.fMinFrac = -1;
    settings.fMaxFrac = -1;
    settings.fMinSig = static_cast<digits_t>(minSig);
    settings.fMaxSig = static_cast<digits_t>(maxSig);
    PrecisionUnion union_;
    union_.fracSig = settings;
    return {RND_SIGNIFICANT, union_, kDefaultMode};
}

Precision
Precision::constructFractionSignificant(const FractionPrecision &base, int32_t minSig, int32_t maxSig) {
    FractionSignificantSettings settings = base.fUnion.fracSig;
    settings.fMinSig = static_cast<digits_t>(minSig);
    settings.fMaxSig = static_cast<digits_t>(maxSig);
    PrecisionUnion union_;
    union_.fracSig = settings;
    return {RND_FRACTION_SIGNIFICANT, union_, kDefaultMode};
}

IncrementPrecision Precision::constructIncrement(double increment, int32_t minFrac) {
    IncrementSettings settings;
    settings.fIncrement = increment;
    settings.fMinFrac = static_cast<digits_t>(minFrac);
    // One of the few pre-computed quantities:
    // Note: it is possible for minFrac to be more than maxFrac... (misleading)
    settings.fMaxFrac = roundingutils::doubleFractionLength(increment);
    PrecisionUnion union_;
    union_.increment = settings;
    return {RND_INCREMENT, union_, kDefaultMode};
}

CurrencyPrecision Precision::constructCurrency(UCurrencyUsage usage) {
    PrecisionUnion union_;
    union_.currencyUsage = usage;
    return {RND_CURRENCY, union_, kDefaultMode};
}


RoundingImpl::RoundingImpl(const Precision& precision, UNumberFormatRoundingMode roundingMode,
                           const CurrencyUnit& currency, UErrorCode& status)
        : fPrecision(precision), fRoundingMode(roundingMode), fPassThrough(false) {
    if (precision.fType == Precision::RND_CURRENCY) {
        fPrecision = precision.withCurrency(currency, status);
    }
}

RoundingImpl RoundingImpl::passThrough() {
    RoundingImpl retval;
    retval.fPassThrough = true;
    return retval;
}

bool RoundingImpl::isSignificantDigits() const {
    return fPrecision.fType == Precision::RND_SIGNIFICANT;
}

int32_t
RoundingImpl::chooseMultiplierAndApply(impl::DecimalQuantity &input, const impl::MultiplierProducer &producer,
                                  UErrorCode &status) {
    // Do not call this method with zero.
    U_ASSERT(!input.isZero());

    // Perform the first attempt at rounding.
    int magnitude = input.getMagnitude();
    int multiplier = producer.getMultiplier(magnitude);
    input.adjustMagnitude(multiplier);
    apply(input, status);

    // If the number rounded to zero, exit.
    if (input.isZero() || U_FAILURE(status)) {
        return multiplier;
    }

    // If the new magnitude after rounding is the same as it was before rounding, then we are done.
    // This case applies to most numbers.
    if (input.getMagnitude() == magnitude + multiplier) {
        return multiplier;
    }

    // If the above case DIDN'T apply, then we have a case like 99.9 -> 100 or 999.9 -> 1000:
    // The number rounded up to the next magnitude. Check if the multiplier changes; if it doesn't,
    // we do not need to make any more adjustments.
    int _multiplier = producer.getMultiplier(magnitude + 1);
    if (multiplier == _multiplier) {
        return multiplier;
    }

    // We have a case like 999.9 -> 1000, where the correct output is "1K", not "1000".
    // Fix the magnitude and re-apply the rounding strategy.
    input.adjustMagnitude(_multiplier - multiplier);
    apply(input, status);
    return _multiplier;
}

/** This is the method that contains the actual rounding logic. */
void RoundingImpl::apply(impl::DecimalQuantity &value, UErrorCode& status) const {
    if (fPassThrough) {
        return;
    }
    switch (fPrecision.fType) {
        case Precision::RND_BOGUS:
        case Precision::RND_ERROR:
            // Errors should be caught before the apply() method is called
            status = U_INTERNAL_PROGRAM_ERROR;
            break;

        case Precision::RND_NONE:
            value.roundToInfinity();
            break;

        case Precision::RND_FRACTION:
            value.roundToMagnitude(
                    getRoundingMagnitudeFraction(fPrecision.fUnion.fracSig.fMaxFrac),
                    fRoundingMode,
                    status);
            value.setFractionLength(
                    uprv_max(0, -getDisplayMagnitudeFraction(fPrecision.fUnion.fracSig.fMinFrac)),
                    INT32_MAX);
            break;

        case Precision::RND_SIGNIFICANT:
            value.roundToMagnitude(
                    getRoundingMagnitudeSignificant(value, fPrecision.fUnion.fracSig.fMaxSig),
                    fRoundingMode,
                    status);
            value.setFractionLength(
                    uprv_max(0, -getDisplayMagnitudeSignificant(value, fPrecision.fUnion.fracSig.fMinSig)),
                    INT32_MAX);
            // Make sure that digits are displayed on zero.
            if (value.isZero() && fPrecision.fUnion.fracSig.fMinSig > 0) {
                value.setIntegerLength(1, INT32_MAX);
            }
            break;

        case Precision::RND_FRACTION_SIGNIFICANT: {
            int32_t displayMag = getDisplayMagnitudeFraction(fPrecision.fUnion.fracSig.fMinFrac);
            int32_t roundingMag = getRoundingMagnitudeFraction(fPrecision.fUnion.fracSig.fMaxFrac);
            if (fPrecision.fUnion.fracSig.fMinSig == -1) {
                // Max Sig override
                int32_t candidate = getRoundingMagnitudeSignificant(
                        value,
                        fPrecision.fUnion.fracSig.fMaxSig);
                roundingMag = uprv_max(roundingMag, candidate);
            } else {
                // Min Sig override
                int32_t candidate = getDisplayMagnitudeSignificant(
                        value,
                        fPrecision.fUnion.fracSig.fMinSig);
                roundingMag = uprv_min(roundingMag, candidate);
            }
            value.roundToMagnitude(roundingMag, fRoundingMode, status);
            value.setFractionLength(uprv_max(0, -displayMag), INT32_MAX);
            break;
        }

        case Precision::RND_INCREMENT:
            value.roundToIncrement(
                    fPrecision.fUnion.increment.fIncrement,
                    fRoundingMode,
                    fPrecision.fUnion.increment.fMaxFrac,
                    status);
            value.setFractionLength(fPrecision.fUnion.increment.fMinFrac, INT32_MAX);
            break;

        case Precision::RND_CURRENCY:
            // Call .withCurrency() before .apply()!
            U_ASSERT(false);
            break;
    }
}

void RoundingImpl::apply(impl::DecimalQuantity &value, int32_t minInt, UErrorCode /*status*/) {
    // This method is intended for the one specific purpose of helping print "00.000E0".
    U_ASSERT(isSignificantDigits());
    U_ASSERT(value.isZero());
    value.setFractionLength(fPrecision.fUnion.fracSig.fMinSig - minInt, INT32_MAX);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
