// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use alloc::borrow::Cow;
use icu_provider::prelude::*;
use tinystr::UnvalidatedTinyAsciiStr;
use zerovec::{VarZeroVec, ZeroMap};

#[cfg(feature = "serde")]
use icu_pattern::DoublePlaceholder;
use icu_pattern::DoublePlaceholderPattern;

use crate::dimension::currency::options::Width;
use crate::dimension::currency::CurrencyCode;

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
    /// Essential currency data needed for currency formatting. For example, currency patterns.
    CurrencyEssentialsV1,
    CurrencyEssentials<'static>
);

/// This type contains all of the essential data for currency formatting.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path =  icu_experimental::dimension::provider::currency::essentials))]
#[yoke(prove_covariance_manually)]
pub struct CurrencyEssentials<'data> {
    /// A mapping from 3-letter currency ISO codes to their [`CurrencyPatternConfig`].
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub pattern_config_map: ZeroMap<'data, UnvalidatedTinyAsciiStr<3>, CurrencyPatternConfig>,

    // TODO(#4677): Implement the pattern to accept the signed negative and signed positive patterns.
    /// The standard currency pattern used for formatting.
    ///
    /// This pattern uses two placeholders:
    /// - `0`: The numeric currency value.
    /// - `1`: The currency symbol (`¤`).
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<DoublePlaceholder, _>"
        )
    )]
    pub standard_pattern: Cow<'data, DoublePlaceholderPattern>,

    // TODO(#4677): Implement the pattern to accept the signed negative and signed positive patterns.
    /// The `standard_alpha_next_to_number` currency pattern used for formatting.
    ///
    /// This pattern uses two placeholders:
    /// - `0`: The numeric currency value.
    /// - `1`: The currency symbol (`¤`).
    #[cfg_attr(
        feature = "serde",
        serde(
            borrow,
            deserialize_with = "icu_pattern::deserialize_borrowed_cow::<DoublePlaceholder, _>"
        )
    )]
    pub standard_alpha_next_to_number_pattern: Cow<'data, DoublePlaceholderPattern>,

    /// A list of placeholders (strings), such as currency symbols, referenced by index.
    ///
    /// These values are retrieved using [`PlaceholderValue::Index`] stored in [`CurrencyPatternConfig`].
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub placeholders: VarZeroVec<'data, str>,

    /// The fallback currency pattern configuration used
    /// when a specific currency's pattern is not found in the currency patterns map.
    pub default_pattern_config: CurrencyPatternConfig,
}

icu_provider::data_struct!(CurrencyEssentials<'_>, #[cfg(feature = "datagen")]);

#[zerovec::make_ule(PatternSelectionULE)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::essentials))]
#[derive(Copy, Clone, Debug, PartialOrd, Ord, PartialEq, Eq, Default)]
#[repr(u8)]
pub enum PatternSelection {
    /// Use the standard pattern.
    #[default]
    Standard = 0,

    /// Use the `standard_alpha_next_to_number` pattern.
    StandardAlphaNextToNumber = 1,
}

#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::essentials))]
#[derive(Copy, Debug, Clone, PartialEq, PartialOrd, Eq, Ord)]
#[repr(u16)]
pub enum PlaceholderValue {
    /// The index of the place holder in the place holders list.
    /// NOTE: the maximum value is `MAX_PLACEHOLDER_INDEX` which is 2045 (`0b0111_1111_1101`).
    Index(u16),

    /// The place holder is the iso code.
    ISO,
}

#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_experimental::dimension::provider::currency::essentials))]
#[derive(Copy, Debug, Clone, Default, PartialEq, PartialOrd, Eq, Ord)]
pub struct CurrencyPatternConfig {
    /// Indicates which pattern to use for short currency formatting.
    pub short_pattern_selection: PatternSelection,

    /// Indicates which pattern to use for narrow currency formatting.
    pub narrow_pattern_selection: PatternSelection,

    /// The index of the short pattern place holder in the place holders list.
    /// If the value is `None`, this means that the short pattern does not have a place holder.
    pub short_placeholder_value: Option<PlaceholderValue>,

    /// The index of the narrow pattern place holder in the place holders list.
    /// If the value is `None`, this means that the narrow pattern does not have a place holder.
    pub narrow_placeholder_value: Option<PlaceholderValue>,
}

impl<'a> CurrencyEssentials<'a> {
    // Returns the currency placeholder value for the currency,
    // and the currency pattern for the given width and currency
    pub(crate) fn name_and_pattern(
        &'a self,
        width: Width,
        currency: &'a CurrencyCode,
    ) -> (&'a str, &'a DoublePlaceholderPattern, PatternSelection) {
        let config = self
            .pattern_config_map
            .get_copied(&currency.0.to_unvalidated())
            .unwrap_or(self.default_pattern_config);

        let placeholder_index = match width {
            Width::Short => config.short_placeholder_value,
            Width::Narrow => config.narrow_placeholder_value,
        };

        let currency = match placeholder_index {
            Some(PlaceholderValue::Index(index)) => self.placeholders.get(index.into()),
            Some(PlaceholderValue::ISO) | None => None,
        }
        .unwrap_or(currency.0.as_str());

        let pattern_selection = match width {
            Width::Short => config.short_pattern_selection,
            Width::Narrow => config.narrow_pattern_selection,
        };

        let pattern = match pattern_selection {
            PatternSelection::Standard => &*self.standard_pattern,
            PatternSelection::StandardAlphaNextToNumber => {
                &*self.standard_alpha_next_to_number_pattern
            }
        };

        (currency, pattern, pattern_selection)
    }
}
