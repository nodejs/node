// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data provider struct definitions for this ICU4X component.
//!
//! Read more about data providers: [`icu_provider`]

use icu_pattern::SinglePlaceholderPattern;
use icu_plurals::provider::PluralElementsPackedCow;
use icu_provider::prelude::*;

icu_provider::data_marker!(
    /// `UnitsDisplayNamesV1`
    UnitsDisplayNamesV1,
    UnitsDisplayNames<'static>,
    #[cfg(feature = "datagen")]
    attributes_domain = "units"
);

#[derive(Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct UnitsDisplayNames<'data> {
    // TODO: use `MeasureUnit` for the units key instead of strings.
    /// Contains the long width patterns for the units.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub patterns: PluralElementsPackedCow<'data, SinglePlaceholderPattern>,
}

icu_provider::data_struct!(UnitsDisplayNames<'_>, #[cfg(feature = "datagen")]);

impl<'data> UnitsDisplayNames<'data> {
    /// Construct an instance directly from a byte slice.
    ///
    /// # Safety
    ///
    /// The bytes must represent a valid [`icu_plurals::provider::PluralElementsPackedULE`]
    pub const unsafe fn from_bytes_unchecked(bytes: &'data [u8]) -> Self {
        Self {
            patterns: PluralElementsPackedCow {
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
impl databake::Bake for UnitsDisplayNames<'_> {
    fn bake(&self, ctx: &databake::CrateEnv) -> databake::TokenStream {
        use zerovec::ule::VarULE;
        ctx.insert("icu_experimental::dimension::provider::units::display_names");
        let bytes = self.patterns.elements.as_bytes().bake(ctx);
        // Safety: The bytes are returned by `PluralElementsPackedULE::slice_as_bytes`.
        databake::quote! { unsafe {
            icu_experimental::dimension::provider::units::display_names::UnitsDisplayNames::from_bytes_unchecked(#bytes)
        }}
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for UnitsDisplayNames<'_> {
    fn borrows_size(&self) -> usize {
        self.patterns.borrows_size()
    }
}
