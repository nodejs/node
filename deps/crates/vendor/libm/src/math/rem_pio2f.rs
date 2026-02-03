/* origin: FreeBSD /usr/src/lib/msun/src/e_rem_pio2f.c */
/*
 * Conversion to float by Ian Lance Taylor, Cygnus Support, ian@cygnus.com.
 * Debugged and optimized by Bruce D. Evans.
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

use core::f64;

use super::rem_pio2_large;

const TOINT: f64 = 1.5 / f64::EPSILON;

/// 53 bits of 2/pi
const INV_PIO2: f64 = 6.36619772367581382433e-01; /* 0x3FE45F30, 0x6DC9C883 */
/// first 25 bits of pi/2
const PIO2_1: f64 = 1.57079631090164184570e+00; /* 0x3FF921FB, 0x50000000 */
/// pi/2 - pio2_1
const PIO2_1T: f64 = 1.58932547735281966916e-08; /* 0x3E5110b4, 0x611A6263 */

/// Return the remainder of x rem pi/2 in *y
///
/// use double precision for everything except passing x
/// use __rem_pio2_large() for large x
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub(crate) fn rem_pio2f(x: f32) -> (i32, f64) {
    let x64 = x as f64;

    let mut tx: [f64; 1] = [0.];
    let mut ty: [f64; 1] = [0.];

    let ix = x.to_bits() & 0x7fffffff;
    /* 25+53 bit pi is good enough for medium size */
    if ix < 0x4dc90fdb {
        /* |x| ~< 2^28*(pi/2), medium size */
        /* Use a specialized rint() to get fn.  Assume round-to-nearest. */
        let tmp = x64 * INV_PIO2 + TOINT;
        // force rounding of tmp to it's storage format on x87 to avoid
        // excess precision issues.
        #[cfg(all(target_arch = "x86", not(target_feature = "sse2")))]
        let tmp = force_eval!(tmp);
        let f_n = tmp - TOINT;
        return (f_n as i32, x64 - f_n * PIO2_1 - f_n * PIO2_1T);
    }
    if ix >= 0x7f800000 {
        /* x is inf or NaN */
        return (0, x64 - x64);
    }
    /* scale x into [2^23, 2^24-1] */
    let sign = (x.to_bits() >> 31) != 0;
    let e0 = ((ix >> 23) - (0x7f + 23)) as i32; /* e0 = ilogb(|x|)-23, positive */
    tx[0] = f32::from_bits(ix - (e0 << 23) as u32) as f64;
    let n = rem_pio2_large(&tx, &mut ty, e0, 0);
    if sign {
        return (-n, -ty[0]);
    }
    (n, ty[0])
}
