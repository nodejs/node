/* origin: FreeBSD /usr/src/lib/msun/src/s_erff.c */
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

use super::{expf, fabsf};

const ERX: f32 = 8.4506291151e-01; /* 0x3f58560b */
/*
 * Coefficients for approximation to  erf on [0,0.84375]
 */
const EFX8: f32 = 1.0270333290e+00; /* 0x3f8375d4 */
const PP0: f32 = 1.2837916613e-01; /* 0x3e0375d4 */
const PP1: f32 = -3.2504209876e-01; /* 0xbea66beb */
const PP2: f32 = -2.8481749818e-02; /* 0xbce9528f */
const PP3: f32 = -5.7702702470e-03; /* 0xbbbd1489 */
const PP4: f32 = -2.3763017452e-05; /* 0xb7c756b1 */
const QQ1: f32 = 3.9791721106e-01; /* 0x3ecbbbce */
const QQ2: f32 = 6.5022252500e-02; /* 0x3d852a63 */
const QQ3: f32 = 5.0813062117e-03; /* 0x3ba68116 */
const QQ4: f32 = 1.3249473704e-04; /* 0x390aee49 */
const QQ5: f32 = -3.9602282413e-06; /* 0xb684e21a */
/*
 * Coefficients for approximation to  erf  in [0.84375,1.25]
 */
const PA0: f32 = -2.3621185683e-03; /* 0xbb1acdc6 */
const PA1: f32 = 4.1485610604e-01; /* 0x3ed46805 */
const PA2: f32 = -3.7220788002e-01; /* 0xbebe9208 */
const PA3: f32 = 3.1834661961e-01; /* 0x3ea2fe54 */
const PA4: f32 = -1.1089469492e-01; /* 0xbde31cc2 */
const PA5: f32 = 3.5478305072e-02; /* 0x3d1151b3 */
const PA6: f32 = -2.1663755178e-03; /* 0xbb0df9c0 */
const QA1: f32 = 1.0642088205e-01; /* 0x3dd9f331 */
const QA2: f32 = 5.4039794207e-01; /* 0x3f0a5785 */
const QA3: f32 = 7.1828655899e-02; /* 0x3d931ae7 */
const QA4: f32 = 1.2617121637e-01; /* 0x3e013307 */
const QA5: f32 = 1.3637083583e-02; /* 0x3c5f6e13 */
const QA6: f32 = 1.1984500103e-02; /* 0x3c445aa3 */
/*
 * Coefficients for approximation to  erfc in [1.25,1/0.35]
 */
const RA0: f32 = -9.8649440333e-03; /* 0xbc21a093 */
const RA1: f32 = -6.9385856390e-01; /* 0xbf31a0b7 */
const RA2: f32 = -1.0558626175e+01; /* 0xc128f022 */
const RA3: f32 = -6.2375331879e+01; /* 0xc2798057 */
const RA4: f32 = -1.6239666748e+02; /* 0xc322658c */
const RA5: f32 = -1.8460508728e+02; /* 0xc3389ae7 */
const RA6: f32 = -8.1287437439e+01; /* 0xc2a2932b */
const RA7: f32 = -9.8143291473e+00; /* 0xc11d077e */
const SA1: f32 = 1.9651271820e+01; /* 0x419d35ce */
const SA2: f32 = 1.3765776062e+02; /* 0x4309a863 */
const SA3: f32 = 4.3456588745e+02; /* 0x43d9486f */
const SA4: f32 = 6.4538726807e+02; /* 0x442158c9 */
const SA5: f32 = 4.2900814819e+02; /* 0x43d6810b */
const SA6: f32 = 1.0863500214e+02; /* 0x42d9451f */
const SA7: f32 = 6.5702495575e+00; /* 0x40d23f7c */
const SA8: f32 = -6.0424413532e-02; /* 0xbd777f97 */
/*
 * Coefficients for approximation to  erfc in [1/.35,28]
 */
const RB0: f32 = -9.8649431020e-03; /* 0xbc21a092 */
const RB1: f32 = -7.9928326607e-01; /* 0xbf4c9dd4 */
const RB2: f32 = -1.7757955551e+01; /* 0xc18e104b */
const RB3: f32 = -1.6063638306e+02; /* 0xc320a2ea */
const RB4: f32 = -6.3756646729e+02; /* 0xc41f6441 */
const RB5: f32 = -1.0250950928e+03; /* 0xc480230b */
const RB6: f32 = -4.8351919556e+02; /* 0xc3f1c275 */
const SB1: f32 = 3.0338060379e+01; /* 0x41f2b459 */
const SB2: f32 = 3.2579251099e+02; /* 0x43a2e571 */
const SB3: f32 = 1.5367296143e+03; /* 0x44c01759 */
const SB4: f32 = 3.1998581543e+03; /* 0x4547fdbb */
const SB5: f32 = 2.5530502930e+03; /* 0x451f90ce */
const SB6: f32 = 4.7452853394e+02; /* 0x43ed43a7 */
const SB7: f32 = -2.2440952301e+01; /* 0xc1b38712 */

fn erfc1(x: f32) -> f32 {
    let s: f32;
    let p: f32;
    let q: f32;

    s = fabsf(x) - 1.0;
    p = PA0 + s * (PA1 + s * (PA2 + s * (PA3 + s * (PA4 + s * (PA5 + s * PA6)))));
    q = 1.0 + s * (QA1 + s * (QA2 + s * (QA3 + s * (QA4 + s * (QA5 + s * QA6)))));
    return 1.0 - ERX - p / q;
}

fn erfc2(mut ix: u32, mut x: f32) -> f32 {
    let s: f32;
    let r: f32;
    let big_s: f32;
    let z: f32;

    if ix < 0x3fa00000 {
        /* |x| < 1.25 */
        return erfc1(x);
    }

    x = fabsf(x);
    s = 1.0 / (x * x);
    if ix < 0x4036db6d {
        /* |x| < 1/0.35 */
        r = RA0 + s * (RA1 + s * (RA2 + s * (RA3 + s * (RA4 + s * (RA5 + s * (RA6 + s * RA7))))));
        big_s = 1.0
            + s * (SA1
                + s * (SA2 + s * (SA3 + s * (SA4 + s * (SA5 + s * (SA6 + s * (SA7 + s * SA8)))))));
    } else {
        /* |x| >= 1/0.35 */
        r = RB0 + s * (RB1 + s * (RB2 + s * (RB3 + s * (RB4 + s * (RB5 + s * RB6)))));
        big_s =
            1.0 + s * (SB1 + s * (SB2 + s * (SB3 + s * (SB4 + s * (SB5 + s * (SB6 + s * SB7))))));
    }
    ix = x.to_bits();
    z = f32::from_bits(ix & 0xffffe000);

    expf(-z * z - 0.5625) * expf((z - x) * (z + x) + r / big_s) / x
}

/// Error function (f32)
///
/// Calculates an approximation to the “error function”, which estimates
/// the probability that an observation will fall within x standard
/// deviations of the mean (assuming a normal distribution).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn erff(x: f32) -> f32 {
    let r: f32;
    let s: f32;
    let z: f32;
    let y: f32;
    let mut ix: u32;
    let sign: usize;

    ix = x.to_bits();
    sign = (ix >> 31) as usize;
    ix &= 0x7fffffff;
    if ix >= 0x7f800000 {
        /* erf(nan)=nan, erf(+-inf)=+-1 */
        return 1.0 - 2.0 * (sign as f32) + 1.0 / x;
    }
    if ix < 0x3f580000 {
        /* |x| < 0.84375 */
        if ix < 0x31800000 {
            /* |x| < 2**-28 */
            /*avoid underflow */
            return 0.125 * (8.0 * x + EFX8 * x);
        }
        z = x * x;
        r = PP0 + z * (PP1 + z * (PP2 + z * (PP3 + z * PP4)));
        s = 1.0 + z * (QQ1 + z * (QQ2 + z * (QQ3 + z * (QQ4 + z * QQ5))));
        y = r / s;
        return x + x * y;
    }
    if ix < 0x40c00000 {
        /* |x| < 6 */
        y = 1.0 - erfc2(ix, x);
    } else {
        let x1p_120 = f32::from_bits(0x03800000);
        y = 1.0 - x1p_120;
    }

    if sign != 0 { -y } else { y }
}

/// Complementary error function (f32)
///
/// Calculates the complementary probability.
/// Is `1 - erf(x)`. Is computed directly, so that you can use it to avoid
/// the loss of precision that would result from subtracting
/// large probabilities (on large `x`) from 1.
pub fn erfcf(x: f32) -> f32 {
    let r: f32;
    let s: f32;
    let z: f32;
    let y: f32;
    let mut ix: u32;
    let sign: usize;

    ix = x.to_bits();
    sign = (ix >> 31) as usize;
    ix &= 0x7fffffff;
    if ix >= 0x7f800000 {
        /* erfc(nan)=nan, erfc(+-inf)=0,2 */
        return 2.0 * (sign as f32) + 1.0 / x;
    }

    if ix < 0x3f580000 {
        /* |x| < 0.84375 */
        if ix < 0x23800000 {
            /* |x| < 2**-56 */
            return 1.0 - x;
        }
        z = x * x;
        r = PP0 + z * (PP1 + z * (PP2 + z * (PP3 + z * PP4)));
        s = 1.0 + z * (QQ1 + z * (QQ2 + z * (QQ3 + z * (QQ4 + z * QQ5))));
        y = r / s;
        if sign != 0 || ix < 0x3e800000 {
            /* x < 1/4 */
            return 1.0 - (x + x * y);
        }
        return 0.5 - (x - 0.5 + x * y);
    }
    if ix < 0x41e00000 {
        /* |x| < 28 */
        if sign != 0 {
            return 2.0 - erfc2(ix, x);
        } else {
            return erfc2(ix, x);
        }
    }

    let x1p_120 = f32::from_bits(0x03800000);
    if sign != 0 {
        2.0 - x1p_120
    } else {
        x1p_120 * x1p_120
    }
}
