/* SPDX-License-Identifier: MIT OR Apache-2.0 */
//! IEEE 754-2019 `minimum`.
//!
//! Per the spec, returns:
//! - `x` if `x < y`
//! - `y` if `y < x`
//! - Non-NaN if one operand is NaN
//! - Logic following +0.0 > -0.0
//! - Either `x` or `y` if `x == y` and the signs are the same
//! - qNaN if either operand is a NaN
//!
//! Excluded from our implementation is sNaN handling.

use crate::support::Float;

#[inline]
pub fn fminimum_num<F: Float>(x: F, y: F) -> F {
    let res =
        if y.is_nan() || x < y || (x.to_bits() == F::NEG_ZERO.to_bits() && y.is_sign_positive()) {
            x
        } else {
            y
        };

    // Canonicalize
    res * F::ONE
}
