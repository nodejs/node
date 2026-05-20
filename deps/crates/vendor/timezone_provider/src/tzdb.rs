//! `timezone_provider` is the core data provider implementations for `temporal_rs`

// What are we even doing here? Why are providers needed?
//
// Two core data sources need to be accounted for:
//
//   - IANA identifier normalization (hopefully, semi easy)
//   - IANA TZif data (much harder)
//

use alloc::borrow::Cow;

use crate::provider::{NormalizedId, TimeZoneNormalizer, TimeZoneProviderResult};
use crate::TimeZoneProviderError;
use crate::SINGLETON_IANA_NORMALIZER;
use zerotrie::ZeroAsciiIgnoreCaseTrie;
use zerovec::{VarZeroVec, ZeroVec};

#[cfg(feature = "datagen")]
pub(crate) mod datagen;

/// A data struct for IANA identifier normalization
#[derive(PartialEq, Debug, Clone)]
#[cfg_attr(
    feature = "datagen",
    derive(serde::Serialize, yoke::Yokeable, serde::Deserialize, databake::Bake)
)]
#[cfg_attr(feature = "datagen", databake(path = timezone_provider))]
pub struct IanaIdentifierNormalizer<'data> {
    /// TZDB version
    pub version: Cow<'data, str>,
    /// An index to the location of the normal identifier.
    #[cfg_attr(feature = "datagen", serde(borrow))]
    pub available_id_index: ZeroAsciiIgnoreCaseTrie<ZeroVec<'data, u8>>,
    /// A "links" table mapping non-canonical IDs to their canonical IDs
    #[cfg_attr(feature = "datagen", serde(borrow))]
    pub non_canonical_identifiers: ZeroVec<'data, (u32, u32)>,
    /// The normalized IANA identifier
    #[cfg_attr(feature = "datagen", serde(borrow))]
    pub normalized_identifiers: VarZeroVec<'data, str>,
}

/// A simple [`TimeZoneNormalizer`] that uses compiled data.
#[derive(Default, Copy, Clone, Debug)]
pub struct CompiledNormalizer;

impl TimeZoneNormalizer for CompiledNormalizer {
    fn normalized(&self, identifier: &[u8]) -> TimeZoneProviderResult<NormalizedId> {
        SINGLETON_IANA_NORMALIZER
            .available_id_index
            .get(identifier)
            .map(NormalizedId)
            .ok_or(TimeZoneProviderError::Range("Unknown time zone identifier"))
    }

    fn canonicalized(&self, index: NormalizedId) -> TimeZoneProviderResult<NormalizedId> {
        let Ok(u32_index) = u32::try_from(index.0) else {
            return Ok(index);
        };
        let Ok(canonicalized_idx) = SINGLETON_IANA_NORMALIZER
            .non_canonical_identifiers
            .binary_search_by(|probe| probe.0.cmp(&u32_index))
        else {
            return Ok(index);
        };

        Ok(NormalizedId(
            usize::try_from(
                SINGLETON_IANA_NORMALIZER
                    .non_canonical_identifiers
                    .get(canonicalized_idx)
                    .ok_or(TimeZoneProviderError::Range("Unknown time zone identifier"))?
                    .1,
            )
            .unwrap_or(0),
        ))
    }

    fn identifier(&self, index: NormalizedId) -> TimeZoneProviderResult<&str> {
        SINGLETON_IANA_NORMALIZER
            .normalized_identifiers
            .get(index.0)
            .ok_or(TimeZoneProviderError::Range("Unknown time zone identifier"))
    }
}
