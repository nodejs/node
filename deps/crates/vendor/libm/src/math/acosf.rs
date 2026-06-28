/* origin: FreeBSD /usr/src/lib/msun/src/e_acosf.c */
/*
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
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

use super::sqrt::sqrtf;

const PIO2_HI: f32 = 1.5707962513e+00; /* 0x3fc90fda */
const PIO2_LO: f32 = 7.5497894159e-08; /* 0x33a22168 */
const P_S0: f32 = 1.6666586697e-01;
const P_S1: f32 = -4.2743422091e-02;
const P_S2: f32 = -8.6563630030e-03;
const Q_S1: f32 = -7.0662963390e-01;

fn r(z: f32) -> f32 {
    let p = z * (P_S0 + z * (P_S1 + z * P_S2));
    let q = 1. + z * Q_S1;
    p / q
}

/// Arccosine (f32)
///
/// Computes the inverse cosine (arc cosine) of the input value.
/// Arguments must be in the range -1 to 1.
/// Returns values in radians, in the range of 0 to pi.
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn acosf(x: f32) -> f32 {
    let x1p_120 = f32::from_bits(0x03800000); // 0x1p-120 === 2 ^ (-120)

    let z: f32;
    let w: f32;
    let s: f32;

    let mut hx = x.to_bits();
    let ix = hx & 0x7fffffff;
    /* |x| >= 1 or nan */
    if ix >= 0x3f800000 {
        if ix == 0x3f800000 {
            if (hx >> 31) != 0 {
                return 2. * PIO2_HI + x1p_120;
            }
            return 0.;
        }
        return 0. / (x - x);
    }
    /* |x| < 0.5 */
    if ix < 0x3f000000 {
        if ix <= 0x32800000 {
            /* |x| < 2**-26 */
            return PIO2_HI + x1p_120;
        }
        return PIO2_HI - (x - (PIO2_LO - x * r(x * x)));
    }
    /* x < -0.5 */
    if (hx >> 31) != 0 {
        z = (1. + x) * 0.5;
        s = sqrtf(z);
        w = r(z) * s - PIO2_LO;
        return 2. * (PIO2_HI - (s + w));
    }
    /* x > 0.5 */
    z = (1. - x) * 0.5;
    s = sqrtf(z);
    hx = s.to_bits();
    let df = f32::from_bits(hx & 0xfffff000);
    let c = (z - df * df) / (s + df);
    w = r(z) * s + c;
    2. * (df + w)
}
