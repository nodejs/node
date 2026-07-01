// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Formatting basic decimal numbers.
//!
//! This module is published as its own crate ([`icu_decimal`](https://docs.rs/icu_decimal/latest/icu_decimal/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! Support for currencies, measurement units, and compact notation is planned. To track progress,
//! follow [icu4x#275](https://github.com/unicode-org/icu4x/issues/275).
//!
//! # Examples
//!
//! ## Format a number with Bangla digits
//!
//! ```
//! use icu::decimal::input::Decimal;
//! use icu::decimal::DecimalFormatter;
//! use icu::locale::locale;
//! use writeable::assert_writeable_eq;
//!
//! let formatter =
//!     DecimalFormatter::try_new(locale!("bn").into(), Default::default())
//!         .expect("locale should be present");
//!
//! let decimal = Decimal::from(1000007);
//!
//! assert_writeable_eq!(formatter.format(&decimal), "১০,০০,০০৭");
//! ```
//!
//! ## Format a number with digits after the decimal separator
//!
//! ```
//! use icu::decimal::input::Decimal;
//! use icu::decimal::DecimalFormatter;
//! use icu::locale::Locale;
//! use writeable::assert_writeable_eq;
//!
//! let formatter =
//!     DecimalFormatter::try_new(Default::default(), Default::default())
//!         .expect("locale should be present");
//!
//! let decimal = {
//!     let mut decimal = Decimal::from(200050);
//!     decimal.multiply_pow10(-2);
//!     decimal
//! };
//!
//! assert_writeable_eq!(formatter.format(&decimal), "2,000.50");
//! ```
//!
//! ## Format a number using an alternative numbering system
//!
//! Numbering systems specified in the `-u-nu` subtag will be followed.
//!
//! ```
//! use icu::decimal::input::Decimal;
//! use icu::decimal::DecimalFormatter;
//! use icu::locale::locale;
//! use writeable::assert_writeable_eq;
//!
//! let formatter = DecimalFormatter::try_new(
//!     locale!("th-u-nu-thai").into(),
//!     Default::default(),
//! )
//! .expect("locale should be present");
//!
//! let decimal = Decimal::from(1000007);
//!
//! assert_writeable_eq!(formatter.format(&decimal), "๑,๐๐๐,๐๐๗");
//! ```
//!
//! [`DecimalFormatter`]: DecimalFormatter

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

#[cfg(feature = "alloc")]
extern crate alloc;

#[cfg(feature = "alloc")]
#[doc(hidden)] // TODO(#3647): should be private
pub use alloc::borrow::Cow;

#[cfg(not(feature = "alloc"))]
#[derive(Debug, PartialEq, Eq, Clone)]
#[doc(hidden)] // TODO(#3647): should be private
pub enum Cow<'a, T> {
    Borrowed(&'a T),
    Owned(T),
}

#[cfg(not(feature = "alloc"))]
impl<'a, T> core::ops::Deref for Cow<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        match self {
            Self::Borrowed(r) => r,
            Self::Owned(ref r) => r,
        }
    }
}

#[cfg(feature = "unstable")]
mod compact_formatter;
mod decimal_formatter;
#[cfg(feature = "unstable")]
pub mod error;
mod grouper;
pub mod options;
pub mod parts;
pub mod preferences;
pub mod provider;
mod size_test_macro;

pub use decimal_formatter::{
    DecimalFormatter, FormattedDecimal, FormattedSign, FormattedUnsignedDecimal,
};

#[cfg(feature = "unstable")]
pub use compact_formatter::CompactDecimalFormatter;

pub use preferences::DecimalFormatterPreferences;

/// Types that can be fed to [`DecimalFormatter`] and their utilities
///
/// This module contains re-exports from the [`fixed_decimal`] crate.
pub mod input {
    pub use fixed_decimal::Decimal;
    #[cfg(feature = "ryu")]
    pub use fixed_decimal::FloatPrecision;
    pub use fixed_decimal::SignDisplay;
}
