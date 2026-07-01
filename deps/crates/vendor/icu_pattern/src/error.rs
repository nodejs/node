// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use displaydoc::Display;

#[derive(Debug, Display, Copy, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum PatternError {
    #[displaydoc("Syntax error in pattern string or invalid serialized pattern")]
    InvalidPattern,
    #[displaydoc("A placeholder is invalid or out of range for the pattern type")]
    InvalidPlaceholder,
}

impl core::error::Error for PatternError {}
