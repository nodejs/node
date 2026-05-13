// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use displaydoc::Display;

/// Error types for the `zerotrie` crate.
#[derive(Debug, Copy, Clone, PartialEq, Eq, Display)]
#[non_exhaustive]
pub enum ZeroTrieBuildError {
    /// Non-ASCII data was added to an ASCII-only trie.
    #[displaydoc("Non-ASCII cannot be added to an ASCII-only trie")]
    NonAsciiError,
    /// The trie reached its maximum supported capacity.
    #[displaydoc("Reached maximum capacity of trie")]
    CapacityExceeded,
    /// The builder could not solve the perfect hash function.
    #[displaydoc("Failed to solve the perfect hash function. This is rare! Please report your case to the ICU4X team.")]
    CouldNotSolvePerfectHash,
    /// Mixed-case data was added to a case-insensitive trie.
    #[displaydoc("Mixed-case data added to case-insensitive trie")]
    MixedCase,
    /// Strings were added to a trie containing the delimiter.
    ///
    /// # Examples
    ///
    /// ```
    /// use std::collections::BTreeMap;
    /// use zerotrie::dense::ZeroAsciiDenseSparse2dTrieOwned;
    /// use zerotrie::ZeroTrieBuildError;
    ///
    /// let mut data: BTreeMap<&str, BTreeMap<&str, usize>> = BTreeMap::new();
    ///
    /// // Delimiter in a prefix
    /// data.entry("aa/bb").or_default().insert("CCC", 1);
    /// let err =
    ///     ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/')
    ///         .unwrap_err();
    /// assert_eq!(err, ZeroTrieBuildError::IllegalDelimiter);
    ///
    /// // Delimiter in a suffix
    /// data.clear();
    /// data.entry("aaa").or_default().insert("BB/CC", 1);
    /// let err =
    ///     ZeroAsciiDenseSparse2dTrieOwned::try_from_btree_map_str(&data, b'/')
    ///         .unwrap_err();
    /// assert_eq!(err, ZeroTrieBuildError::IllegalDelimiter);
    /// ```
    #[displaydoc("Delimiter is contained in one or more strings")]
    IllegalDelimiter,
}

impl core::error::Error for ZeroTrieBuildError {}
