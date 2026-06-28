// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use alloc::borrow::Cow;
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

icu_provider::data_marker!(
    /// `CurrencyDisplaynameV1`
    CurrencyDisplaynameV1,
    CurrencyDisplayname<'static>,
    #[cfg(feature = "datagen")]
    attributes_domain = "currency",
);

/// Currency Extended  data struct.
#[derive(Debug, Clone, Default, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::displayname))]
#[yoke(prove_covariance_manually)]
pub struct CurrencyDisplayname<'data> {
    /// The display name for the currency.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub display_name: Cow<'data, str>,
}

icu_provider::data_struct!(CurrencyDisplayname<'_>, #[cfg(feature = "datagen")]);
