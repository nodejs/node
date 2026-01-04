use super::{exp, expm1, k_expo2};

/// Hyperbolic cosine (f64)
///
/// Computes the hyperbolic cosine of the argument x.
/// Is defined as `(exp(x) + exp(-x))/2`
/// Angles are specified in radians.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn cosh(mut x: f64) -> f64 {
    /* |x| */
    let mut ix = x.to_bits();
    ix &= 0x7fffffffffffffff;
    x = f64::from_bits(ix);
    let w = ix >> 32;

    /* |x| < log(2) */
    if w < 0x3fe62e42 {
        if w < 0x3ff00000 - (26 << 20) {
            let x1p120 = f64::from_bits(0x4770000000000000);
            force_eval!(x + x1p120);
            return 1.;
        }
        let t = expm1(x); // exponential minus 1
        return 1. + t * t / (2. * (1. + t));
    }

    /* |x| < log(DBL_MAX) */
    if w < 0x40862e42 {
        let t = exp(x);
        /* note: if x>log(0x1p26) then the 1/t is not needed */
        return 0.5 * (t + 1. / t);
    }

    /* |x| > log(DBL_MAX) or nan */
    k_expo2(x)
}
