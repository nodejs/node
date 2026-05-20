/* origin: FreeBSD /usr/src/lib/msun/src/s_expm1f.c */
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

const O_THRESHOLD: f32 = 8.8721679688e+01; /* 0x42b17180 */
const LN2_HI: f32 = 6.9313812256e-01; /* 0x3f317180 */
const LN2_LO: f32 = 9.0580006145e-06; /* 0x3717f7d1 */
const INV_LN2: f32 = 1.4426950216e+00; /* 0x3fb8aa3b */
/*
 * Domain [-0.34568, 0.34568], range ~[-6.694e-10, 6.696e-10]:
 * |6 / x * (1 + 2 * (1 / (exp(x) - 1) - 1 / x)) - q(x)| < 2**-30.04
 * Scaled coefficients: Qn_here = 2**n * Qn_for_q (see s_expm1.c):
 */
const Q1: f32 = -3.3333212137e-2; /* -0x888868.0p-28 */
const Q2: f32 = 1.5807170421e-3; /*  0xcf3010.0p-33 */

/// Exponential, base *e*, of x-1 (f32)
///
/// Calculates the exponential of `x` and subtract 1, that is, *e* raised
/// to the power `x` minus 1 (where *e* is the base of the natural
/// system of logarithms, approximately 2.71828).
/// The result is accurate even for small values of `x`,
/// where using `exp(x)-1` would lose many significant digits.
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn expm1f(mut x: f32) -> f32 {
    let x1p127 = f32::from_bits(0x7f000000); // 0x1p127f === 2 ^ 127

    let mut hx = x.to_bits();
    let sign = (hx >> 31) != 0;
    hx &= 0x7fffffff;

    /* filter out huge and non-finite argument */
    if hx >= 0x4195b844 {
        /* if |x|>=27*ln2 */
        if hx > 0x7f800000 {
            /* NaN */
            return x;
        }
        if sign {
            return -1.;
        }
        if x > O_THRESHOLD {
            x *= x1p127;
            return x;
        }
    }

    let k: i32;
    let hi: f32;
    let lo: f32;
    let mut c = 0f32;
    /* argument reduction */
    if hx > 0x3eb17218 {
        /* if  |x| > 0.5 ln2 */
        if hx < 0x3F851592 {
            /* and |x| < 1.5 ln2 */
            if !sign {
                hi = x - LN2_HI;
                lo = LN2_LO;
                k = 1;
            } else {
                hi = x + LN2_HI;
                lo = -LN2_LO;
                k = -1;
            }
        } else {
            k = (INV_LN2 * x + (if sign { -0.5 } else { 0.5 })) as i32;
            let t = k as f32;
            hi = x - t * LN2_HI; /* t*ln2_hi is exact here */
            lo = t * LN2_LO;
        }
        x = hi - lo;
        c = (hi - x) - lo;
    } else if hx < 0x33000000 {
        /* when |x|<2**-25, return x */
        if hx < 0x00800000 {
            force_eval!(x * x);
        }
        return x;
    } else {
        k = 0;
    }

    /* x is now in primary range */
    let hfx = 0.5 * x;
    let hxs = x * hfx;
    let r1 = 1. + hxs * (Q1 + hxs * Q2);
    let t = 3. - r1 * hfx;
    let mut e = hxs * ((r1 - t) / (6. - x * t));
    if k == 0 {
        /* c is 0 */
        return x - (x * e - hxs);
    }
    e = x * (e - c) - c;
    e -= hxs;
    /* exp(x) ~ 2^k (x_reduced - e + 1) */
    if k == -1 {
        return 0.5 * (x - e) - 0.5;
    }
    if k == 1 {
        if x < -0.25 {
            return -2. * (e - (x + 0.5));
        }
        return 1. + 2. * (x - e);
    }
    let twopk = f32::from_bits(((0x7f + k) << 23) as u32); /* 2^k */
    if !(0..=56).contains(&k) {
        /* suffice to return exp(x)-1 */
        let mut y = x - e + 1.;
        if k == 128 {
            y = y * 2. * x1p127;
        } else {
            y = y * twopk;
        }
        return y - 1.;
    }
    let uf = f32::from_bits(((0x7f - k) << 23) as u32); /* 2^-k */
    if k < 23 {
        (x - e + (1. - uf)) * twopk
    } else {
        (x - (e + uf) + 1.) * twopk
    }
}
