// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use num_bigint::BigInt;
use num_rational::Ratio;
use num_traits::ToPrimitive;

// TODO: add Mul & Add for references to avoid cloning.
/// A trait for types that can be converted between two units.
pub trait Convertible: Clone {
    /// Adds two values by reference, avoiding data cloning.
    fn add_refs(&self, other: &Self) -> Self;

    /// Multiplies two values by reference, avoiding data cloning.
    fn mul_refs(&self, other: &Self) -> Self;

    /// Converts a [`Ratio<BigInt>`] to the implementing type.
    fn from_ratio_bigint(ratio: Ratio<BigInt>) -> Option<Self>;

    /// Returns the reciprocal of the implementing type.
    fn reciprocal(&self) -> Self;
}

impl Convertible for Ratio<BigInt> {
    fn mul_refs(&self, other: &Self) -> Self {
        self * other
    }

    fn add_refs(&self, other: &Self) -> Self {
        self + other
    }

    fn from_ratio_bigint(ratio: Ratio<BigInt>) -> Option<Self> {
        Some(ratio)
    }

    fn reciprocal(&self) -> Self {
        self.recip()
    }
}

impl Convertible for f64 {
    fn mul_refs(&self, other: &Self) -> Self {
        self * other
    }

    fn add_refs(&self, other: &Self) -> Self {
        self + other
    }

    fn from_ratio_bigint(ratio: Ratio<BigInt>) -> Option<Self> {
        ratio.to_f64()
    }

    fn reciprocal(&self) -> Self {
        self.recip()
    }
}
