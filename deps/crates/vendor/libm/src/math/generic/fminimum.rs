/* SPDX-License-Identifier: MIT OR Apache-2.0 */
//! IEEE 754-2019 `minimum`.
//!
//! Per the spec, returns the canonicalized result of:
//! - `x` if `x < y`
//! - `y` if `y < x`
//! - qNaN if either operation is NaN
//! - Logic following +0.0 > -0.0
//!
//! Excluded from our implementation is sNaN handling.

use crate::support::Float;

#[inline]
pub fn fminimum<F: Float>(x: F, y: F) -> F {
    let res = if x.is_nan() {
        x
    } else if y.is_nan() {
        y
    } else if x < y || (x.to_bits() == F::NEG_ZERO.to_bits() && y.is_sign_positive()) {
        x
    } else {
        y
    };

    // Canonicalize
    res * F::ONE
}
