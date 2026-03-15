// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! The `text` module provides a reader for the text-based resource bundle
//! format.
//!
//! WARNING: This reader is intended for use in development-time tools and is
//! not written with runtime efficiency in mind.

mod reader;

pub use reader::Reader;
