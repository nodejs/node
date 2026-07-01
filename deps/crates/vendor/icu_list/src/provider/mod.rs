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

use icu_provider::prelude::*;

mod serde_dfa;
pub use serde_dfa::SerdeDFA;
use zerovec::VarZeroCow;

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
    use icu_list_data::*;
    pub mod icu {
        pub use crate as list;
        pub use icu_locale as locale;
    }
    make_provider!(Baked);
    impl_list_and_v1!(Baked);
    impl_list_or_v1!(Baked);
    impl_list_unit_v1!(Baked);
};

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[ListAndV1::INFO, ListOrV1::INFO, ListUnitV1::INFO];

data_marker!(
    /// Marker for and lists
    ListAndV1,
    "list/and/v1",
    ListFormatterPatterns<'static>,
);
data_marker!(
    /// Marker for or lists
    ListOrV1,
    "list/or/v1",
    ListFormatterPatterns<'static>,
);
data_marker!(
    /// Marker for unit lists
    ListUnitV1,
    "list/unit/v1",
    ListFormatterPatterns<'static>,
);

icu_provider::data_struct!(
    ListFormatterPatterns<'_>,
    #[cfg(feature = "datagen")]
);

/// Symbols and metadata required for [`ListFormatter`](crate::ListFormatter).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Debug, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_list::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct ListFormatterPatterns<'data> {
    /// The start pattern
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub start: ListJoinerPattern<'data>,
    /// The middle pattern. It doesn't need to be a pattern because it has to start with `{0}`
    /// and end with `{1}`, so we just store the string in between.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub middle: VarZeroCow<'data, str>,
    /// The end pattern
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub end: ConditionalListJoinerPattern<'data>,
    /// The pair pattern, if it's different from the end pattern.
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub pair: Option<ConditionalListJoinerPattern<'data>>,
}

impl ListFormatterPatterns<'_> {
    /// The marker attributes for narrow lists
    pub const NARROW: &'static DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("N");
    #[doc(hidden)]
    pub const NARROW_STR: &'static str = Self::NARROW.as_str();
    /// The marker attributes for short lists
    pub const SHORT: &'static DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("S");
    #[doc(hidden)]
    pub const SHORT_STR: &'static str = Self::SHORT.as_str();
    /// The marker attributes for wide lists
    pub const WIDE: &'static DataMarkerAttributes = DataMarkerAttributes::from_str_or_panic("W");
    #[doc(hidden)]
    pub const WIDE_STR: &'static str = Self::WIDE.as_str();
}

/// A pattern that can behave conditionally on the next element.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Debug, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_list::provider))]
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
pub struct ConditionalListJoinerPattern<'data> {
    /// The default pattern
    #[cfg_attr(feature = "serde", serde(borrow))]
    pub default: ListJoinerPattern<'data>,
    /// And optional special case
    #[cfg_attr(
        feature = "serde",
        serde(borrow, deserialize_with = "SpecialCasePattern::deserialize_option")
    )]
    pub special_case: Option<SpecialCasePattern<'data>>,
}

/// The special case of a [`ConditionalListJoinerPattern`]
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Debug, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_list::provider))]
pub struct SpecialCasePattern<'data> {
    /// The condition on the following element
    pub condition: SerdeDFA<'data>,
    /// The pattern if the condition matches
    pub pattern: ListJoinerPattern<'data>,
}

#[cfg(feature = "serde")]
impl<'data> SpecialCasePattern<'data> {
    // If the condition doesn't deserialize, the whole special case becomes `None`
    fn deserialize_option<'de: 'data, D>(deserializer: D) -> Result<Option<Self>, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        use serde::Deserialize;

        #[derive(Deserialize)]
        struct SpecialCasePatternOptionalDfa<'data> {
            #[cfg_attr(
                feature = "serde",
                serde(borrow, deserialize_with = "SerdeDFA::maybe_deserialize")
            )]
            pub condition: Option<SerdeDFA<'data>>,
            #[cfg_attr(feature = "serde", serde(borrow))]
            pub pattern: ListJoinerPattern<'data>,
        }

        Ok(
            match Option::<SpecialCasePatternOptionalDfa<'data>>::deserialize(deserializer)? {
                Some(SpecialCasePatternOptionalDfa {
                    condition: Some(condition),
                    pattern,
                }) => Some(SpecialCasePattern { condition, pattern }),
                _ => None,
            },
        )
    }
}

/// A pattern containing two numeric placeholders ("{0}, and {1}.")
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Clone, Debug, PartialEq, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize))]
pub struct ListJoinerPattern<'data> {
    /// The pattern string without the placeholders
    pub(crate) string: VarZeroCow<'data, str>,
    /// The index of the first placeholder. Always `<= index_1`.
    // Always 0 for CLDR data, so we don't need to serialize it.
    // In-memory we have free space for it as index_1 doesn't
    // fill a word.
    #[cfg_attr(feature = "datagen", serde(skip))]
    pub(crate) index_0: u8,
    /// The index of the second placeholder. Always `< string.len()`.
    pub(crate) index_1: u8,
}

#[cfg(feature = "serde")]
impl<'de: 'data, 'data> serde::Deserialize<'de> for ListJoinerPattern<'data> {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        #[derive(serde::Deserialize)]
        struct Dummy<'data> {
            #[cfg_attr(feature = "serde", serde(borrow))]
            string: VarZeroCow<'data, str>,
            index_1: u8,
        }
        let Dummy { string, index_1 } = Dummy::deserialize(deserializer)?;

        if index_1 as usize > string.len() {
            use serde::de::Error;
            Err(D::Error::custom("invalid index_1"))
        } else {
            Ok(ListJoinerPattern {
                string,
                index_0: 0,
                index_1,
            })
        }
    }
}

impl<'a> ListJoinerPattern<'a> {
    /// Constructs a [`ListJoinerPattern`] from raw parts. Used by databake.
    ///
    /// # Panics
    /// If `string[..index_1]` panics.
    pub const fn from_parts(string: VarZeroCow<'a, str>, index_1: u8) -> Self {
        Self {
            string,
            index_0: 0,
            index_1,
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::Bake for ListJoinerPattern<'_> {
    fn bake(&self, env: &databake::CrateEnv) -> databake::TokenStream {
        env.insert("icu_list");
        let string = self.string.bake(env);
        let index_1 = self.index_1.bake(env);
        databake::quote! {
            icu_list::provider::ListJoinerPattern::from_parts(#string, #index_1)
        }
    }
}

#[cfg(feature = "datagen")]
impl databake::BakeSize for ListJoinerPattern<'_> {
    fn borrows_size(&self) -> usize {
        self.string.borrows_size()
    }
}

#[cfg(all(test, feature = "datagen"))]
#[test]
fn databake() {
    databake::test_bake!(
        ListJoinerPattern,
        const,
        crate::provider::ListJoinerPattern::from_parts(
            unsafe { zerovec::VarZeroCow::from_bytes_unchecked(b", ") },
            2u8
        ),
        icu_list
    );
}
