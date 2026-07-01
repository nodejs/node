// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data for reverse folding

#[cfg(feature = "datagen")]
use alloc::string::String;
use icu_provider::prelude::*;
use potential_utf::PotentialUtf8;
use zerovec::ZeroMap;

/// Reverse case folding data. Maps from multi-character strings back
/// to code-points that fold to those strings.
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. While the serde representation of data structs is guaranteed
/// to be stable, their Rust representation might not be. Use with caution.
/// </div>
#[cfg_attr(feature = "serde", derive(serde::Deserialize))]
#[cfg_attr(feature = "datagen", derive(serde::Serialize, databake::Bake))]
#[cfg_attr(feature = "datagen", databake(path = icu_casemap::provider))]
#[derive(Debug, PartialEq, Clone, yoke::Yokeable, zerofrom::ZeroFrom)]
#[yoke(prove_covariance_manually)]
pub struct CaseMapUnfold<'data> {
    #[cfg_attr(feature = "serde", serde(borrow))]
    /// The actual map. Maps from strings to a list of codepoints, stored as a contiguous UTF-8 string
    pub map: ZeroMap<'data, PotentialUtf8, str>,
}

icu_provider::data_struct!(
    CaseMapUnfold<'_>,
    #[cfg(feature = "datagen")]
);

impl CaseMapUnfold<'_> {
    /// Creates a new [`CaseMapUnfold`] using data exported by the `icuexportdata` tool in ICU4C.
    ///
    /// Unfold data is exported by ICU as an array of 16-bit values, representing a short
    /// header followed by a two-column key/value table. The header indicates:
    /// - The number of rows.
    /// - The number of UTF16 code units per row.
    /// - The number of UTF16 code units in the first (key) column.
    ///   (The number of code units in the value column can be derived from the above.)
    ///
    /// The key in the first column is the case folding of each of the code points in
    /// the second column. Keys/values that are shorter than the column width are
    /// null-terminated. The table is sorted by key. Binary search is used to find the value.
    ///
    /// Rust strings are UTF8 by default. To avoid the cost of converting from UTF16 on access,
    /// we convert the ICU data into a more convenient format during construction.
    #[cfg(feature = "datagen")]
    #[expect(clippy::indexing_slicing)] // panics are ok in datagen
    pub fn try_from_icu(raw: &[u16]) -> Result<Self, DataError> {
        const ROWS_INDEX: usize = 0;
        const ROW_WIDTH_INDEX: usize = 1;
        const STRING_WIDTH_INDEX: usize = 2;

        if raw.len() <= STRING_WIDTH_INDEX {
            return Err(DataError::custom("Unfold: header missing"));
        }

        let num_rows = raw[ROWS_INDEX] as usize;
        let row_width = raw[ROW_WIDTH_INDEX] as usize;
        let string_width = raw[STRING_WIDTH_INDEX] as usize;

        if row_width == 0 {
            return Err(DataError::custom("Unfold: invalid row width"));
        }

        // Header takes up one row.
        let row_data = &raw[row_width..];

        let mut map = ZeroMap::new();

        debug_assert!(num_rows == row_data.chunks_exact(row_width).count());
        for row in row_data.chunks_exact(row_width) {
            let key = Self::decode_string(&row[..string_width])
                .ok_or(DataError::custom("Unfold: unpaired surrogate in key"))?;
            let val = Self::decode_string(&row[string_width..])
                .ok_or(DataError::custom("Unfold: unpaired surrogate in value"))?;
            if map
                .try_append(PotentialUtf8::from_str(&key), val.as_ref())
                .is_some()
            {
                return Err(DataError::custom("Unfold: keys not sorted/unique"));
            }
        }
        Ok(Self { map })
    }

    // Decode a zero-terminated UTF16 string from a slice of u16.
    #[cfg(feature = "datagen")]
    pub(crate) fn decode_string(slice: &[u16]) -> Option<String> {
        let iter = slice.iter().copied().take_while(|&c| c != 0);
        char::decode_utf16(iter).collect::<Result<String, _>>().ok()
    }

    // Given a string, returns another string representing the set of characters
    // that case fold to that string.
    pub(crate) fn get(&self, key: &str) -> Option<&str> {
        self.map.get(PotentialUtf8::from_str(key))
    }
}
