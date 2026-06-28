// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use alloc::borrow::Cow;
use zerovec::ZeroMap;

use icu_provider::prelude::*;

#[cfg(feature = "compiled_data")]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub use crate::provider::Baked;

use crate::dimension::provider::units::pattern_key::PatternKey;

icu_provider::data_marker!(
   /// `UnitsEssentialsV1`
   UnitsEssentialsV1,
   UnitsEssentials<'static>
);

// TODO: use `Pattern`s instead of `str` for the patterns' string representations.
/// This type contains all of the essential data for units formatting such as `per`, `power`, `times`, etc.
///
/// Note:
//      TODO: Use the auxiliary key as real key in the future because the choice of the length is developer's decision.
///     Auxiliary key represent the length: e.g. `long`, `short`, `narrow`.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::units::essentials))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct UnitsEssentials<'data> {
    // TODO: use `SinglePlaceholderPattern` instead of `str` for the patterns' string representations.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub prefixes: ZeroMap<'data, PatternKey, str>,

    // TODO: use `DoublePlaceholderPattern` instead of `str` for the patterns' string representations.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub per: Cow<'data, str>,

    // TODO: use `DoublePlaceholderPattern` instead of `str` for the patterns' string representations.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub times: Cow<'data, str>,
}

icu_provider::data_struct!(UnitsEssentials<'_>, #[cfg(feature = "datagen")]);

/// A CLDR plural keyword, or the explicit value 1.
/// See <https://www.unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules>. // TODO??
#[zerovec::make_ule(CompoundCountULE)]
#[derive(Copy, Clone, PartialOrd, Ord, PartialEq, Eq, Debug)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::units::essentials))]
#[repr(u8)]
pub enum CompoundCount {
    /// The CLDR keyword `zero`.
    Zero = 0,
    /// The CLDR keyword `one`.
    One = 1,
    /// The CLDR keyword `two`.
    Two = 2,
    /// The CLDR keyword `few`.
    Few = 3,
    /// The CLDR keyword `many`.
    Many = 4,
    /// The CLDR keyword `other`.
    Other = 5,
}

impl From<u8> for CompoundCount {
    fn from(val: u8) -> Self {
        match val {
            0 => CompoundCount::Zero,
            1 => CompoundCount::One,
            2 => CompoundCount::Two,
            3 => CompoundCount::Few,
            4 => CompoundCount::Many,
            5 => CompoundCount::Other,
            _ => unreachable!(),
        }
    }
}
