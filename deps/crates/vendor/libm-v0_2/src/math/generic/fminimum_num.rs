/* SPDX-License-Identifier: MIT OR Apache-2.0 */
//! IEEE 754-2019 `minimum`.
//!
//! Per the spec, returns:
//! - `x` if `x < y`
//! - `y` if `y < x`
//! - -0.0 if x and y are zero with opposite signs
//! - Either `x` or `y` if `x == y` and the signs are the same
//! - Non-NaN if one operand is NaN
//! - qNaN if both operands are NaNx
//!
//! Excluded from our implementation is sNaN handling.

use crate::support::Float;

#[inline]
pub fn fminimum_num<F: Float>(x: F, y: F) -> F {
    let res = if x > y || x.is_nan() {
        y
    } else if y > x || y.is_nan() {
        x
    } else if x.is_sign_positive() {
        y
    } else {
        x
    };

    res.canonicalize()
}
