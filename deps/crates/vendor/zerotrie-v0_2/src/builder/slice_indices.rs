// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

#[cfg(feature = "alloc")]
use alloc::vec::Vec;

/// Const function to evaluate `a < b` lexicographically.
const fn is_less_than(a: &[u8], b: &[u8]) -> bool {
    let mut i = 0;
    #[allow(clippy::indexing_slicing, reason = "bounds checked in while loop")]
    while i < a.len() && i < b.len() {
        if a[i] < b[i] {
            return true;
        }
        if a[i] > b[i] {
            return false;
        }
        i += 1;
    }
    a.len() < b.len()
}

/// Const function to evaluate `a[..prefix_len] == b[..prefix_len]`
///
/// # Panics
///
/// Panics if `prefix_len` is longer than either `a` or `b`.
pub(crate) const fn prefix_eq_or_panic(a: &[u8], b: &[u8], prefix_len: usize) -> bool {
    let mut i = 0;
    #[allow(clippy::indexing_slicing, reason = "documented panic")]
    while i < prefix_len {
        if a[i] != b[i] {
            return false;
        }
        i += 1;
    }
    true
}

/// An abstraction over a slice of key-value pairs that can have either `&[u8]`
/// or `&str` keys. This is used in the builder to avoid unsound casts.
#[derive(Copy, Clone)]
pub(crate) enum ByteSliceWithIndices<'a> {
    Bytes(&'a [(&'a [u8], usize)]),
    Str(&'a [(&'a str, usize)]),
}

impl<'a> ByteSliceWithIndices<'a> {
    pub const fn from_byte_slice(s: &'a [(&'a [u8], usize)]) -> Self {
        Self::Bytes(s)
    }

    pub const fn from_str_slice(s: &'a [(&'a str, usize)]) -> Self {
        Self::Str(s)
    }

    pub const fn len(&self) -> usize {
        match self {
            Self::Bytes(s) => s.len(),
            Self::Str(s) => s.len(),
        }
    }

    /// Gets the bytes and index at `index`
    ///
    /// # Panics
    ///
    /// Panics when `index >= self.len()`
    #[allow(clippy::indexing_slicing, reason = "documented")]
    pub const fn get_or_panic(&self, index: usize) -> (&'a [u8], usize) {
        match self {
            Self::Bytes(s) => s[index],
            Self::Str(s) => {
                let (key, value) = s[index];
                (key.as_bytes(), value)
            }
        }
    }

    pub const fn last(&self) -> Option<(&'a [u8], usize)> {
        if self.len() == 0 {
            None
        } else {
            Some(self.get_or_panic(self.len() - 1))
        }
    }

    #[cfg(feature = "alloc")]
    pub fn to_vec_u8(self) -> Vec<(&'a [u8], usize)> {
        match self {
            Self::Bytes(s) => s.to_vec(),
            Self::Str(s) => s.iter().map(|(k, v)| (k.as_bytes(), *v)).collect(),
        }
    }

    pub const fn is_sorted(self) -> bool {
        let mut i = 0;
        let mut prev: Option<&'a [u8]> = None;

        while i < self.len() {
            let (ascii_str, _) = self.get_or_panic(i);
            match prev {
                None => (),
                Some(prev) => {
                    if !is_less_than(prev, ascii_str) {
                        return false;
                    }
                }
            };
            prev = Some(ascii_str);
            i += 1;
        }
        true
    }

    #[cfg(feature = "alloc")]
    pub fn is_all_ascii(&self) -> bool {
        match self {
            Self::Bytes(s) => s
                .iter()
                .all(|(slice, _)| slice.iter().all(|c| c.is_ascii())),
            Self::Str(s) => s
                .iter()
                .all(|(slice, _)| slice.as_bytes().iter().all(|c| c.is_ascii())),
        }
    }
}
