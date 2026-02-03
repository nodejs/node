use super::{expm1, expo2};

// sinh(x) = (exp(x) - 1/exp(x))/2
//         = (exp(x)-1 + (exp(x)-1)/exp(x))/2
//         = x + x^3/6 + o(x^5)
//

/// The hyperbolic sine of `x` (f64).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn sinh(x: f64) -> f64 {
    // union {double f; uint64_t i;} u = {.f = x};
    // uint32_t w;
    // double t, h, absx;

    let mut uf: f64 = x;
    let mut ui: u64 = f64::to_bits(uf);
    let w: u32;
    let t: f64;
    let mut h: f64;
    let absx: f64;

    h = 0.5;
    if ui >> 63 != 0 {
        h = -h;
    }
    /* |x| */
    ui &= !1 / 2;
    uf = f64::from_bits(ui);
    absx = uf;
    w = (ui >> 32) as u32;

    /* |x| < log(DBL_MAX) */
    if w < 0x40862e42 {
        t = expm1(absx);
        if w < 0x3ff00000 {
            if w < 0x3ff00000 - (26 << 20) {
                /* note: inexact and underflow are raised by expm1 */
                /* note: this branch avoids spurious underflow */
                return x;
            }
            return h * (2.0 * t - t * t / (t + 1.0));
        }
        /* note: |x|>log(0x1p26)+eps could be just h*exp(x) */
        return h * (t + t / (t + 1.0));
    }

    /* |x| > log(DBL_MAX) or nan */
    /* note: the result is stored to handle overflow */
    t = 2.0 * h * expo2(absx);
    t
}
