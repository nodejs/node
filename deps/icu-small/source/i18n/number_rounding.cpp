// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "uassert.h"
#include "unicode/numberformatter.h"
#include "number_types.h"
#include "number_decimalquantity.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

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


Rounder Rounder::unlimited() {
    return Rounder(RND_NONE, {}, kDefaultMode);
}

FractionRounder Rounder::integer() {
    return constructFraction(0, 0);
}

FractionRounder Rounder::fixedFraction(int32_t minMaxFractionPlaces) {
    if (minMaxFractionPlaces >= 0 && minMaxFractionPlaces <= kMaxIntFracSig) {
        return constructFraction(minMaxFractionPlaces, minMaxFractionPlaces);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

FractionRounder Rounder::minFraction(int32_t minFractionPlaces) {
    if (minFractionPlaces >= 0 && minFractionPlaces <= kMaxIntFracSig) {
        return constructFraction(minFractionPlaces, -1);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

FractionRounder Rounder::maxFraction(int32_t maxFractionPlaces) {
    if (maxFractionPlaces >= 0 && maxFractionPlaces <= kMaxIntFracSig) {
        return constructFraction(0, maxFractionPlaces);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

FractionRounder Rounder::minMaxFraction(int32_t minFractionPlaces, int32_t maxFractionPlaces) {
    if (minFractionPlaces >= 0 && maxFractionPlaces <= kMaxIntFracSig &&
        minFractionPlaces <= maxFractionPlaces) {
        return constructFraction(minFractionPlaces, maxFractionPlaces);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

Rounder Rounder::fixedDigits(int32_t minMaxSignificantDigits) {
    if (minMaxSignificantDigits >= 0 && minMaxSignificantDigits <= kMaxIntFracSig) {
        return constructSignificant(minMaxSignificantDigits, minMaxSignificantDigits);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

Rounder Rounder::minDigits(int32_t minSignificantDigits) {
    if (minSignificantDigits >= 0 && minSignificantDigits <= kMaxIntFracSig) {
        return constructSignificant(minSignificantDigits, -1);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

Rounder Rounder::maxDigits(int32_t maxSignificantDigits) {
    if (maxSignificantDigits >= 0 && maxSignificantDigits <= kMaxIntFracSig) {
        return constructSignificant(0, maxSignificantDigits);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

Rounder Rounder::minMaxDigits(int32_t minSignificantDigits, int32_t maxSignificantDigits) {
    if (minSignificantDigits >= 0 && maxSignificantDigits <= kMaxIntFracSig &&
        minSignificantDigits <= maxSignificantDigits) {
        return constructSignificant(minSignificantDigits, maxSignificantDigits);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

IncrementRounder Rounder::increment(double roundingIncrement) {
    if (roundingIncrement > 0.0) {
        return constructIncrement(roundingIncrement, 0);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

CurrencyRounder Rounder::currency(UCurrencyUsage currencyUsage) {
    return constructCurrency(currencyUsage);
}

Rounder Rounder::withMode(RoundingMode roundingMode) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    return {fType, fUnion, roundingMode};
}

Rounder FractionRounder::withMinDigits(int32_t minSignificantDigits) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    if (minSignificantDigits >= 0 && minSignificantDigits <= kMaxIntFracSig) {
        return constructFractionSignificant(*this, minSignificantDigits, -1);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

Rounder FractionRounder::withMaxDigits(int32_t maxSignificantDigits) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    if (maxSignificantDigits >= 0 && maxSignificantDigits <= kMaxIntFracSig) {
        return constructFractionSignificant(*this, -1, maxSignificantDigits);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

// Private method on base class
Rounder Rounder::withCurrency(const CurrencyUnit &currency, UErrorCode &status) const {
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

// Public method on CurrencyRounder subclass
Rounder CurrencyRounder::withCurrency(const CurrencyUnit &currency) const {
    UErrorCode localStatus = U_ZERO_ERROR;
    Rounder result = Rounder::withCurrency(currency, localStatus);
    if (U_FAILURE(localStatus)) {
        return {localStatus};
    }
    return result;
}

Rounder IncrementRounder::withMinFraction(int32_t minFrac) const {
    if (fType == RND_ERROR) { return *this; } // no-op in error state
    if (minFrac >= 0 && minFrac <= kMaxIntFracSig) {
        return constructIncrement(fUnion.increment.fIncrement, minFrac);
    } else {
        return {U_NUMBER_DIGIT_WIDTH_OUTOFBOUNDS_ERROR};
    }
}

FractionRounder Rounder::constructFraction(int32_t minFrac, int32_t maxFrac) {
    FractionSignificantSettings settings;
    settings.fMinFrac = static_cast<int8_t> (minFrac);
    settings.fMaxFrac = static_cast<int8_t> (maxFrac);
    settings.fMinSig = -1;
    settings.fMaxSig = -1;
    RounderUnion union_;
    union_.fracSig = settings;
    return {RND_FRACTION, union_, kDefaultMode};
}

Rounder Rounder::constructSignificant(int32_t minSig, int32_t maxSig) {
    FractionSignificantSettings settings;
    settings.fMinFrac = -1;
    settings.fMaxFrac = -1;
    settings.fMinSig = static_cast<int8_t>(minSig);
    settings.fMaxSig = static_cast<int8_t>(maxSig);
    RounderUnion union_;
    union_.fracSig = settings;
    return {RND_SIGNIFICANT, union_, kDefaultMode};
}

Rounder
Rounder::constructFractionSignificant(const FractionRounder &base, int32_t minSig, int32_t maxSig) {
    FractionSignificantSettings settings = base.fUnion.fracSig;
    settings.fMinSig = static_cast<int8_t>(minSig);
    settings.fMaxSig = static_cast<int8_t>(maxSig);
    RounderUnion union_;
    union_.fracSig = settings;
    return {RND_FRACTION_SIGNIFICANT, union_, kDefaultMode};
}

IncrementRounder Rounder::constructIncrement(double increment, int32_t minFrac) {
    IncrementSettings settings;
    settings.fIncrement = increment;
    settings.fMinFrac = minFrac;
    RounderUnion union_;
    union_.increment = settings;
    return {RND_INCREMENT, union_, kDefaultMode};
}

CurrencyRounder Rounder::constructCurrency(UCurrencyUsage usage) {
    RounderUnion union_;
    union_.currencyUsage = usage;
    return {RND_CURRENCY, union_, kDefaultMode};
}

Rounder Rounder::constructPassThrough() {
    RounderUnion union_;
    union_.errorCode = U_ZERO_ERROR; // initialize the variable
    return {RND_PASS_THROUGH, union_, kDefaultMode};
}

void Rounder::setLocaleData(const CurrencyUnit &currency, UErrorCode &status) {
    if (fType == RND_CURRENCY) {
        *this = withCurrency(currency, status);
    }
}

int32_t
Rounder::chooseMultiplierAndApply(impl::DecimalQuantity &input, const impl::MultiplierProducer &producer,
                                  UErrorCode &status) {
    // TODO: Make a better and more efficient implementation.
    // TODO: Avoid the object creation here.
    DecimalQuantity copy(input);

    U_ASSERT(!input.isZero());
    int32_t magnitude = input.getMagnitude();
    int32_t multiplier = producer.getMultiplier(magnitude);
    input.adjustMagnitude(multiplier);
    apply(input, status);

    // If the number turned to zero when rounding, do not re-attempt the rounding.
    if (!input.isZero() && input.getMagnitude() == magnitude + multiplier + 1) {
        magnitude += 1;
        input = copy;
        multiplier = producer.getMultiplier(magnitude);
        input.adjustMagnitude(multiplier);
        U_ASSERT(input.getMagnitude() == magnitude + multiplier - 1);
        apply(input, status);
        U_ASSERT(input.getMagnitude() == magnitude + multiplier);
    }

    return multiplier;
}

/** This is the method that contains the actual rounding logic. */
void Rounder::apply(impl::DecimalQuantity &value, UErrorCode& status) const {
    switch (fType) {
        case RND_BOGUS:
        case RND_ERROR:
            // Errors should be caught before the apply() method is called
            status = U_INTERNAL_PROGRAM_ERROR;
            break;

        case RND_NONE:
            value.roundToInfinity();
            break;

        case RND_FRACTION:
            value.roundToMagnitude(
                    getRoundingMagnitudeFraction(fUnion.fracSig.fMaxFrac), fRoundingMode, status);
            value.setFractionLength(
                    uprv_max(0, -getDisplayMagnitudeFraction(fUnion.fracSig.fMinFrac)), INT32_MAX);
            break;

        case RND_SIGNIFICANT:
            value.roundToMagnitude(
                    getRoundingMagnitudeSignificant(value, fUnion.fracSig.fMaxSig),
                    fRoundingMode,
                    status);
            value.setFractionLength(
                    uprv_max(0, -getDisplayMagnitudeSignificant(value, fUnion.fracSig.fMinSig)),
                    INT32_MAX);
            break;

        case RND_FRACTION_SIGNIFICANT: {
            int32_t displayMag = getDisplayMagnitudeFraction(fUnion.fracSig.fMinFrac);
            int32_t roundingMag = getRoundingMagnitudeFraction(fUnion.fracSig.fMaxFrac);
            if (fUnion.fracSig.fMinSig == -1) {
                // Max Sig override
                int32_t candidate = getRoundingMagnitudeSignificant(value, fUnion.fracSig.fMaxSig);
                roundingMag = uprv_max(roundingMag, candidate);
            } else {
                // Min Sig override
                int32_t candidate = getDisplayMagnitudeSignificant(value, fUnion.fracSig.fMinSig);
                roundingMag = uprv_min(roundingMag, candidate);
            }
            value.roundToMagnitude(roundingMag, fRoundingMode, status);
            value.setFractionLength(uprv_max(0, -displayMag), INT32_MAX);
            break;
        }

        case RND_INCREMENT:
            value.roundToIncrement(
                fUnion.increment.fIncrement, fRoundingMode, fUnion.increment.fMinFrac, status);
            value.setFractionLength(fUnion.increment.fMinFrac, fUnion.increment.fMinFrac);
            break;

        case RND_CURRENCY:
            // Call .withCurrency() before .apply()!
            U_ASSERT(false);

        case RND_PASS_THROUGH:
            break;
    }
}

void Rounder::apply(impl::DecimalQuantity &value, int32_t minInt, UErrorCode /*status*/) {
    // This method is intended for the one specific purpose of helping print "00.000E0".
    U_ASSERT(fType == RND_SIGNIFICANT);
    U_ASSERT(value.isZero());
    value.setFractionLength(fUnion.fracSig.fMinSig - minInt, INT32_MAX);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
