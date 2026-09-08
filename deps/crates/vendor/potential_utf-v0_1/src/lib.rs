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

//! A crate providing unvalidated string and character types.

#[cfg(feature = "alloc")]
extern crate alloc;

mod uchar;
mod ustr;

pub use uchar::PotentialCodePoint;
pub use ustr::PotentialUtf16;
pub use ustr::PotentialUtf8;

#[cfg(feature = "writeable")]
mod writeable;
