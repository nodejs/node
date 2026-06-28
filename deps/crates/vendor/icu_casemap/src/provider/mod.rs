// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data provider struct definitions for this ICU4X component.
//!
//! <div class="stab unstable">
//! 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. While the serde representation of data structs is guaranteed
//! to be stable, their Rust representation might not be. Use with caution.
//! </div>
//!
//! Read more about data providers: [`icu_provider`]

// Provider structs must be stable
#![allow(clippy::exhaustive_structs, clippy::exhaustive_enums)]

use icu_provider::prelude::*;

use crate::provider::data::CaseMapData;
use crate::provider::exceptions::CaseMapExceptions;
use icu_collections::codepointtrie::CodePointTrie;
#[cfg(feature = "datagen")]
use icu_collections::codepointtrie::CodePointTrieHeader;

pub mod data;
pub mod exception_helpers;
pub mod exceptions;
#[cfg(feature = "datagen")]
mod exceptions_builder;
mod unfold;

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
    use icu_casemap_data::*;
    pub mod icu {
        pub use crate as casemap;
        pub use icu_collections as collections;
    }
    make_provider!(Baked);
    impl_case_map_v1!(Baked);
    impl_case_map_unfold_v1!(Baked);
};

icu_provider::data_marker!(
    /// Marker for casemapping data.
    CaseMapV1,
    "case/map/v1",
    CaseMap<'static>,
    is_singleton = true
);

icu_provider::data_marker!(
    /// Reverse case mapping data.
    CaseMapUnfoldV1,
    "case/map/unfold/v1",
    CaseMapUnfold<'static>,
    is_singleton = true
);

#[cfg(feature = "datagen")]
/// The latest minimum set of markers required by this component.
pub const MARKERS: &[DataMarkerInfo] = &[CaseMapUnfoldV1::INFO, CaseMapV1::INFO];

pub use self::unfold::CaseMapUnfold;

/// This type contains all of the casemapping data
///
/// The methods in the provider module are primarily about accessing its data,
/// however the full algorithms are also implemented as methods on this type in
/// the `internals` module of this crate.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider))]
#[yoke(prove_covariance_manually)]
/// CaseMapper provides low-level access to the data necessary to
/// convert characters and strings to upper, lower, or title case.
pub struct CaseMap<'data> {
    /// Case mapping data
    pub trie: CodePointTrie<'data, CaseMapData>,
    /// Exceptions to the case mapping data
    pub exceptions: CaseMapExceptions<'data>,
}

icu_provider::data_struct!(
    CaseMap<'_>,
    #[cfg(feature = "datagen")]
);

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for CaseMap<'de> {
    fn deserialize<D: serde::Deserializer<'de>>(deserializer: D) -> Result<Self, D::Error> {
        #[derive(serde::Deserialize)]
        pub struct Raw<'data> {
            #[serde(borrow)]
            pub trie: CodePointTrie<'data, CaseMapData>,
            #[serde(borrow)]
            pub exceptions: CaseMapExceptions<'data>,
        }

        let Raw { trie, exceptions } = Raw::deserialize(deserializer)?;
        let result = Self { trie, exceptions };
        debug_assert!(result.validate().is_ok());
        Ok(result)
    }
}

impl CaseMap<'_> {
    /// Creates a new [`CaseMap`] using data exported by the
    // `icuexportdata` tool in ICU4C. Validates that the data is
    // consistent.
    #[cfg(feature = "datagen")]
    pub fn try_from_icu(
        trie_header: CodePointTrieHeader,
        trie_index: &[u16],
        trie_data: &[u16],
        exceptions: &[u16],
    ) -> Result<Self, DataError> {
        use self::exceptions_builder::CaseMapExceptionsBuilder;
        use zerovec::ZeroVec;
        let exceptions_builder = CaseMapExceptionsBuilder::new(exceptions);
        let (exceptions, idx_map) = exceptions_builder.build()?;

        let trie_index = ZeroVec::alloc_from_slice(trie_index);

        #[expect(clippy::unwrap_used)] // datagen only
        let trie_data = trie_data
            .iter()
            .map(|&i| {
                CaseMapData::try_from_icu_integer(i)
                    .unwrap()
                    .with_updated_exception(&idx_map)
            })
            .collect::<ZeroVec<_>>();

        let trie = CodePointTrie::try_new(trie_header, trie_index, trie_data)
            .map_err(|_| DataError::custom("Casemapping data does not form valid trie"))?;

        let result = Self { trie, exceptions };
        result.validate().map_err(DataError::custom)?;
        Ok(result)
    }

    /// Given an existing [`CaseMap`], validates that the data is
    /// consistent. A [`CaseMap`] created by the ICU transformer has
    /// already been validated. Calling this function is only
    /// necessary if you are concerned about data corruption after
    /// deserializing.
    #[cfg(any(feature = "serde", feature = "datagen"))]
    #[allow(unused)] // is only used in debug mode for serde
    pub(crate) fn validate(&self) -> Result<(), &'static str> {
        // First, validate that exception data is well-formed.
        let valid_exception_indices = self.exceptions.validate()?;

        let validate_delta = |c: char, delta: i32| -> Result<(), &'static str> {
            let new_c =
                u32::try_from(c as i32 + delta).map_err(|_| "Delta larger than character")?;
            char::from_u32(new_c).ok_or("Invalid delta")?;
            Ok(())
        };

        for i in 0..char::MAX as u32 {
            if let Some(c) = char::from_u32(i) {
                let data = self.lookup_data(c);
                if data.has_exception() {
                    let idx = data.exception_index();
                    let exception = self.exceptions.get(idx);
                    // Verify that the exception index points to a valid exception header.
                    if !valid_exception_indices.contains(&idx) {
                        return Err("Invalid exception index in trie data");
                    }
                    exception.validate()?;
                } else {
                    validate_delta(c, data.delta() as i32)?;
                }
            }
        }
        Ok(())
    }

    pub(crate) fn lookup_data(&self, c: char) -> CaseMapData {
        self.trie.get32(c as u32)
    }
}
