/* SPDX-License-Identifier: MIT OR Apache-2.0 */
//! IEEE 754-2019 `maximum`.
//!
//! Per the spec, returns the canonicalized result of:
//! - `x` if `x > y`
//! - `y` if `y > x`
//! - +0.0 if x and y are zero with opposite signs
//! - qNaN if either operation is NaN
//!
//! Excluded from our implementation is sNaN handling.

use crate::support::Float;

#[inline]
pub fn fmaximum<F: Float>(x: F, y: F) -> F {
    let res = if x.is_nan() {
        x
    } else if y.is_nan() {
        y
    } else if x > y || (y.biteq(F::NEG_ZERO) && x.is_sign_positive()) {
        x
    } else {
        y
    };

    res.canonicalize()
}
