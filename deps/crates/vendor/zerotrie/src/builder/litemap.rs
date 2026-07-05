// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Impls for functions gated on the "litemap" feature.

use super::konst::*;
use crate::builder::slice_indices::ByteSliceWithIndices;
use crate::error::ZeroTrieBuildError;
use crate::zerotrie::ZeroTrieSimpleAscii;
use crate::ZeroTrie;
use alloc::vec::Vec;
use core::borrow::Borrow;
use litemap::LiteMap;

impl ZeroTrieSimpleAscii<Vec<u8>> {
    #[doc(hidden)]
    pub fn try_from_litemap_with_const_builder<'a, S>(
        items: &LiteMap<&'a [u8], usize, S>,
    ) -> Result<Self, ZeroTrieBuildError>
    where
        S: litemap::store::StoreSlice<&'a [u8], usize, Slice = [(&'a [u8], usize)]>,
    {
        let tuples = items.as_slice();
        let byte_str_slice = ByteSliceWithIndices::from_byte_slice(tuples);

        Ok(Self {
            store: ZeroTrieBuilderConst::<10000>::from_slice_with_indices::<100>(byte_str_slice)
                .as_bytes()
                .to_vec(),
        })
    }
}

impl<K, S> TryFrom<&LiteMap<K, usize, S>> for ZeroTrie<Vec<u8>>
where
    // Borrow, not AsRef, because we rely on Ord being the same. Unfortunately
    // this means `LiteMap<&str, usize>` does not work.
    K: Borrow<[u8]>,
    S: litemap::store::StoreSlice<K, usize, Slice = [(K, usize)]>,
{
    type Error = ZeroTrieBuildError;
    fn try_from(items: &LiteMap<K, usize, S>) -> Result<Self, ZeroTrieBuildError> {
        let byte_litemap = items.to_borrowed_keys::<[u8], Vec<_>>();
        let byte_slice = byte_litemap.as_slice();
        let byte_str_slice = ByteSliceWithIndices::from_byte_slice(byte_slice);
        Self::try_from_tuple_slice(byte_str_slice)
    }
}

#[cfg(feature = "serde")]
impl TryFrom<&LiteMap<crate::serde::SerdeByteStrOwned, usize>> for ZeroTrie<Vec<u8>> {
    type Error = ZeroTrieBuildError;
    fn try_from(
        items: &LiteMap<crate::serde::SerdeByteStrOwned, usize>,
    ) -> Result<Self, ZeroTrieBuildError> {
        let tuples: Vec<(&[u8], usize)> = items.iter().map(|(k, v)| (k.as_bytes(), *v)).collect();
        let byte_str_slice = ByteSliceWithIndices::from_byte_slice(&tuples);
        Self::try_from_tuple_slice(byte_str_slice)
    }
}

// TODO(#7084): Make this more infallible by calculating the required length,
// heap-allocating the required capacity, and pointing ConstAsciiTrieBuilderStore
// to the heap buffer.
