use super::{exp2, exp2f, modff};

const LN10_F32: f32 = 3.32192809488736234787031942948939;
const LN10_F64: f64 = 3.32192809488736234787031942948939;
const P10: &[f32] = &[
    1e-7, 1e-6, 1e-5, 1e-4, 1e-3, 1e-2, 1e-1, 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7,
];

/// Calculates 10 raised to the power of `x` (f32).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn exp10f(x: f32) -> f32 {
    let (mut y, n) = modff(x);
    let u = n.to_bits();
    /* fabsf(n) < 8 without raising invalid on nan */
    if ((u >> 23) & 0xff) < 0x7f + 3 {
        if y == 0.0 {
            return i!(P10, ((n as isize) + 7) as usize);
        }
        y = exp2f(LN10_F32 * y);
        return y * i!(P10, ((n as isize) + 7) as usize);
    }
    return exp2(LN10_F64 * (x as f64)) as f32;
}
