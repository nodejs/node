/* SPDX-License-Identifier: MIT OR Apache-2.0 */
//! IEEE 754-2011 `maxNum`. This has been superseded by IEEE 754-2019 `maximumNumber`.
//!
//! Per the spec, returns the canonicalized result of:
//! - `x` if `x > y`
//! - `y` if `y > x`
//! - The other number if one is NaN
//! - Otherwise, either `x` or `y`, canonicalized
//! - -0.0 and +0.0 may be disregarded (unlike newer operations)
//!
//! Excluded from our implementation is sNaN handling.
//!
//! More on the differences: [link].
//!
//! [link]: https://grouper.ieee.org/groups/msc/ANSI_IEEE-Std-754-2019/background/minNum_maxNum_Removal_Demotion_v3.pdf

use crate::support::Float;

#[inline]
pub fn fmax<F: Float>(x: F, y: F) -> F {
    let res = if x.is_nan() || x < y { y } else { x };
    // Canonicalize
    res * F::ONE
}
