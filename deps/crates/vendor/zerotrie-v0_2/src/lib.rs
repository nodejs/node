// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, doc)), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
    )
)]
#![warn(missing_docs)]

//! A data structure offering zero-copy storage and retrieval of byte strings, with a focus
//! on the efficient storage of ASCII strings. Strings are mapped to `usize` values.
//!
//! [`ZeroTrie`] does not support mutation because doing so would require recomputing the entire
//! data structure. Instead, it supports conversion to and from [`LiteMap`] and [`BTreeMap`].
//!
//! There are multiple variants of [`ZeroTrie`] optimized for different use cases.
//!
//! # Safe Rust
//!
//! All runtime lookup code in this crate is 100% safe Rust.
//!
//! A small amount of unsafe Rust is used in these situations:
//!
//! - Constructing unsized transparent newtypes (i.e. <https://github.com/rust-lang/rust/issues/18806>),
//!   which is reachable from builder code and when creating a `&TypedZeroTrie<[u8]>` DST.
//! - Implementing unsafe traits when the `zerovec` feature is enabled
//!
//! # Examples
//!
//! ```
//! use zerotrie::ZeroTrie;
//!
//! let data: &[(&str, usize)] = &[("abc", 11), ("xyz", 22), ("axyb", 33)];
//!
//! let trie: ZeroTrie<Vec<u8>> = data.iter().copied().collect();
//!
//! assert_eq!(trie.get("axyb"), Some(33));
//! assert_eq!(trie.byte_len(), 18);
//! ```
//!
//! # Internal Structure
//!
//! To read about the internal structure of [`ZeroTrie`], build the docs with private modules:
//!
//! ```bash
//! cargo doc --document-private-items --all-features --no-deps --open
//! ```
//!
//! [`LiteMap`]: litemap::LiteMap
//! [`BTreeMap`]: alloc::collections::BTreeMap

// To back up the claim in the docs:
#![deny(unsafe_code)]

#[cfg(feature = "alloc")]
extern crate alloc;

mod builder;
mod byte_phf;
pub mod cursor;
#[cfg(feature = "dense")]
pub mod dense;
mod error;
#[macro_use]
mod helpers;
mod options;
mod reader;
#[cfg(feature = "serde")]
mod serde;
mod varint;
mod zerotrie;

pub use crate::zerotrie::ZeroAsciiIgnoreCaseTrie;
pub use crate::zerotrie::ZeroTrie;
pub use crate::zerotrie::ZeroTrieExtendedCapacity;
pub use crate::zerotrie::ZeroTriePerfectHash;
pub use crate::zerotrie::ZeroTrieSimpleAscii;
pub use error::ZeroTrieBuildError;

#[cfg(feature = "alloc")]
pub use crate::zerotrie::ZeroTrieStringIterator;
#[cfg(feature = "alloc")]
pub use reader::ZeroTrieIterator;

#[doc(hidden)]
pub mod _internal {
    pub use crate::byte_phf::f1;
    pub use crate::byte_phf::f2;
    pub use crate::byte_phf::PerfectByteHashMap;
}
