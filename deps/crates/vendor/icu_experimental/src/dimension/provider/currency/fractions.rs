// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for currency fraction data.
//!
//! Read more about data providers: [`icu_provider`]

use icu_provider::prelude::*;
use tinystr::UnvalidatedTinyAsciiStr;
use zerovec::ZeroMap;

#[cfg(feature = "compiled_data")]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub use crate::provider::Baked;

icu_provider::data_marker!(
    /// `CurrencyFractionsV1` provides currency fraction data for rounding and decimal digit rules.
    CurrencyFractionsV1,
    CurrencyFractions<'static>,
    is_singleton = true
);

/// Currency fraction information for all currencies.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::fractions))]
#[yoke(prove_covariance_manually)]
pub struct CurrencyFractions<'data> {
    /// Map from 3-letter ISO code to fraction info (only currencies that differ from default)
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub fractions: ZeroMap<'data, UnvalidatedTinyAsciiStr<3>, FractionInfo>,

    /// Default fraction info (used when currency not in map)
    pub default: FractionInfo,
}

icu_provider::data_struct!(CurrencyFractions<'_>, #[cfg(feature = "datagen")]);

/// Fraction and rounding information for a currency.
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::fractions))]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Default)]
pub struct FractionInfo {
    /// Number of decimal digits for standard formatting
    pub digits: u8,
    /// Rounding increment (0 = no special rounding)
    pub rounding: u8,
    /// Number of decimal digits for cash transactions (if different)
    pub cash_digits: Option<u8>,
    /// Rounding increment for cash transactions (if different)
    pub cash_rounding: Option<u8>,
}
