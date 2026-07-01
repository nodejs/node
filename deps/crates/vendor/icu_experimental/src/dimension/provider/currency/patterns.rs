// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_pattern::DoublePlaceholderPattern;
use icu_plurals::provider::PluralElementsPackedCow;
use icu_provider::prelude::*;

icu_provider::data_marker!(
    /// `CurrencyPatternsDataV1`
    CurrencyPatternsDataV1,
    CurrencyPatternsData<'static>,
);

/// Currency Extended data struct.
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::patterns))]
#[yoke(prove_covariance_manually)]
pub struct CurrencyPatternsData<'data> {
    /// Contains the unit patterns for a currency based on plural rules.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub patterns: PluralElementsPackedCow<'data, DoublePlaceholderPattern>,
}

icu_provider::data_struct!(CurrencyPatternsData<'_>, #[cfg(feature = "datagen")]);
