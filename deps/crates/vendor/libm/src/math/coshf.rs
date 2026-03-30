use super::{expf, expm1f, k_expo2f};

/// Hyperbolic cosine (f64)
///
/// Computes the hyperbolic cosine of the argument x.
/// Is defined as `(exp(x) + exp(-x))/2`
/// Angles are specified in radians.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn coshf(mut x: f32) -> f32 {
    let x1p120 = f32::from_bits(0x7b800000); // 0x1p120f === 2 ^ 120

    /* |x| */
    let mut ix = x.to_bits();
    ix &= 0x7fffffff;
    x = f32::from_bits(ix);
    let w = ix;

    /* |x| < log(2) */
    if w < 0x3f317217 {
        if w < (0x3f800000 - (12 << 23)) {
            force_eval!(x + x1p120);
            return 1.;
        }
        let t = expm1f(x);
        return 1. + t * t / (2. * (1. + t));
    }

    /* |x| < log(FLT_MAX) */
    if w < 0x42b17217 {
        let t = expf(x);
        return 0.5 * (t + 1. / t);
    }

    /* |x| > log(FLT_MAX) or nan */
    k_expo2f(x)
}
