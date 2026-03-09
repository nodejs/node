// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! `resb` is a utility crate of the [`ICU4X`] project for working with ICU
//! resource bundle files.
//!
//! It comprises modules for reading and optionally writing [`binary`] `.res`
//! files as well as optionally for reading [`text`] bundles.
//!
//! [`ICU4X`]: ../icu/index.html

// https://github.com/unicode-org/icu4x/blob/main/documents/process/boilerplate.md#library-annotations
#![cfg_attr(not(any(test, feature = "std")), no_std)]
#![cfg_attr(
    not(test),
    deny(
        clippy::indexing_slicing,
        clippy::unwrap_used,
        clippy::expect_used,
        clippy::panic,
        clippy::exhaustive_structs,
        clippy::exhaustive_enums,
        clippy::trivially_copy_pass_by_ref,
        missing_debug_implementations,
    )
)]
#![warn(missing_docs)]

extern crate alloc;

pub mod binary;

#[cfg(feature = "text")]
pub mod text;

#[cfg(any(feature = "serialize", feature = "text"))]
pub mod bundle;

/// A mask for extracting the least significant 28 bits of a 32-bit integer.
const MASK_28_BIT: u32 = 0x0fff_ffff;
