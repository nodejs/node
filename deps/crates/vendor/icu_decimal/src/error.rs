// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Error types for functions in the decimal crate

use displaydoc::Display;

/// An error due to incorrect arguments to
/// [`CompactDecimalFormatter::format_with_exponent`](crate::CompactDecimalFormatter::format_with_exponent).
///
/// <div class="stab unstable">
/// 🚧 This code is considered unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Do not use this type unless you are prepared for things to occasionally break.
///
/// Graduation tracking issue: [issue #7161](https://github.com/unicode-org/icu4x/issues/7161).
/// </div>
///
/// ✨ *Enabled with the `unstable` Cargo feature.*
#[derive(Display, Copy, Clone, Debug)]
#[displaydoc("Expected compact exponent {expected} for 10^{magnitude}, got {actual}")]
pub struct CompactExponentError {
    /// The compact decimal exponent passed to the formatter.
    pub(crate) actual: u8,
    /// The appropriate exponent for a number of the given magnitude.
    pub(crate) expected: u8,
    /// The magnitude of the number being formatted.
    pub(crate) magnitude: i16,
}

impl core::error::Error for CompactExponentError {}
