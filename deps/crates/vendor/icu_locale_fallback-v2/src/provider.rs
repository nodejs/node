// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

#[cfg(feature = "compiled_data")]
#[derive(Debug)]
/// Baked data
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. In particular, the `DataProvider` implementations are only
/// guaranteed to match with this version's `*_unstable` providers. Use with caution.
/// </div>
pub struct Baked;

#[cfg(feature = "compiled_data")]
#[allow(unused_imports)]
const _: () = {
    use icu_locale_fallback_data::*;
    pub mod icu {
        pub mod locale {
            pub use crate as fallback;
        }
    }
    pub use crate as icu_locale_fallback;
    make_provider!(Baked);
    impl_locale_likely_subtags_language_v1!(Baked);
    impl_locale_parents_v1!(Baked);
};

icu_provider::data_marker!(
    /// Marker for data for likely subtags for languages.
    LocaleLikelySubtagsLanguageV1,
    "locale/likely/subtags/language/v1",
    LikelySubtagsForLanguage<'static>,
    is_singleton = true
);
icu_provider::data_marker!(
    /// Marker for locale fallback parents data.
    LocaleParentsV1,
    "locale/parents/v1",
    Parents<'static>,
    is_singleton = true
);

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] =
    &[LocaleLikelySubtagsLanguageV1::INFO, LocaleParentsV1::INFO];

use icu_locale_core::subtags::{Language, Region, Script};
use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;
use tinystr::UnvalidatedTinyAsciiStr;
use zerovec::ZeroMap;

type UnvalidatedLanguage = UnvalidatedTinyAsciiStr<3>;
type UnvalidatedScript = UnvalidatedTinyAsciiStr<4>;
type UnvalidatedRegion = UnvalidatedTinyAsciiStr<3>;

#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_locale_fallback::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
/// This likely subtags data is used for the minimize and maximize operations.
///
/// Each field defines a mapping from an old identifier to a new identifier,
/// based upon the rules in
/// <https://www.unicode.org/reports/tr35/#Likely_Subtags>.
///
/// The data is stored is broken down into smaller vectors based upon the rules
/// defined for the likely subtags maximize algorithm.
///
/// For efficiency, only the relevant part of the [`icu_locale_core::LanguageIdentifier`] is stored
/// for searching and replacing. E.g., the `language_script` field is used to store
/// rules for [`icu_locale_core::LanguageIdentifier`]s that contain a language and a script, but not a
/// region.
///
/// This struct contains mappings when the input contains a language subtag.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[yoke(prove_covariance_manually)]
pub struct LikelySubtagsForLanguage<'data> {
    /// Language and script.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub language_script: ZeroMap<'data, (UnvalidatedLanguage, UnvalidatedScript), Region>,
    /// Language and region.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub language_region: ZeroMap<'data, (UnvalidatedLanguage, UnvalidatedRegion), Script>,
    /// Just language.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub language: ZeroMap<'data, UnvalidatedLanguage, (Script, Region)>,
    /// Undefined.
    pub und: (Language, Script, Region),
}

icu_provider::data_struct!(
    LikelySubtagsForLanguage<'_>,
    #[cfg(feature = "datagen")]
);

/// Locale fallback rules derived from CLDR parent locales data.
#[derive(Default, Clone, PartialEq, Debug, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_locale_fallback::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[yoke(prove_covariance_manually)]
pub struct Parents<'data> {
    /// Map from language identifier to language identifier, indicating that the language on the
    /// left should inherit from the language on the right.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub parents: ZeroMap<'data, PotentialUtf8, (Language, Option<Script>, Option<Region>)>,
}

icu_provider::data_struct!(
    Parents<'_>,
    #[cfg(feature = "datagen")]
);
