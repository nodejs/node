/* origin: FreeBSD /usr/src/lib/msun/src/s_cosf.c */
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

use core::f64::consts::FRAC_PI_2;

use super::{k_cosf, k_sinf, rem_pio2f};

/* Small multiples of pi/2 rounded to double precision. */
const C1_PIO2: f64 = 1. * FRAC_PI_2; /* 0x3FF921FB, 0x54442D18 */
const C2_PIO2: f64 = 2. * FRAC_PI_2; /* 0x400921FB, 0x54442D18 */
const C3_PIO2: f64 = 3. * FRAC_PI_2; /* 0x4012D97C, 0x7F3321D2 */
const C4_PIO2: f64 = 4. * FRAC_PI_2; /* 0x401921FB, 0x54442D18 */

/// The cosine of `x` (f32).
///
/// `x` is specified in radians.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn cosf(x: f32) -> f32 {
    let x64 = x as f64;

    let x1p120 = f32::from_bits(0x7b800000); // 0x1p120f === 2 ^ 120

    let mut ix = x.to_bits();
    let sign = (ix >> 31) != 0;
    ix &= 0x7fffffff;

    if ix <= 0x3f490fda {
        /* |x| ~<= pi/4 */
        if ix < 0x39800000 {
            /* |x| < 2**-12 */
            /* raise inexact if x != 0 */
            force_eval!(x + x1p120);
            return 1.;
        }
        return k_cosf(x64);
    }
    if ix <= 0x407b53d1 {
        /* |x| ~<= 5*pi/4 */
        if ix > 0x4016cbe3 {
            /* |x|  ~> 3*pi/4 */
            return -k_cosf(if sign { x64 + C2_PIO2 } else { x64 - C2_PIO2 });
        } else if sign {
            return k_sinf(x64 + C1_PIO2);
        } else {
            return k_sinf(C1_PIO2 - x64);
        }
    }
    if ix <= 0x40e231d5 {
        /* |x| ~<= 9*pi/4 */
        if ix > 0x40afeddf {
            /* |x| ~> 7*pi/4 */
            return k_cosf(if sign { x64 + C4_PIO2 } else { x64 - C4_PIO2 });
        } else if sign {
            return k_sinf(-x64 - C3_PIO2);
        } else {
            return k_sinf(x64 - C3_PIO2);
        }
    }

    /* cos(Inf or NaN) is NaN */
    if ix >= 0x7f800000 {
        return x - x;
    }

    /* general argument reduction needed */
    let (n, y) = rem_pio2f(x);
    match n & 3 {
        0 => k_cosf(y),
        1 => k_sinf(-y),
        2 => -k_cosf(y),
        _ => k_sinf(y),
    }
}
