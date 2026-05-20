/* SPDX-License-Identifier: MIT OR Apache-2.0 */
//! IEEE 754-2008 `minNum`. This has been superseded by IEEE 754-2019 `minimumNumber`.
//!
//! Per the spec, returns the canonicalized result of:
//! - `x` if `x < y`
//! - `y` if `y < x`
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
pub fn fmin<F: Float>(x: F, y: F) -> F {
    let res = if y.is_nan() || x < y { x } else { y };
    // Canonicalize
    res * F::ONE
}
