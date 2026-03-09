// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//! # Smart String
//!
//! [`SmartString`] is a wrapper around [`String`] which offers
//! automatic inlining of small strings. It comes in two flavours:
//! [`LazyCompact`], which takes up exactly as much space as a [`String`]
//! and is generally a little faster, and [`Compact`], which is the same as
//! [`LazyCompact`] except it will aggressively re-inline any expanded
//! [`String`]s which become short enough to do so.
//! [`LazyCompact`] is the default, and what you should be using unless
//! you care considerably more about heap memory usage than performance.
//!
//! ## What Is It For?
//!
//! The intended use for [`SmartString`] is as a key type for a
//! B-tree (such as [`std::collections::BTreeMap`]) or any kind of
//! array operation where cache locality is critical.
//!
//! In general, it's a nice data type for reducing your heap allocations and
//! increasing the locality of string data. If you use [`SmartString`]
//! as a drop-in replacement for [`String`], you're almost certain to see
//! a slight performance boost, as well as slightly reduced memory usage.
//!
//! ## How To Use It?
//!
//! [`SmartString`] has the exact same API as [`String`],
//! all the clever bits happen automatically behind the scenes, so you could just:
//!
//! ```rust
//! use smartstring::alias::String;
//! use std::fmt::Write;
//!
//! let mut string = String::new();
//! string.push_str("This is just a string!");
//! string.clear();
//! write!(string, "Hello Joe!");
//! assert_eq!("Hello Joe!", string);
//! ```
//!
//! ## Give Me The Details
//!
//! [`SmartString`] is the same size as [`String`] and
//! relies on pointer alignment to be able to store a discriminant bit in its
//! inline form that will never be present in its [`String`] form, thus
//! giving us 24 bytes (on 64-bit architectures) minus one bit to encode our
//! inline string. It uses 23 bytes to store the string data and the remaining
//! 7 bits to encode the string's length. When the available space is exceeded,
//! it swaps itself out with a boxed string type containing its previous
//! contents. Likewise, if the string's length should drop below its inline
//! capacity again, it deallocates the string and moves its contents inline.
//!
//! In [`Compact`] mode, it is aggressive about inlining strings, meaning that if you modify a heap allocated
//! string such that it becomes short enough for inlining, it will be inlined immediately
//! and the allocated [`String`] will be dropped. This may cause multiple
//! unintended allocations if you repeatedly adjust your string's length across the
//! inline capacity threshold, so if your string's construction can get
//! complicated and you're relying on performance during construction, it might be better
//! to construct it as a [`String`] and convert it once construction is done.
//!
//! [`LazyCompact`] looks the same as [`Compact`], except
//! it never re-inlines a string that's already been heap allocated, instead
//! keeping the allocation around in case it needs it. This makes for less
//! cache local strings, but is the best choice if you're more worried about
//! time spent on unnecessary allocations than cache locality.
//!
//! ## Performance
//!
//! It doesn't aim to be more performant than [`String`] in the general case,
//! except that it doesn't trigger heap allocations for anything shorter than
//! its inline capacity and so can be reasonably expected to exceed
//! [`String`]'s performance perceptibly on shorter strings, as well as being more
//! memory efficient in these cases. There will always be a slight overhead on all
//! operations on boxed strings, compared to [`String`].
//!
//! ## Feature Flags
//!
//! `smartstring` comes with optional support for the following crates through Cargo
//! feature flags. You can enable them in your `Cargo.toml` file like this:
//!
//! ```no_compile
//! [dependencies]
//! smartstring = { version = "*", features = ["proptest", "serde"] }
//! ```
//!
//! | Feature | Description |
//! | ------- | ----------- |
//! | [`arbitrary`](https://crates.io/crates/arbitrary) | [`Arbitrary`][Arbitrary] implementation for [`SmartString`]. |
//! | [`proptest`](https://crates.io/crates/proptest) | A strategy for generating [`SmartString`]s from a regular expression. |
//! | [`serde`](https://crates.io/crates/serde) | [`Serialize`][Serialize] and [`Deserialize`][Deserialize] implementations for [`SmartString`]. |
//!
//! [Serialize]: https://docs.rs/serde/latest/serde/trait.Serialize.html
//! [Deserialize]: https://docs.rs/serde/latest/serde/trait.Deserialize.html
//! [Arbitrary]: https://docs.rs/arbitrary/latest/arbitrary/trait.Arbitrary.html

// Ensure all unsafe blocks get flagged for manual validation.
#![deny(unsafe_code)]
#![forbid(rust_2018_idioms)]
#![deny(nonstandard_style)]
#![warn(unreachable_pub, missing_debug_implementations, missing_docs)]
#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(needs_allocator_feature, feature(allocator_api))]

extern crate alloc;

use alloc::{
    boxed::Box,
    string::{String, ToString},
};
use core::{
    borrow::{Borrow, BorrowMut},
    cmp::Ordering,
    convert::Infallible,
    fmt::{Debug, Display, Error, Formatter, Write},
    hash::{Hash, Hasher},
    iter::FromIterator,
    marker::PhantomData,
    mem::{forget, MaybeUninit},
    ops::{
        Add, Deref, DerefMut, Index, IndexMut, Range, RangeBounds, RangeFrom, RangeFull,
        RangeInclusive, RangeTo, RangeToInclusive,
    },
    ptr::drop_in_place,
    str::FromStr,
};

#[cfg(feature = "std")]
use std::borrow::Cow;

mod config;
pub use config::{Compact, LazyCompact, SmartStringMode, MAX_INLINE};

mod marker_byte;
use marker_byte::Discriminant;

mod inline;
use inline::InlineString;

mod boxed;
use boxed::BoxedString;

mod casts;
use casts::{StringCast, StringCastInto, StringCastMut};

mod iter;
pub use iter::Drain;

mod ops;
use ops::{string_op_grow, string_op_shrink};

#[cfg(feature = "serde")]
mod serde;

#[cfg(feature = "arbitrary")]
mod arbitrary;

#[cfg(feature = "proptest")]
pub mod proptest;

/// Convenient type aliases.
pub mod alias {
    use super::*;

    /// A convenience alias for a [`LazyCompact`] layout [`SmartString`].
    ///
    /// Just pretend it's a [`String`][String]!
    pub type String = SmartString<LazyCompact>;

    /// A convenience alias for a [`Compact`] layout [`SmartString`].
    pub type CompactString = SmartString<Compact>;
}

/// A smart string.
///
/// This wraps one of two string types: an inline string or a boxed string.
/// Conversion between the two happens opportunistically and transparently.
///
/// It takes a layout as its type argument: one of [`Compact`] or [`LazyCompact`].
///
/// It mimics the interface of [`String`] except where behaviour cannot
/// be guaranteed to stay consistent between its boxed and inline states. This means
/// you still have `capacity()` and `shrink_to_fit()`, relating to state that only
/// really exists in the boxed variant, because the inline variant can still give
/// sensible behaviour for these operations, but `with_capacity()`, `reserve()` etc are
/// absent, because they would have no effect on inline strings and the requested
/// state changes wouldn't carry over if the inline string is promoted to a boxed
/// one - not without also storing that state in the inline representation, which
/// would waste precious bytes for inline string data.
pub struct SmartString<Mode: SmartStringMode> {
    data: MaybeUninit<InlineString>,
    mode: PhantomData<Mode>,
}

impl<Mode: SmartStringMode> Drop for SmartString<Mode> {
    fn drop(&mut self) {
        if let StringCastMut::Boxed(string) = self.cast_mut() {
            #[allow(unsafe_code)]
            unsafe {
                drop_in_place(string)
            };
        }
    }
}

impl<Mode: SmartStringMode> Clone for SmartString<Mode> {
    /// Clone a [`SmartString`].
    ///
    /// If the string is inlined, this is a [`Copy`] operation. Otherwise,
    /// a string with the same capacity as the source is allocated.
    fn clone(&self) -> Self {
        match self.cast() {
            StringCast::Boxed(string) => Self::from_boxed(string.clone()),
            StringCast::Inline(string) => Self::from_inline(*string),
        }
    }
}

impl<Mode: SmartStringMode> Deref for SmartString<Mode> {
    type Target = str;

    #[inline(always)]
    fn deref(&self) -> &Self::Target {
        match self.cast() {
            StringCast::Boxed(string) => string.deref(),
            StringCast::Inline(string) => string.deref(),
        }
    }
}

impl<Mode: SmartStringMode> DerefMut for SmartString<Mode> {
    #[inline(always)]
    fn deref_mut(&mut self) -> &mut Self::Target {
        match self.cast_mut() {
            StringCastMut::Boxed(string) => string.deref_mut(),
            StringCastMut::Inline(string) => string.deref_mut(),
        }
    }
}

impl SmartString<LazyCompact> {
    /// Construct an empty string.
    ///
    /// This is a `const fn` version of [`SmartString::new`].
    /// It's a temporary measure while we wait for trait bounds on
    /// type arguments to `const fn`s to stabilise, and will be deprecated
    /// once this happens.
    pub const fn new_const() -> Self {
        Self {
            data: MaybeUninit::new(InlineString::new()),
            mode: PhantomData,
        }
    }
}

impl SmartString<Compact> {
    /// Construct an empty string.
    ///
    /// This is a `const fn` version of [`SmartString::new`].
    /// It's a temporary measure while we wait for trait bounds on
    /// type arguments to `const fn`s to stabilise, and will be deprecated
    /// once this happens.
    pub const fn new_const() -> Self {
        Self {
            data: MaybeUninit::new(InlineString::new()),
            mode: PhantomData,
        }
    }
}

impl<Mode: SmartStringMode> SmartString<Mode> {
    /// Construct an empty string.
    #[inline(always)]
    pub fn new() -> Self {
        Self::from_inline(InlineString::new())
    }

    fn from_boxed(boxed: BoxedString) -> Self {
        let mut out = Self {
            data: MaybeUninit::uninit(),
            mode: PhantomData,
        };
        let data_ptr: *mut BoxedString = out.data.as_mut_ptr().cast();
        #[allow(unsafe_code)]
        unsafe {
            data_ptr.write(boxed)
        };
        out
    }

    fn from_inline(inline: InlineString) -> Self {
        Self {
            data: MaybeUninit::new(inline),
            mode: PhantomData,
        }
    }

    fn discriminant(&self) -> Discriminant {
        // unsafe { self.data.assume_init() }.marker.discriminant()
        let str_ptr: *const BoxedString =
            self.data.as_ptr().cast() as *const _ as *const BoxedString;
        #[allow(unsafe_code)]
        Discriminant::from_bit(BoxedString::check_alignment(unsafe { &*str_ptr }))
    }

    fn cast(&self) -> StringCast<'_> {
        #[allow(unsafe_code)]
        match self.discriminant() {
            Discriminant::Inline => StringCast::Inline(unsafe { &*self.data.as_ptr() }),
            Discriminant::Boxed => StringCast::Boxed(unsafe { &*self.data.as_ptr().cast() }),
        }
    }

    fn cast_mut(&mut self) -> StringCastMut<'_> {
        #[allow(unsafe_code)]
        match self.discriminant() {
            Discriminant::Inline => StringCastMut::Inline(unsafe { &mut *self.data.as_mut_ptr() }),
            Discriminant::Boxed => {
                StringCastMut::Boxed(unsafe { &mut *self.data.as_mut_ptr().cast() })
            }
        }
    }

    fn cast_into(mut self) -> StringCastInto {
        #[allow(unsafe_code)]
        match self.discriminant() {
            Discriminant::Inline => StringCastInto::Inline(unsafe { self.data.assume_init() }),
            Discriminant::Boxed => StringCastInto::Boxed(unsafe {
                let boxed_ptr: *mut BoxedString = self.data.as_mut_ptr().cast();
                let string = boxed_ptr.read();
                forget(self);
                string
            }),
        }
    }

    fn promote_from(&mut self, string: BoxedString) {
        debug_assert!(self.discriminant() == Discriminant::Inline);
        let data: *mut BoxedString = self.data.as_mut_ptr().cast();
        #[allow(unsafe_code)]
        unsafe {
            data.write(string)
        };
    }

    /// Attempt to inline the string if it's currently heap allocated.
    ///
    /// Returns the resulting state: `true` if it's inlined, `false` if it's not.
    fn try_demote(&mut self) -> bool {
        if Mode::DEALLOC {
            self.really_try_demote()
        } else {
            false
        }
    }

    /// Attempt to inline the string regardless of whether `Mode::DEALLOC` is set.
    fn really_try_demote(&mut self) -> bool {
        if let StringCastMut::Boxed(string) = self.cast_mut() {
            if string.len() > MAX_INLINE {
                false
            } else {
                let s: &str = string.deref();
                let inlined = s.into();
                #[allow(unsafe_code)]
                unsafe {
                    drop_in_place(string);
                    self.data.as_mut_ptr().write(inlined);
                }
                true
            }
        } else {
            true
        }
    }

    /// Return the length in bytes of the string.
    ///
    /// Note that this may differ from the length in `char`s.
    pub fn len(&self) -> usize {
        match self.cast() {
            StringCast::Boxed(string) => string.len(),
            StringCast::Inline(string) => string.len(),
        }
    }

    /// Test whether the string is empty.
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Test whether the string is currently inlined.
    pub fn is_inline(&self) -> bool {
        self.discriminant() == Discriminant::Inline
    }

    /// Get a reference to the string as a string slice.
    pub fn as_str(&self) -> &str {
        self.deref()
    }

    /// Get a reference to the string as a mutable string slice.
    pub fn as_mut_str(&mut self) -> &mut str {
        self.deref_mut()
    }

    /// Return the currently allocated capacity of the string.
    ///
    /// Note that if this is a boxed string, it returns [`String::capacity()`][String::capacity],
    /// but an inline string always returns [`MAX_INLINE`].
    ///
    /// Note also that if a boxed string is converted into an inline string, its capacity is
    /// deallocated, and if the inline string is promoted to a boxed string in the future,
    /// it will be reallocated with a default capacity.
    pub fn capacity(&self) -> usize {
        if let StringCast::Boxed(string) = self.cast() {
            string.capacity()
        } else {
            MAX_INLINE
        }
    }

    /// Push a character to the end of the string.
    pub fn push(&mut self, ch: char) {
        string_op_grow!(ops::Push, self, ch)
    }

    /// Copy a string slice onto the end of the string.
    pub fn push_str(&mut self, string: &str) {
        string_op_grow!(ops::PushStr, self, string)
    }

    /// Shrink the capacity of the string to fit its contents exactly.
    ///
    /// This has no effect on inline strings, which always have a fixed capacity.
    /// Thus, it's not safe to assume that [`capacity()`][SmartString::capacity] will
    /// equal [`len()`][SmartString::len] after calling this.
    ///
    /// Calling this on a [`LazyCompact`] string that is currently
    /// heap allocated but is short enough to be inlined will deallocate the
    /// heap allocation and convert it to an inline string.
    pub fn shrink_to_fit(&mut self) {
        if let StringCastMut::Boxed(string) = self.cast_mut() {
            if string.len() > MAX_INLINE {
                string.shrink_to_fit();
            }
        }
        self.really_try_demote();
    }

    /// Truncate the string to `new_len` bytes.
    ///
    /// If `new_len` is larger than the string's current length, this does nothing.
    /// If `new_len` isn't on a UTF-8 character boundary, this method panics.
    pub fn truncate(&mut self, new_len: usize) {
        string_op_shrink!(ops::Truncate, self, new_len)
    }

    /// Pop a `char` off the end of the string.
    pub fn pop(&mut self) -> Option<char> {
        string_op_shrink!(ops::Pop, self)
    }

    /// Remove a `char` from the string at the given index.
    ///
    /// If the index doesn't fall on a UTF-8 character boundary, this method panics.
    pub fn remove(&mut self, index: usize) -> char {
        string_op_shrink!(ops::Remove, self, index)
    }

    /// Insert a `char` into the string at the given index.
    ///
    /// If the index doesn't fall on a UTF-8 character boundary, this method panics.
    pub fn insert(&mut self, index: usize, ch: char) {
        string_op_grow!(ops::Insert, self, index, ch)
    }

    /// Insert a string slice into the string at the given index.
    ///
    /// If the index doesn't fall on a UTF-8 character boundary, this method panics.
    pub fn insert_str(&mut self, index: usize, string: &str) {
        string_op_grow!(ops::InsertStr, self, index, string)
    }

    /// Split the string into two at the given index.
    ///
    /// Returns the content to the right of the index as a new string, and removes
    /// it from the original.
    ///
    /// If the index doesn't fall on a UTF-8 character boundary, this method panics.
    pub fn split_off(&mut self, index: usize) -> Self {
        string_op_shrink!(ops::SplitOff<Mode>, self, index)
    }

    /// Clear the string.
    ///
    /// This causes any memory reserved by the string to be immediately deallocated.
    pub fn clear(&mut self) {
        *self = Self::new();
    }

    /// Filter out `char`s not matching a predicate.
    pub fn retain<F>(&mut self, f: F)
    where
        F: FnMut(char) -> bool,
    {
        string_op_shrink!(ops::Retain, self, f)
    }

    /// Construct a draining iterator over a given range.
    ///
    /// This removes the given range from the string, and returns an iterator over the
    /// removed `char`s.
    pub fn drain<R>(&mut self, range: R) -> Drain<'_, Mode>
    where
        R: RangeBounds<usize>,
    {
        Drain::new(self, range)
    }

    /// Replaces a range with the contents of a string slice.
    pub fn replace_range<R>(&mut self, range: R, replace_with: &str)
    where
        R: RangeBounds<usize>,
    {
        string_op_grow!(ops::ReplaceRange, self, &range, replace_with);
        self.try_demote();
    }
}

impl<Mode: SmartStringMode> Default for SmartString<Mode> {
    fn default() -> Self {
        Self::new()
    }
}

impl<Mode: SmartStringMode> AsRef<str> for SmartString<Mode> {
    fn as_ref(&self) -> &str {
        self.deref()
    }
}

impl<Mode: SmartStringMode> AsMut<str> for SmartString<Mode> {
    fn as_mut(&mut self) -> &mut str {
        self.deref_mut()
    }
}

impl<Mode: SmartStringMode> AsRef<[u8]> for SmartString<Mode> {
    fn as_ref(&self) -> &[u8] {
        self.deref().as_bytes()
    }
}

impl<Mode: SmartStringMode> Borrow<str> for SmartString<Mode> {
    fn borrow(&self) -> &str {
        self.deref()
    }
}

impl<Mode: SmartStringMode> BorrowMut<str> for SmartString<Mode> {
    fn borrow_mut(&mut self) -> &mut str {
        self.deref_mut()
    }
}

impl<Mode: SmartStringMode> Index<Range<usize>> for SmartString<Mode> {
    type Output = str;
    fn index(&self, index: Range<usize>) -> &Self::Output {
        &self.deref()[index]
    }
}

impl<Mode: SmartStringMode> Index<RangeTo<usize>> for SmartString<Mode> {
    type Output = str;
    fn index(&self, index: RangeTo<usize>) -> &Self::Output {
        &self.deref()[index]
    }
}

impl<Mode: SmartStringMode> Index<RangeFrom<usize>> for SmartString<Mode> {
    type Output = str;
    fn index(&self, index: RangeFrom<usize>) -> &Self::Output {
        &self.deref()[index]
    }
}

impl<Mode: SmartStringMode> Index<RangeFull> for SmartString<Mode> {
    type Output = str;
    fn index(&self, _index: RangeFull) -> &Self::Output {
        self.deref()
    }
}

impl<Mode: SmartStringMode> Index<RangeInclusive<usize>> for SmartString<Mode> {
    type Output = str;
    fn index(&self, index: RangeInclusive<usize>) -> &Self::Output {
        &self.deref()[index]
    }
}

impl<Mode: SmartStringMode> Index<RangeToInclusive<usize>> for SmartString<Mode> {
    type Output = str;
    fn index(&self, index: RangeToInclusive<usize>) -> &Self::Output {
        &self.deref()[index]
    }
}

impl<Mode: SmartStringMode> IndexMut<Range<usize>> for SmartString<Mode> {
    fn index_mut(&mut self, index: Range<usize>) -> &mut Self::Output {
        &mut self.deref_mut()[index]
    }
}

impl<Mode: SmartStringMode> IndexMut<RangeTo<usize>> for SmartString<Mode> {
    fn index_mut(&mut self, index: RangeTo<usize>) -> &mut Self::Output {
        &mut self.deref_mut()[index]
    }
}

impl<Mode: SmartStringMode> IndexMut<RangeFrom<usize>> for SmartString<Mode> {
    fn index_mut(&mut self, index: RangeFrom<usize>) -> &mut Self::Output {
        &mut self.deref_mut()[index]
    }
}

impl<Mode: SmartStringMode> IndexMut<RangeFull> for SmartString<Mode> {
    fn index_mut(&mut self, _index: RangeFull) -> &mut Self::Output {
        self.deref_mut()
    }
}

impl<Mode: SmartStringMode> IndexMut<RangeInclusive<usize>> for SmartString<Mode> {
    fn index_mut(&mut self, index: RangeInclusive<usize>) -> &mut Self::Output {
        &mut self.deref_mut()[index]
    }
}

impl<Mode: SmartStringMode> IndexMut<RangeToInclusive<usize>> for SmartString<Mode> {
    fn index_mut(&mut self, index: RangeToInclusive<usize>) -> &mut Self::Output {
        &mut self.deref_mut()[index]
    }
}

impl<Mode: SmartStringMode> From<&'_ str> for SmartString<Mode> {
    fn from(string: &'_ str) -> Self {
        if string.len() > MAX_INLINE {
            Self::from_boxed(string.to_string().into())
        } else {
            Self::from_inline(string.into())
        }
    }
}

impl<Mode: SmartStringMode> From<&'_ mut str> for SmartString<Mode> {
    fn from(string: &'_ mut str) -> Self {
        if string.len() > MAX_INLINE {
            Self::from_boxed(string.to_string().into())
        } else {
            Self::from_inline(string.deref().into())
        }
    }
}

impl<Mode: SmartStringMode> From<&'_ String> for SmartString<Mode> {
    fn from(string: &'_ String) -> Self {
        if string.len() > MAX_INLINE {
            Self::from_boxed(string.clone().into())
        } else {
            Self::from_inline(string.deref().into())
        }
    }
}

impl<Mode: SmartStringMode> From<String> for SmartString<Mode> {
    fn from(string: String) -> Self {
        if string.len() > MAX_INLINE {
            Self::from_boxed(string.into())
        } else {
            Self::from_inline(string.deref().into())
        }
    }
}

impl<Mode: SmartStringMode> From<Box<str>> for SmartString<Mode> {
    fn from(string: Box<str>) -> Self {
        if string.len() > MAX_INLINE {
            String::from(string).into()
        } else {
            Self::from(&*string)
        }
    }
}

#[cfg(feature = "std")]
impl<Mode: SmartStringMode> From<Cow<'_, str>> for SmartString<Mode> {
    fn from(string: Cow<'_, str>) -> Self {
        if string.len() > MAX_INLINE {
            String::from(string).into()
        } else {
            Self::from(&*string)
        }
    }
}

impl<'a, Mode: SmartStringMode> Extend<&'a str> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = &'a str>>(&mut self, iter: I) {
        for item in iter {
            self.push_str(item);
        }
    }
}

impl<'a, Mode: SmartStringMode> Extend<&'a char> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = &'a char>>(&mut self, iter: I) {
        for item in iter {
            self.push(*item);
        }
    }
}

impl<Mode: SmartStringMode> Extend<char> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = char>>(&mut self, iter: I) {
        for item in iter {
            self.push(item);
        }
    }
}

impl<Mode: SmartStringMode> Extend<SmartString<Mode>> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = SmartString<Mode>>>(&mut self, iter: I) {
        for item in iter {
            self.push_str(&item);
        }
    }
}

impl<Mode: SmartStringMode> Extend<String> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = String>>(&mut self, iter: I) {
        for item in iter {
            self.push_str(&item);
        }
    }
}

impl<'a, Mode: SmartStringMode + 'a> Extend<&'a SmartString<Mode>> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = &'a SmartString<Mode>>>(&mut self, iter: I) {
        for item in iter {
            self.push_str(item);
        }
    }
}

impl<'a, Mode: SmartStringMode> Extend<&'a String> for SmartString<Mode> {
    fn extend<I: IntoIterator<Item = &'a String>>(&mut self, iter: I) {
        for item in iter {
            self.push_str(item);
        }
    }
}

impl<Mode: SmartStringMode> Add<Self> for SmartString<Mode> {
    type Output = Self;
    fn add(mut self, rhs: Self) -> Self::Output {
        self.push_str(&rhs);
        self
    }
}

impl<Mode: SmartStringMode> Add<&'_ Self> for SmartString<Mode> {
    type Output = Self;
    fn add(mut self, rhs: &'_ Self) -> Self::Output {
        self.push_str(rhs);
        self
    }
}

impl<Mode: SmartStringMode> Add<&'_ str> for SmartString<Mode> {
    type Output = Self;
    fn add(mut self, rhs: &'_ str) -> Self::Output {
        self.push_str(rhs);
        self
    }
}

impl<Mode: SmartStringMode> Add<&'_ String> for SmartString<Mode> {
    type Output = Self;
    fn add(mut self, rhs: &'_ String) -> Self::Output {
        self.push_str(rhs);
        self
    }
}

impl<Mode: SmartStringMode> Add<String> for SmartString<Mode> {
    type Output = Self;
    fn add(mut self, rhs: String) -> Self::Output {
        self.push_str(&rhs);
        self
    }
}

impl<Mode: SmartStringMode> Add<SmartString<Mode>> for String {
    type Output = Self;
    fn add(mut self, rhs: SmartString<Mode>) -> Self::Output {
        self.push_str(&rhs);
        self
    }
}

impl<Mode: SmartStringMode> FromIterator<Self> for SmartString<Mode> {
    fn from_iter<I: IntoIterator<Item = Self>>(iter: I) -> Self {
        let mut out = Self::new();
        out.extend(iter.into_iter());
        out
    }
}

impl<Mode: SmartStringMode> FromIterator<String> for SmartString<Mode> {
    fn from_iter<I: IntoIterator<Item = String>>(iter: I) -> Self {
        let mut out = Self::new();
        out.extend(iter.into_iter());
        out
    }
}

impl<'a, Mode: SmartStringMode + 'a> FromIterator<&'a Self> for SmartString<Mode> {
    fn from_iter<I: IntoIterator<Item = &'a Self>>(iter: I) -> Self {
        let mut out = Self::new();
        out.extend(iter.into_iter());
        out
    }
}

impl<'a, Mode: SmartStringMode> FromIterator<&'a str> for SmartString<Mode> {
    fn from_iter<I: IntoIterator<Item = &'a str>>(iter: I) -> Self {
        let mut out = Self::new();
        out.extend(iter.into_iter());
        out
    }
}

impl<'a, Mode: SmartStringMode> FromIterator<&'a String> for SmartString<Mode> {
    fn from_iter<I: IntoIterator<Item = &'a String>>(iter: I) -> Self {
        let mut out = Self::new();
        out.extend(iter.into_iter());
        out
    }
}

impl<Mode: SmartStringMode> FromIterator<char> for SmartString<Mode> {
    fn from_iter<I: IntoIterator<Item = char>>(iter: I) -> Self {
        let mut out = Self::new();
        for ch in iter {
            out.push(ch);
        }
        out
    }
}

impl<Mode: SmartStringMode> FromStr for SmartString<Mode> {
    type Err = Infallible;
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(Self::from(s))
    }
}

impl<Mode: SmartStringMode> From<SmartString<Mode>> for String {
    /// Unwrap a boxed [`String`][String], or copy an inline string into a new [`String`][String].
    ///
    /// [String]: https://doc.rust-lang.org/std/string/struct.String.html
    fn from(s: SmartString<Mode>) -> Self {
        match s.cast_into() {
            StringCastInto::Boxed(string) => string.into(),
            StringCastInto::Inline(string) => string.to_string(),
        }
    }
}

impl<Mode: SmartStringMode> PartialEq<str> for SmartString<Mode> {
    fn eq(&self, other: &str) -> bool {
        self.as_str() == other
    }
}

impl<Mode: SmartStringMode> PartialEq<&'_ str> for SmartString<Mode> {
    fn eq(&self, other: &&str) -> bool {
        self.as_str() == *other
    }
}

impl<Mode: SmartStringMode> PartialEq<SmartString<Mode>> for &'_ str {
    fn eq(&self, other: &SmartString<Mode>) -> bool {
        other.eq(*self)
    }
}

impl<Mode: SmartStringMode> PartialEq<SmartString<Mode>> for str {
    fn eq(&self, other: &SmartString<Mode>) -> bool {
        other.eq(self)
    }
}

impl<Mode: SmartStringMode> PartialEq<String> for SmartString<Mode> {
    fn eq(&self, other: &String) -> bool {
        self.eq(other.as_str())
    }
}

impl<Mode: SmartStringMode> PartialEq<SmartString<Mode>> for String {
    fn eq(&self, other: &SmartString<Mode>) -> bool {
        other.eq(self.as_str())
    }
}

impl<Mode: SmartStringMode> PartialEq for SmartString<Mode> {
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}

impl<Mode: SmartStringMode> Eq for SmartString<Mode> {}

impl<Mode: SmartStringMode> PartialOrd<str> for SmartString<Mode> {
    fn partial_cmp(&self, other: &str) -> Option<Ordering> {
        self.as_str().partial_cmp(other)
    }
}

impl<Mode: SmartStringMode> PartialOrd for SmartString<Mode> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        self.partial_cmp(other.as_str())
    }
}

impl<Mode: SmartStringMode> Ord for SmartString<Mode> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.as_str().cmp(other.as_str())
    }
}

impl<Mode: SmartStringMode> Hash for SmartString<Mode> {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_str().hash(state)
    }
}

impl<Mode: SmartStringMode> Debug for SmartString<Mode> {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        Debug::fmt(self.as_str(), f)
    }
}

impl<Mode: SmartStringMode> Display for SmartString<Mode> {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), Error> {
        Display::fmt(self.as_str(), f)
    }
}

impl<Mode: SmartStringMode> Write for SmartString<Mode> {
    fn write_str(&mut self, string: &str) -> Result<(), Error> {
        self.push_str(string);
        Ok(())
    }
}

#[cfg(any(test, feature = "test"))]
#[allow(missing_docs)]
pub mod test;
