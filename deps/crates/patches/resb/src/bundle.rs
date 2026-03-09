// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The `bundle` module provides data structures for working directly with the
//! contents of a resource bundle.
//!
//! WARNING: This module is not suitable for use at runtime due to its reliance
//! on `std` and `alloc` and therefore not intended for general deserialization
//! of resource bundles. Rather, it is intended to be used in development-time
//! tools for working with bundles.

extern crate alloc;
use alloc::{borrow::Cow, collections::BTreeMap};

use crate::MASK_28_BIT;

/// A tree-like collection of data [`Resource`]s primarily intended for storing
/// locale and other internationalization data for [ICU] (International
/// Components for Unicode).
///
/// [ICU]: https://icu.unicode.org/
#[derive(Debug)]
pub struct ResourceBundle<'a> {
    name: Cow<'a, str>,
    root: Resource<'a>,

    /// Whether fallback is enabled for this resource bundle.
    ///
    /// A resource bundle storing locale data may omit some data in order to
    /// reduce duplication, allowing fallback to more general locales which
    /// use the same values.
    pub is_locale_fallback_enabled: bool,
}

impl<'a> ResourceBundle<'a> {
    /// Makes a new resource bundle with the specified resource at its root.
    pub fn new(name: Cow<'a, str>, root: Resource<'a>, is_locale_fallback_enabled: bool) -> Self {
        Self {
            name,
            root,
            is_locale_fallback_enabled,
        }
    }

    /// Gets the name of the resource bundle.
    ///
    /// This name is used as the "key" of the root resource in a text format
    /// bundle, but is not used in building binary resource bundles.
    pub fn name(&self) -> &str {
        &self.name
    }

    /// Gets the root resource in the resource tree.
    pub fn root(&self) -> &Resource<'_> {
        &self.root
    }
}

/// A data resource within a [`ResourceBundle`].
#[derive(Debug)]
#[non_exhaustive]
pub enum Resource<'a> {
    /// A well-formed UTF-8 string.
    String(Cow<'a, str>),

    /// A heterogeneous list of resources, ordered by insertion.
    Array(Vec<Resource<'a>>),

    /// A set of key-resource pairs, sorted lexically by key.
    Table(Table<'a>),

    /// A slice of arbitrary binary data.
    Binary(Cow<'a, [u8]>),

    /// A 28-bit integer.
    ///
    /// May be interpreted as either signed or unsigned depending on consumer
    /// expectations. See [`Int28`] for further details.
    Integer(Int28),

    /// A list of 32-bit integers, ordered by insertion.
    IntVector(Vec<u32>),
}

/// A table of [`Resource`]s indexed by a string-based [`Key`].
// Ordering of keys in a table is significant, so `HashMap` isn't appropriate
// here.
pub type Table<'a> = BTreeMap<Key<'a>, Resource<'a>>;

/// A key for a [`Resource`] within a [`Table`].
#[derive(Clone, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct Key<'a>(Cow<'a, str>);

impl Key<'_> {
    /// Converts the string representing the key into a slice of UTF-8 bytes.
    pub fn as_bytes(&self) -> &[u8] {
        self.0.as_bytes()
    }
}

impl<'a> From<&'a str> for Key<'a> {
    fn from(value: &'a str) -> Self {
        Self(Cow::from(value))
    }
}

impl From<String> for Key<'_> {
    fn from(value: String) -> Self {
        Self(Cow::from(value))
    }
}

impl<'a> From<Key<'a>> for String {
    fn from(value: Key<'a>) -> Self {
        value.0.into_owned()
    }
}

/// A 28-bit integer of undetermined signedness.
///
/// [`Resource`]s may include 28-bit integers whose signedness is determined at
/// runtime by consumers. Because these integers are stored in a 32-bit value,
/// negative values in signed integers require special handling, provided by
/// this newtype wrapper.
#[derive(Copy, Clone, Debug, PartialEq)]
pub struct Int28(u32);

impl From<Int28> for i32 {
    fn from(value: Int28) -> Self {
        ((value.0 as i32) << 4) >> 4
    }
}

impl From<Int28> for u32 {
    fn from(value: Int28) -> Self {
        value.0
    }
}

impl From<i32> for Int28 {
    fn from(value: i32) -> Self {
        Self::from(value as u32)
    }
}

impl From<u32> for Int28 {
    fn from(value: u32) -> Self {
        Self(value & MASK_28_BIT)
    }
}
