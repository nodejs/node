// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! 🚧 \[Unstable\] Data Structures and conversions for directly constructing the plural rule
//! operands of a number
//!
//! <div class="stab unstable">
//! 🚧 This code is unstable; it may change at any time, in breaking or non-breaking ways,
//! including in SemVer minor releases. Use with caution.
//! <a href="https://github.com/unicode-org/icu4x/issues/1091">#1091</a>
//! </div>

use crate::PluralOperands;

/// 🚧 \[Unstable\] A struct for low-level users who want to construct a [`PluralOperands`]
/// directly based on the LDML Plural Operand definitions.
///
/// This may be useful
/// for people with unstable rules parsing.
///
/// This struct is not intended for supported API use, and it is subject to breaking
/// changes (ex: a new Plural Operand needs to be supported).
///
/// Most users with numerical data inputs for places where [`PluralOperands`] is
/// accepted, like [`PluralRules::category_for`](super::PluralRules::category_for), should convert to [`PluralOperands`].
/// See [`PluralOperands`] for details.
///
/// <div class="stab unstable">
/// 🚧 This code is unstable; it may change at any time, in breaking or non-breaking ways,
/// including in SemVer minor releases. Use with caution.
/// <a href="https://github.com/unicode-org/icu4x/issues/1091">#1091</a>
/// </div>
#[cfg(feature = "unstable")]
#[allow(clippy::exhaustive_structs)] // unstable
#[derive(Debug)]
pub struct RawPluralOperands {
    /// Integer value of input
    pub i: u64,
    /// Number of visible fraction digits with trailing zeros
    pub v: usize,
    /// Number of visible fraction digits without trailing zeros
    pub w: usize,
    /// Visible fraction digits with trailing zeros
    pub f: u64,
    /// Visible fraction digits without trailing zeros
    pub t: u64,
    /// Exponent of the power of 10 used in compact decimal formatting
    pub c: usize,
}

impl From<RawPluralOperands> for PluralOperands {
    fn from(rpo: RawPluralOperands) -> PluralOperands {
        Self {
            i: rpo.i,
            v: rpo.v,
            w: rpo.w,
            f: rpo.f,
            t: rpo.t,
            c: rpo.c,
        }
    }
}

impl From<PluralOperands> for RawPluralOperands {
    fn from(po: PluralOperands) -> RawPluralOperands {
        Self {
            i: po.i,
            v: po.v,
            w: po.w,
            f: po.f,
            t: po.t,
            c: po.c,
        }
    }
}
