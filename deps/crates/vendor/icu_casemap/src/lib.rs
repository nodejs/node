// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Case mapping for Unicode characters and strings.
//!
//! This module is published as its own crate ([`icu_casemap`](https://docs.rs/icu_casemap/latest/icu_casemap/))
//! and as part of the [`icu`](https://docs.rs/icu/latest/icu/) crate. See the latter for more details on the ICU4X project.
//!
//! # Examples
//!
//! ```rust
//! use icu::casemap::CaseMapper;
//! use icu::locale::langid;
//!
//! let cm = CaseMapper::new();
//!
//! assert_eq!(
//!     cm.uppercase_to_string("hello world", &langid!("und")),
//!     "HELLO WORLD"
//! );
//! assert_eq!(
//!     cm.lowercase_to_string("Γειά σου Κόσμε", &langid!("und")),
//!     "γειά σου κόσμε"
//! );
//! ```
//!
//! [`ICU4X`]: ../icu/index.html

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
// We're using Greek identifiers here on purpose. These lints can only be disabled at the crate level
#![allow(confusable_idents, uncommon_codepoints)]

extern crate alloc;

mod casemapper;
mod closer;
pub mod provider;
mod set;
pub(crate) mod titlecase;

#[doc(hidden)] // testing
#[expect(clippy::exhaustive_structs, clippy::exhaustive_enums)]
pub mod greek_to_me;
mod internals;

pub use casemapper::{CaseMapper, CaseMapperBorrowed};
pub use closer::{CaseMapCloser, CaseMapCloserBorrowed};
pub use set::ClosureSink;
pub use titlecase::{TitlecaseMapper, TitlecaseMapperBorrowed};

/// Options used by types in this crate
pub mod options {
    pub use crate::titlecase::{LeadingAdjustment, TitlecaseOptions, TrailingCase};
}
