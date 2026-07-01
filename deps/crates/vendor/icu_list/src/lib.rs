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

//! Formatting lists in a locale-sensitive way.
//!
//! This module is published as its own crate ([`icu_list`](https://docs.rs/icu_list/latest/icu_list/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! # Examples
//!
//! ## Formatting *and* lists in Spanish
//!
//! ```
//! # use icu::list::ListFormatter;
//! # use icu::list::options::{ListFormatterOptions, ListLength};
//! # use icu::locale::locale;
//! # use writeable::*;
//! #
//! let list_formatter = ListFormatter::try_new_and(
//!     locale!("es").into(),
//!     ListFormatterOptions::default().with_length(ListLength::Wide),
//! )
//! .expect("locale should be present");
//!
//! assert_writeable_eq!(
//!     list_formatter.format(["España", "Suiza"].iter()),
//!     "España y Suiza",
//! );
//!
//! // The Spanish 'y' sometimes becomes an 'e':
//! assert_writeable_eq!(
//!     list_formatter.format(["España", "Suiza", "Italia"].iter()),
//!     "España, Suiza e Italia",
//! );
//! ```
//!
//! ## Formatting *or* lists in Thai
//!
//! ```
//! # use icu::list::ListFormatter;
//! # use icu::list::options::{ListFormatterOptions, ListLength};
//! # use icu::locale::locale;
//! # use writeable::*;
//! #
//! let list_formatter = ListFormatter::try_new_or(
//!     locale!("th").into(),
//!     ListFormatterOptions::default().with_length(ListLength::Short),
//! )
//! .expect("locale should be present");
//!
//! // We can use any Writeables as inputs
//! assert_writeable_eq!(list_formatter.format(1..=3), "1, 2 หรือ 3",);
//! ```
//!
//! ## Formatting unit lists in English
//!
//! ```
//! # use icu::list::ListFormatter;
//! # use icu::list::options::{ListFormatterOptions, ListLength};
//! # use icu::locale::locale;
//! # use writeable::*;
//! #
//! let list_formatter = ListFormatter::try_new_unit(
//!     locale!("en").into(),
//!     ListFormatterOptions::default().with_length(ListLength::Wide),
//! )
//! .expect("locale should be present");
//!
//! assert_writeable_eq!(
//!     list_formatter.format(["1ft", "2in"].iter()),
//!     "1ft, 2in",
//! );
//! ```
//! Note: this last example is not fully internationalized. See [icu4x/2192](https://github.com/unicode-org/icu4x/issues/2192)
//! for full unit handling.

#[cfg(feature = "alloc")]
extern crate alloc;

mod lazy_automaton;
mod list_formatter;
pub mod options;
mod patterns;

pub mod provider;

pub use list_formatter::*;
