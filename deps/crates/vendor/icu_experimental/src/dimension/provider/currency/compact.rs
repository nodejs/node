// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_pattern::DoublePlaceholder;
use icu_provider::prelude::*;

use icu_decimal::provider::CompactPatterns;

icu_provider::data_marker!(
    /// `ShortCurrencyCompactV1`
    ShortCurrencyCompactV1,
    ShortCurrencyCompact<'static>
);

/// Currency Compact  data struct.
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::compact))]
#[yoke(prove_covariance_manually)]
pub struct ShortCurrencyCompact<'data> {
    /// Contains the standard compact patterns.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub standard: CompactPatterns<'data, DoublePlaceholder>,
    /// Contains the alpha-next-to-number compact patterns.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub alpha_next_to_number: CompactPatterns<'data, DoublePlaceholder>,
}

icu_provider::data_struct!(ShortCurrencyCompact<'_>, #[cfg(feature = "datagen")]);
