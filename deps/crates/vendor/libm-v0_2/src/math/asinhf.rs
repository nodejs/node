use super::{log1pf, logf, sqrtf};

const LN2: f32 = 0.693147180559945309417232121458176568;

/* asinh(x) = sign(x)*log(|x|+sqrt(x*x+1)) ~= x - x^3/6 + o(x^5) */
/// Inverse hyperbolic sine (f32)
///
/// Calculates the inverse hyperbolic sine of `x`.
/// Is defined as `sgn(x)*log(|x|+sqrt(x*x+1))`.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn asinhf(mut x: f32) -> f32 {
    let u = x.to_bits();
    let i = u & 0x7fffffff;
    let sign = (u >> 31) != 0;

    /* |x| */
    x = f32::from_bits(i);

    if i >= 0x3f800000 + (12 << 23) {
        /* |x| >= 0x1p12 or inf or nan */
        x = logf(x) + LN2;
    } else if i >= 0x3f800000 + (1 << 23) {
        /* |x| >= 2 */
        x = logf(2.0 * x + 1.0 / (sqrtf(x * x + 1.0) + x));
    } else if i >= 0x3f800000 - (12 << 23) {
        /* |x| >= 0x1p-12, up to 1.6ulp error in [0.125,0.5] */
        x = log1pf(x + x * x / (sqrtf(x * x + 1.0) + 1.0));
    } else {
        /* |x| < 0x1p-12, raise inexact if x!=0 */
        let x1p120 = f32::from_bits(0x7b800000);
        force_eval!(x + x1p120);
    }

    if sign { -x } else { x }
}
