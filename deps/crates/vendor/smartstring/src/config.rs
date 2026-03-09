// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use crate::{boxed::BoxedString, inline::InlineString, SmartString};
use alloc::string::String;
use core::mem::{align_of, size_of};
use static_assertions::{assert_eq_align, assert_eq_size, const_assert, const_assert_eq};

/// A compact string representation equal to [`String`] in size with guaranteed inlining.
///
/// This representation relies on pointer alignment to be able to store a discriminant bit in its
/// inline form that will never be present in its [`String`] form, thus
/// giving us 24 bytes on 64-bit architectures, and 12 bytes on 32-bit, minus one bit, to encode our
/// inline string. It uses the rest of the discriminant bit's byte to encode the string length, and
/// the remaining bytes (23 or 11 depending on arch) to store the string data. When the available space is exceeded,
/// it swaps itself out with a [`String`] containing its previous
/// contents, relying on the discriminant bit in the [`String`]'s pointer to be unset, so we can
/// store the [`String`] safely without taking up any extra space for a discriminant.
///
/// This performs generally as well as [`String`] on all ops on boxed strings, and
/// better than [`String`]s on inlined strings.
#[derive(Debug)]
pub struct Compact;

/// A representation similar to [`Compact`] but which doesn't re-inline strings.
///
/// This is a variant of [`Compact`] which doesn't aggressively inline strings.
/// Where [`Compact`] automatically turns a heap allocated string back into an
/// inlined string if it should become short enough, [`LazyCompact`] keeps
/// it heap allocated once heap allocation has occurred. If your aim is to defer heap
/// allocation as much as possible, rather than to ensure cache locality, this is the
/// variant you want - it won't allocate until the inline capacity is exceeded, and it
/// also won't deallocate once allocation has occurred, which risks reallocation if the
/// string exceeds its inline capacity in the future.
#[derive(Debug)]
pub struct LazyCompact;

/// Marker trait for [`SmartString`] representations.
///
/// See [`LazyCompact`] and [`Compact`].
pub trait SmartStringMode {
    /// The inline string type for this layout.
    type InlineArray: AsRef<[u8]> + AsMut<[u8]> + Clone + Copy;
    /// A constant to decide whether to turn a wrapped string back into an inlined
    /// string whenever possible (`true`) or leave it as a wrapped string once wrapping
    /// has occurred (`false`).
    const DEALLOC: bool;
}

impl SmartStringMode for Compact {
    type InlineArray = [u8; size_of::<String>() - 1];
    const DEALLOC: bool = true;
}

impl SmartStringMode for LazyCompact {
    type InlineArray = [u8; size_of::<String>() - 1];
    const DEALLOC: bool = false;
}

/// The maximum capacity of an inline string, in bytes.
pub const MAX_INLINE: usize = size_of::<String>() - 1;

// Assert that we're not using more space than we can encode in the header byte,
// just in case we're on a 1024-bit architecture.
const_assert!(MAX_INLINE < 128);

// Assert that all the structs are of the expected size.
assert_eq_size!(BoxedString, SmartString<Compact>);
assert_eq_size!(BoxedString, SmartString<LazyCompact>);
assert_eq_size!(InlineString, SmartString<Compact>);
assert_eq_size!(InlineString, SmartString<LazyCompact>);

assert_eq_align!(BoxedString, String);
assert_eq_align!(InlineString, String);
assert_eq_align!(SmartString<Compact>, String);
assert_eq_align!(SmartString<LazyCompact>, String);

assert_eq_size!(String, SmartString<Compact>);
assert_eq_size!(String, SmartString<LazyCompact>);

// Assert that `SmartString` is aligned correctly.
const_assert_eq!(align_of::<String>(), align_of::<SmartString<Compact>>());
const_assert_eq!(align_of::<String>(), align_of::<SmartString<LazyCompact>>());
