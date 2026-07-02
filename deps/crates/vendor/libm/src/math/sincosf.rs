/* origin: FreeBSD /usr/src/lib/msun/src/s_sinf.c */
/*
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Optimized by Bruce D. Evans.
 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

use super::{k_cosf, k_sinf, rem_pio2f};

/* Small multiples of pi/2 rounded to double precision. */
const PI_2: f64 = 0.5 * 3.1415926535897931160E+00;
const S1PIO2: f64 = 1.0 * PI_2; /* 0x3FF921FB, 0x54442D18 */
const S2PIO2: f64 = 2.0 * PI_2; /* 0x400921FB, 0x54442D18 */
const S3PIO2: f64 = 3.0 * PI_2; /* 0x4012D97C, 0x7F3321D2 */
const S4PIO2: f64 = 4.0 * PI_2; /* 0x401921FB, 0x54442D18 */

/// Both the sine and cosine of `x` (f32).
///
/// `x` is specified in radians and the return value is (sin(x), cos(x)).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn sincosf(x: f32) -> (f32, f32) {
    let s: f32;
    let c: f32;
    let mut ix: u32;
    let sign: bool;

    ix = x.to_bits();
    sign = (ix >> 31) != 0;
    ix &= 0x7fffffff;

    /* |x| ~<= pi/4 */
    if ix <= 0x3f490fda {
        /* |x| < 2**-12 */
        if ix < 0x39800000 {
            /* raise inexact if x!=0 and underflow if subnormal */

            let x1p120 = f32::from_bits(0x7b800000); // 0x1p120 == 2^120
            if ix < 0x00100000 {
                force_eval!(x / x1p120);
            } else {
                force_eval!(x + x1p120);
            }
            return (x, 1.0);
        }
        return (k_sinf(x as f64), k_cosf(x as f64));
    }

    /* |x| ~<= 5*pi/4 */
    if ix <= 0x407b53d1 {
        if ix <= 0x4016cbe3 {
            /* |x| ~<= 3pi/4 */
            if sign {
                s = -k_cosf(x as f64 + S1PIO2);
                c = k_sinf(x as f64 + S1PIO2);
            } else {
                s = k_cosf(S1PIO2 - x as f64);
                c = k_sinf(S1PIO2 - x as f64);
            }
        }
        /* -sin(x+c) is not correct if x+c could be 0: -0 vs +0 */
        else if sign {
            s = -k_sinf(x as f64 + S2PIO2);
            c = -k_cosf(x as f64 + S2PIO2);
        } else {
            s = -k_sinf(x as f64 - S2PIO2);
            c = -k_cosf(x as f64 - S2PIO2);
        }

        return (s, c);
    }

    /* |x| ~<= 9*pi/4 */
    if ix <= 0x40e231d5 {
        if ix <= 0x40afeddf {
            /* |x| ~<= 7*pi/4 */
            if sign {
                s = k_cosf(x as f64 + S3PIO2);
                c = -k_sinf(x as f64 + S3PIO2);
            } else {
                s = -k_cosf(x as f64 - S3PIO2);
                c = k_sinf(x as f64 - S3PIO2);
            }
        } else if sign {
            s = k_sinf(x as f64 + S4PIO2);
            c = k_cosf(x as f64 + S4PIO2);
        } else {
            s = k_sinf(x as f64 - S4PIO2);
            c = k_cosf(x as f64 - S4PIO2);
        }

        return (s, c);
    }

    /* sin(Inf or NaN) is NaN */
    if ix >= 0x7f800000 {
        let rv = x - x;
        return (rv, rv);
    }

    /* general argument reduction needed */
    let (n, y) = rem_pio2f(x);
    s = k_sinf(y);
    c = k_cosf(y);
    match n & 3 {
        0 => (s, c),
        1 => (c, -s),
        2 => (-s, -c),
        3 => (-c, s),
        #[cfg(debug_assertions)]
        _ => unreachable!(),
        #[cfg(not(debug_assertions))]
        _ => (0.0, 1.0),
    }
}

// PowerPC tests are failing on LLVM 13: https://github.com/rust-lang/rust/issues/88520
#[cfg(not(target_arch = "powerpc64"))]
#[cfg(test)]
mod tests {
    use super::sincosf;

    #[test]
    fn rotational_symmetry() {
        use core::f32::consts::PI;
        const N: usize = 24;
        for n in 0..N {
            let theta = 2. * PI * (n as f32) / (N as f32);
            let (s, c) = sincosf(theta);
            let (s_plus, c_plus) = sincosf(theta + 2. * PI);
            let (s_minus, c_minus) = sincosf(theta - 2. * PI);

            const TOLERANCE: f32 = 1e-6;
            assert!(
                (s - s_plus).abs() < TOLERANCE,
                "|{} - {}| = {} >= {}",
                s,
                s_plus,
                (s - s_plus).abs(),
                TOLERANCE
            );
            assert!(
                (s - s_minus).abs() < TOLERANCE,
                "|{} - {}| = {} >= {}",
                s,
                s_minus,
                (s - s_minus).abs(),
                TOLERANCE
            );
            assert!(
                (c - c_plus).abs() < TOLERANCE,
                "|{} - {}| = {} >= {}",
                c,
                c_plus,
                (c - c_plus).abs(),
                TOLERANCE
            );
            assert!(
                (c - c_minus).abs() < TOLERANCE,
                "|{} - {}| = {} >= {}",
                c,
                c_minus,
                (c - c_minus).abs(),
                TOLERANCE
            );
        }
    }
}
