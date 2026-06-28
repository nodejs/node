// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_plurals::provider::PluralElementsPackedCow;
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
    /// Extended currency data needed for currency formatting. For example, currency display names.
    CurrencyExtendedDataV1,
    CurrencyExtendedData<'static>,
    #[cfg(feature = "datagen")]
    attributes_domain = "currency",
);

/// Currency Extended  data struct.
#[derive(Debug, Clone, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[yoke(prove_covariance_manually)]
pub struct CurrencyExtendedData<'data> {
    /// Contains the localized display names for a currency based on plural rules.
    /// For instance, in the "en" locale for the "USD" currency:
    ///     - "US Dollars" when count is `zero`,
    ///     - "US Dollar" when count is `one`,
    ///     ... etc.
    /// # NOTE
    ///    Regards to the [Unicode Report TR35](https://unicode.org/reports/tr35/tr35-numbers.html#Currencies),
    ///    If no matching for specific count, the `other` count will be used.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub display_names: PluralElementsPackedCow<'data, str>,
}

icu_provider::data_struct!(CurrencyExtendedData<'_>, #[cfg(feature = "datagen")]);

impl<'data> CurrencyExtendedData<'data> {
    /// Construct an instance directly from a byte slice.
    ///
    /// # Safety
    ///
    /// The bytes must represent a valid [`icu_plurals::provider::PluralElementsPackedULE`]
    pub const unsafe fn from_bytes_unchecked(bytes: &'data [u8]) -> Self {
        Self {
            display_names: PluralElementsPackedCow {
                elements: alloc::borrow::Cow::Borrowed(
                    // Safety: this function's safety invariant guarantees that the bytes
                    // represent a valid `PluralElementsPackedULE`
                    icu_plurals::provider::PluralElementsPackedULE::from_bytes_unchecked(bytes),
                ),
            },
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for CurrencyExtendedData<'_> {
    fn bake(&self, ctx: &databake::CrateEnv) -> databake::TokenStream {
        use zerovec::ule::VarULE;
        ctx.insert("icu_experimental::dimension::provider::currency");
        let bytes = self.display_names.elements.as_bytes().bake(ctx);
        // Safety: The bytes are returned by `PluralElementsPackedULE::slice_as_bytes`.
        databake::quote! { unsafe {
            icu_experimental::dimension::provider::currency::extended::CurrencyExtendedData::from_bytes_unchecked(#bytes)
        }}
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for CurrencyExtendedData<'_> {
    fn borrows_size(&self) -> usize {
        self.display_names.borrows_size()
    }
}
