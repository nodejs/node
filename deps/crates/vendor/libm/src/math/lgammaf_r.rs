/* origin: FreeBSD /usr/src/lib/msun/src/e_lgammaf_r.c */
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

use super::{floorf, k_cosf, k_sinf, logf};

const PI: f32 = 3.1415927410e+00; /* 0x40490fdb */
const A0: f32 = 7.7215664089e-02; /* 0x3d9e233f */
const A1: f32 = 3.2246702909e-01; /* 0x3ea51a66 */
const A2: f32 = 6.7352302372e-02; /* 0x3d89f001 */
const A3: f32 = 2.0580807701e-02; /* 0x3ca89915 */
const A4: f32 = 7.3855509982e-03; /* 0x3bf2027e */
const A5: f32 = 2.8905137442e-03; /* 0x3b3d6ec6 */
const A6: f32 = 1.1927076848e-03; /* 0x3a9c54a1 */
const A7: f32 = 5.1006977446e-04; /* 0x3a05b634 */
const A8: f32 = 2.2086278477e-04; /* 0x39679767 */
const A9: f32 = 1.0801156895e-04; /* 0x38e28445 */
const A10: f32 = 2.5214456400e-05; /* 0x37d383a2 */
const A11: f32 = 4.4864096708e-05; /* 0x383c2c75 */
const TC: f32 = 1.4616321325e+00; /* 0x3fbb16c3 */
const TF: f32 = -1.2148628384e-01; /* 0xbdf8cdcd */
/* TT = -(tail of TF) */
const TT: f32 = 6.6971006518e-09; /* 0x31e61c52 */
const T0: f32 = 4.8383611441e-01; /* 0x3ef7b95e */
const T1: f32 = -1.4758771658e-01; /* 0xbe17213c */
const T2: f32 = 6.4624942839e-02; /* 0x3d845a15 */
const T3: f32 = -3.2788541168e-02; /* 0xbd064d47 */
const T4: f32 = 1.7970675603e-02; /* 0x3c93373d */
const T5: f32 = -1.0314224288e-02; /* 0xbc28fcfe */
const T6: f32 = 6.1005386524e-03; /* 0x3bc7e707 */
const T7: f32 = -3.6845202558e-03; /* 0xbb7177fe */
const T8: f32 = 2.2596477065e-03; /* 0x3b141699 */
const T9: f32 = -1.4034647029e-03; /* 0xbab7f476 */
const T10: f32 = 8.8108185446e-04; /* 0x3a66f867 */
const T11: f32 = -5.3859531181e-04; /* 0xba0d3085 */
const T12: f32 = 3.1563205994e-04; /* 0x39a57b6b */
const T13: f32 = -3.1275415677e-04; /* 0xb9a3f927 */
const T14: f32 = 3.3552918467e-04; /* 0x39afe9f7 */
const U0: f32 = -7.7215664089e-02; /* 0xbd9e233f */
const U1: f32 = 6.3282704353e-01; /* 0x3f2200f4 */
const U2: f32 = 1.4549225569e+00; /* 0x3fba3ae7 */
const U3: f32 = 9.7771751881e-01; /* 0x3f7a4bb2 */
const U4: f32 = 2.2896373272e-01; /* 0x3e6a7578 */
const U5: f32 = 1.3381091878e-02; /* 0x3c5b3c5e */
const V1: f32 = 2.4559779167e+00; /* 0x401d2ebe */
const V2: f32 = 2.1284897327e+00; /* 0x4008392d */
const V3: f32 = 7.6928514242e-01; /* 0x3f44efdf */
const V4: f32 = 1.0422264785e-01; /* 0x3dd572af */
const V5: f32 = 3.2170924824e-03; /* 0x3b52d5db */
const S0: f32 = -7.7215664089e-02; /* 0xbd9e233f */
const S1: f32 = 2.1498242021e-01; /* 0x3e5c245a */
const S2: f32 = 3.2577878237e-01; /* 0x3ea6cc7a */
const S3: f32 = 1.4635047317e-01; /* 0x3e15dce6 */
const S4: f32 = 2.6642270386e-02; /* 0x3cda40e4 */
const S5: f32 = 1.8402845599e-03; /* 0x3af135b4 */
const S6: f32 = 3.1947532989e-05; /* 0x3805ff67 */
const R1: f32 = 1.3920053244e+00; /* 0x3fb22d3b */
const R2: f32 = 7.2193557024e-01; /* 0x3f38d0c5 */
const R3: f32 = 1.7193385959e-01; /* 0x3e300f6e */
const R4: f32 = 1.8645919859e-02; /* 0x3c98bf54 */
const R5: f32 = 7.7794247773e-04; /* 0x3a4beed6 */
const R6: f32 = 7.3266842264e-06; /* 0x36f5d7bd */
const W0: f32 = 4.1893854737e-01; /* 0x3ed67f1d */
const W1: f32 = 8.3333335817e-02; /* 0x3daaaaab */
const W2: f32 = -2.7777778450e-03; /* 0xbb360b61 */
const W3: f32 = 7.9365057172e-04; /* 0x3a500cfd */
const W4: f32 = -5.9518753551e-04; /* 0xba1c065c */
const W5: f32 = 8.3633989561e-04; /* 0x3a5b3dd2 */
const W6: f32 = -1.6309292987e-03; /* 0xbad5c4e8 */

/* sin(PI*x) assuming x > 2^-100, if sin(PI*x)==0 the sign is arbitrary */
fn sin_pi(mut x: f32) -> f32 {
    let mut y: f64;
    let mut n: isize;

    /* spurious inexact if odd int */
    x = 2.0 * (x * 0.5 - floorf(x * 0.5)); /* x mod 2.0 */

    n = (x * 4.0) as isize;
    n = div!(n + 1, 2);
    y = (x as f64) - (n as f64) * 0.5;
    y *= 3.14159265358979323846;
    match n {
        1 => k_cosf(y),
        2 => k_sinf(-y),
        3 => -k_cosf(y),
        // 0
        _ => k_sinf(y),
    }
}

#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn lgammaf_r(mut x: f32) -> (f32, i32) {
    let u = x.to_bits();
    let mut t: f32;
    let y: f32;
    let mut z: f32;
    let nadj: f32;
    let p: f32;
    let p1: f32;
    let p2: f32;
    let p3: f32;
    let q: f32;
    let mut r: f32;
    let w: f32;
    let ix: u32;
    let i: i32;
    let sign: bool;
    let mut signgam: i32;

    /* purge off +-inf, NaN, +-0, tiny and negative arguments */
    signgam = 1;
    sign = (u >> 31) != 0;
    ix = u & 0x7fffffff;
    if ix >= 0x7f800000 {
        return (x * x, signgam);
    }
    if ix < 0x35000000 {
        /* |x| < 2**-21, return -log(|x|) */
        if sign {
            signgam = -1;
            x = -x;
        }
        return (-logf(x), signgam);
    }
    if sign {
        x = -x;
        t = sin_pi(x);
        if t == 0.0 {
            /* -integer */
            return (1.0 / (x - x), signgam);
        }
        if t > 0.0 {
            signgam = -1;
        } else {
            t = -t;
        }
        nadj = logf(PI / (t * x));
    } else {
        nadj = 0.0;
    }

    /* purge off 1 and 2 */
    if ix == 0x3f800000 || ix == 0x40000000 {
        r = 0.0;
    }
    /* for x < 2.0 */
    else if ix < 0x40000000 {
        if ix <= 0x3f666666 {
            /* lgamma(x) = lgamma(x+1)-log(x) */
            r = -logf(x);
            if ix >= 0x3f3b4a20 {
                y = 1.0 - x;
                i = 0;
            } else if ix >= 0x3e6d3308 {
                y = x - (TC - 1.0);
                i = 1;
            } else {
                y = x;
                i = 2;
            }
        } else {
            r = 0.0;
            if ix >= 0x3fdda618 {
                /* [1.7316,2] */
                y = 2.0 - x;
                i = 0;
            } else if ix >= 0x3F9da620 {
                /* [1.23,1.73] */
                y = x - TC;
                i = 1;
            } else {
                y = x - 1.0;
                i = 2;
            }
        }
        match i {
            0 => {
                z = y * y;
                p1 = A0 + z * (A2 + z * (A4 + z * (A6 + z * (A8 + z * A10))));
                p2 = z * (A1 + z * (A3 + z * (A5 + z * (A7 + z * (A9 + z * A11)))));
                p = y * p1 + p2;
                r += p - 0.5 * y;
            }
            1 => {
                z = y * y;
                w = z * y;
                p1 = T0 + w * (T3 + w * (T6 + w * (T9 + w * T12))); /* parallel comp */
                p2 = T1 + w * (T4 + w * (T7 + w * (T10 + w * T13)));
                p3 = T2 + w * (T5 + w * (T8 + w * (T11 + w * T14)));
                p = z * p1 - (TT - w * (p2 + y * p3));
                r += TF + p;
            }
            2 => {
                p1 = y * (U0 + y * (U1 + y * (U2 + y * (U3 + y * (U4 + y * U5)))));
                p2 = 1.0 + y * (V1 + y * (V2 + y * (V3 + y * (V4 + y * V5))));
                r += -0.5 * y + p1 / p2;
            }
            #[cfg(debug_assertions)]
            _ => unreachable!(),
            #[cfg(not(debug_assertions))]
            _ => {}
        }
    } else if ix < 0x41000000 {
        /* x < 8.0 */
        i = x as i32;
        y = x - (i as f32);
        p = y * (S0 + y * (S1 + y * (S2 + y * (S3 + y * (S4 + y * (S5 + y * S6))))));
        q = 1.0 + y * (R1 + y * (R2 + y * (R3 + y * (R4 + y * (R5 + y * R6)))));
        r = 0.5 * y + p / q;
        z = 1.0; /* lgamma(1+s) = log(s) + lgamma(s) */
        // TODO: In C, this was implemented using switch jumps with fallthrough.
        // Does this implementation have performance problems?
        if i >= 7 {
            z *= y + 6.0;
        }
        if i >= 6 {
            z *= y + 5.0;
        }
        if i >= 5 {
            z *= y + 4.0;
        }
        if i >= 4 {
            z *= y + 3.0;
        }
        if i >= 3 {
            z *= y + 2.0;
            r += logf(z);
        }
    } else if ix < 0x5c800000 {
        /* 8.0 <= x < 2**58 */
        t = logf(x);
        z = 1.0 / x;
        y = z * z;
        w = W0 + z * (W1 + y * (W2 + y * (W3 + y * (W4 + y * (W5 + y * W6)))));
        r = (x - 0.5) * (t - 1.0) + w;
    } else {
        /* 2**58 <= x <= inf */
        r = x * (logf(x) - 1.0);
    }
    if sign {
        r = nadj - r;
    }
    return (r, signgam);
}
