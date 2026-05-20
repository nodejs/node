use super::{exp2, modf, pow};

const LN10: f64 = 3.32192809488736234787031942948939;
const P10: &[f64] = &[
    1e-15, 1e-14, 1e-13, 1e-12, 1e-11, 1e-10, 1e-9, 1e-8, 1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1,
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
];

/// Calculates 10 raised to the power of `x` (f64).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn exp10(x: f64) -> f64 {
    let (mut y, n) = modf(x);
    let u: u64 = n.to_bits();
    /* fabs(n) < 16 without raising invalid on nan */
    if ((u >> 52) & 0x7ff) < 0x3ff + 4 {
        if y == 0.0 {
            return i!(P10, ((n as isize) + 15) as usize);
        }
        y = exp2(LN10 * y);
        return y * i!(P10, ((n as isize) + 15) as usize);
    }
    return pow(10.0, x);
}
