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
// #![warn(missing_docs)]

//! `icu_pattern` is a utility crate of the [`ICU4X`] project.
//!
//! It includes a [`Pattern`] type which supports patterns with various storage backends.
//!
//! The types are tightly coupled with the [`writeable`] crate.
//!
//! # Examples
//!
//! Parsing and interpolating with a single-placeholder pattern:
//!
//! ```
//! use icu_pattern::SinglePlaceholderPattern;
//! use writeable::assert_writeable_eq;
//!
//! // Parse a pattern string:
//! let pattern = SinglePlaceholderPattern::try_from_str(
//!     "Hello, {0}!",
//!     Default::default(),
//! )
//! .unwrap();
//!
//! // Interpolate into the pattern string:
//! assert_writeable_eq!(pattern.interpolate(["World"]), "Hello, World!");
//! ```
//!
//! [`ICU4X`]: ../icu/index.html
//! [`FromStr`]: core::str::FromStr

#[cfg(feature = "alloc")]
extern crate alloc;

#[cfg(feature = "alloc")]
mod builder;
mod common;
mod double;
mod error;
mod frontend;
mod multi_named;
#[cfg(feature = "alloc")]
mod parser;
mod single;

pub use common::PatternBackend;
pub use common::PatternItem;
#[cfg(feature = "alloc")]
pub use common::PatternItemCow;
pub use common::PlaceholderValueProvider;
pub use double::DoublePlaceholder;
pub use double::DoublePlaceholderKey;
pub use error::PatternError;
#[cfg(feature = "serde")]
pub use frontend::serde::*;
pub use frontend::Pattern;
pub use multi_named::MissingNamedPlaceholderError;
pub use multi_named::MultiNamedPlaceholder;
pub use multi_named::MultiNamedPlaceholderKey;
#[cfg(feature = "alloc")]
pub use parser::ParsedPatternItem;
#[cfg(feature = "alloc")]
pub use parser::Parser;
#[cfg(feature = "alloc")]
pub use parser::ParserError;
#[cfg(feature = "alloc")]
pub use parser::ParserOptions;
#[cfg(feature = "alloc")]
pub use parser::QuoteMode;
pub use single::SinglePlaceholder;
pub use single::SinglePlaceholderKey;
#[doc(no_inline)]
pub use PatternError as Error;

mod private {
    pub trait Sealed {}
}

/// # Examples
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::SinglePlaceholderPattern;
/// use writeable::assert_writeable_eq;
///
/// // Create a pattern from the string syntax:
/// let pattern = SinglePlaceholderPattern::try_from_str(
///     "Hello, {0}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// // Interpolate some values into the pattern:
/// assert_writeable_eq!(pattern.interpolate(["Alice"]), "Hello, Alice!");
/// ```
pub type SinglePlaceholderPattern = Pattern<SinglePlaceholder>;

impl SinglePlaceholderPattern {
    /// An instance of [`SinglePlaceholderPattern`] that has a single placeholder and adds no
    /// prefix or suffix.
    ///
    /// # Examples
    ///
    /// ```
    /// use icu_pattern::SinglePlaceholderPattern;
    /// use writeable::assert_writeable_eq;
    ///
    /// assert_eq!(
    ///     SinglePlaceholderPattern::PASS_THROUGH,
    ///     &*SinglePlaceholderPattern::try_from_str("{0}", Default::default())
    ///         .unwrap()
    /// );
    ///
    /// assert_writeable_eq!(
    ///     SinglePlaceholderPattern::PASS_THROUGH.interpolate(["hello, world!"]),
    ///     "hello, world!"
    /// );
    /// ```
    pub const PASS_THROUGH: &'static SinglePlaceholderPattern =
        SinglePlaceholderPattern::from_ref_store_unchecked("\x01");
}

/// # Examples
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::DoublePlaceholderPattern;
/// use writeable::assert_writeable_eq;
///
/// // Create a pattern from the string syntax:
/// let pattern = DoublePlaceholderPattern::try_from_str(
///     "Hello, {0} and {1}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// // Interpolate some values into the pattern:
/// assert_writeable_eq!(
///     pattern.interpolate(["Alice", "Bob"]),
///     "Hello, Alice and Bob!"
/// );
/// ```
pub type DoublePlaceholderPattern = Pattern<DoublePlaceholder>;

/// # Examples
///
/// ```
/// use core::str::FromStr;
/// use icu_pattern::MultiNamedPlaceholderPattern;
/// use std::collections::BTreeMap;
/// use writeable::assert_try_writeable_eq;
///
/// // Create a pattern from the string syntax:
/// let pattern = MultiNamedPlaceholderPattern::try_from_str(
///     "Hello, {person0} and {person1}!",
///     Default::default(),
/// )
/// .unwrap();
///
/// // Interpolate some values into the pattern:
/// assert_try_writeable_eq!(
///     pattern.try_interpolate(
///         [("person0", "Alice"), ("person1", "Bob")]
///             .into_iter()
///             .collect::<BTreeMap<&str, &str>>()
///     ),
///     "Hello, Alice and Bob!"
/// );
/// ```
pub type MultiNamedPlaceholderPattern = Pattern<MultiNamedPlaceholder>;

#[test]
#[cfg(feature = "alloc")]
fn test_single_placeholder_pattern_impls() {
    let a = SinglePlaceholderPattern::try_from_str("{0}", Default::default()).unwrap();
    let b = SinglePlaceholderPattern::try_from_str("{0}", Default::default()).unwrap();
    assert_eq!(a, b);
    let c = b.clone();
    assert_eq!(a, c);
}
