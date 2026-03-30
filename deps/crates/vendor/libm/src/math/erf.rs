use super::{exp, fabs, get_high_word, with_set_low_word};
/* origin: FreeBSD /usr/src/lib/msun/src/s_erf.c */
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
/* double erf(double x)
 * double erfc(double x)
 *                           x
 *                    2      |\
 *     erf(x)  =  ---------  | exp(-t*t)dt
 *                 sqrt(pi) \|
 *                           0
 *
 *     erfc(x) =  1-erf(x)
 *  Note that
 *              erf(-x) = -erf(x)
 *              erfc(-x) = 2 - erfc(x)
 *
 * Method:
 *      1. For |x| in [0, 0.84375]
 *          erf(x)  = x + x*R(x^2)
 *          erfc(x) = 1 - erf(x)           if x in [-.84375,0.25]
 *                  = 0.5 + ((0.5-x)-x*R)  if x in [0.25,0.84375]
 *         where R = P/Q where P is an odd poly of degree 8 and
 *         Q is an odd poly of degree 10.
 *                                               -57.90
 *                      | R - (erf(x)-x)/x | <= 2
 *
 *
 *         Remark. The formula is derived by noting
 *          erf(x) = (2/sqrt(pi))*(x - x^3/3 + x^5/10 - x^7/42 + ....)
 *         and that
 *          2/sqrt(pi) = 1.128379167095512573896158903121545171688
 *         is close to one. The interval is chosen because the fix
 *         point of erf(x) is near 0.6174 (i.e., erf(x)=x when x is
 *         near 0.6174), and by some experiment, 0.84375 is chosen to
 *         guarantee the error is less than one ulp for erf.
 *
 *      2. For |x| in [0.84375,1.25], let s = |x| - 1, and
 *         c = 0.84506291151 rounded to single (24 bits)
 *              erf(x)  = sign(x) * (c  + P1(s)/Q1(s))
 *              erfc(x) = (1-c)  - P1(s)/Q1(s) if x > 0
 *                        1+(c+P1(s)/Q1(s))    if x < 0
 *              |P1/Q1 - (erf(|x|)-c)| <= 2**-59.06
 *         Remark: here we use the taylor series expansion at x=1.
 *              erf(1+s) = erf(1) + s*Poly(s)
 *                       = 0.845.. + P1(s)/Q1(s)
 *         That is, we use rational approximation to approximate
 *                      erf(1+s) - (c = (single)0.84506291151)
 *         Note that |P1/Q1|< 0.078 for x in [0.84375,1.25]
 *         where
 *              P1(s) = degree 6 poly in s
 *              Q1(s) = degree 6 poly in s
 *
 *      3. For x in [1.25,1/0.35(~2.857143)],
 *              erfc(x) = (1/x)*exp(-x*x-0.5625+R1/S1)
 *              erf(x)  = 1 - erfc(x)
 *         where
 *              R1(z) = degree 7 poly in z, (z=1/x^2)
 *              S1(z) = degree 8 poly in z
 *
 *      4. For x in [1/0.35,28]
 *              erfc(x) = (1/x)*exp(-x*x-0.5625+R2/S2) if x > 0
 *                      = 2.0 - (1/x)*exp(-x*x-0.5625+R2/S2) if -6<x<0
 *                      = 2.0 - tiny            (if x <= -6)
 *              erf(x)  = sign(x)*(1.0 - erfc(x)) if x < 6, else
 *              erf(x)  = sign(x)*(1.0 - tiny)
 *         where
 *              R2(z) = degree 6 poly in z, (z=1/x^2)
 *              S2(z) = degree 7 poly in z
 *
 *      Note1:
 *         To compute exp(-x*x-0.5625+R/S), let s be a single
 *         precision number and s := x; then
 *              -x*x = -s*s + (s-x)*(s+x)
 *              exp(-x*x-0.5626+R/S) =
 *                      exp(-s*s-0.5625)*exp((s-x)*(s+x)+R/S);
 *      Note2:
 *         Here 4 and 5 make use of the asymptotic series
 *                        exp(-x*x)
 *              erfc(x) ~ ---------- * ( 1 + Poly(1/x^2) )
 *                        x*sqrt(pi)
 *         We use rational approximation to approximate
 *              g(s)=f(1/x^2) = log(erfc(x)*x) - x*x + 0.5625
 *         Here is the error bound for R1/S1 and R2/S2
 *              |R1/S1 - f(x)|  < 2**(-62.57)
 *              |R2/S2 - f(x)|  < 2**(-61.52)
 *
 *      5. For inf > x >= 28
 *              erf(x)  = sign(x) *(1 - tiny)  (raise inexact)
 *              erfc(x) = tiny*tiny (raise underflow) if x > 0
 *                      = 2 - tiny if x<0
 *
 *      7. Special case:
 *              erf(0)  = 0, erf(inf)  = 1, erf(-inf) = -1,
 *              erfc(0) = 1, erfc(inf) = 0, erfc(-inf) = 2,
 *              erfc/erf(NaN) is NaN
 */

const ERX: f64 = 8.45062911510467529297e-01; /* 0x3FEB0AC1, 0x60000000 */
/*
 * Coefficients for approximation to  erf on [0,0.84375]
 */
const EFX8: f64 = 1.02703333676410069053e+00; /* 0x3FF06EBA, 0x8214DB69 */
const PP0: f64 = 1.28379167095512558561e-01; /* 0x3FC06EBA, 0x8214DB68 */
const PP1: f64 = -3.25042107247001499370e-01; /* 0xBFD4CD7D, 0x691CB913 */
const PP2: f64 = -2.84817495755985104766e-02; /* 0xBF9D2A51, 0xDBD7194F */
const PP3: f64 = -5.77027029648944159157e-03; /* 0xBF77A291, 0x236668E4 */
const PP4: f64 = -2.37630166566501626084e-05; /* 0xBEF8EAD6, 0x120016AC */
const QQ1: f64 = 3.97917223959155352819e-01; /* 0x3FD97779, 0xCDDADC09 */
const QQ2: f64 = 6.50222499887672944485e-02; /* 0x3FB0A54C, 0x5536CEBA */
const QQ3: f64 = 5.08130628187576562776e-03; /* 0x3F74D022, 0xC4D36B0F */
const QQ4: f64 = 1.32494738004321644526e-04; /* 0x3F215DC9, 0x221C1A10 */
const QQ5: f64 = -3.96022827877536812320e-06; /* 0xBED09C43, 0x42A26120 */
/*
 * Coefficients for approximation to  erf  in [0.84375,1.25]
 */
const PA0: f64 = -2.36211856075265944077e-03; /* 0xBF6359B8, 0xBEF77538 */
const PA1: f64 = 4.14856118683748331666e-01; /* 0x3FDA8D00, 0xAD92B34D */
const PA2: f64 = -3.72207876035701323847e-01; /* 0xBFD7D240, 0xFBB8C3F1 */
const PA3: f64 = 3.18346619901161753674e-01; /* 0x3FD45FCA, 0x805120E4 */
const PA4: f64 = -1.10894694282396677476e-01; /* 0xBFBC6398, 0x3D3E28EC */
const PA5: f64 = 3.54783043256182359371e-02; /* 0x3FA22A36, 0x599795EB */
const PA6: f64 = -2.16637559486879084300e-03; /* 0xBF61BF38, 0x0A96073F */
const QA1: f64 = 1.06420880400844228286e-01; /* 0x3FBB3E66, 0x18EEE323 */
const QA2: f64 = 5.40397917702171048937e-01; /* 0x3FE14AF0, 0x92EB6F33 */
const QA3: f64 = 7.18286544141962662868e-02; /* 0x3FB2635C, 0xD99FE9A7 */
const QA4: f64 = 1.26171219808761642112e-01; /* 0x3FC02660, 0xE763351F */
const QA5: f64 = 1.36370839120290507362e-02; /* 0x3F8BEDC2, 0x6B51DD1C */
const QA6: f64 = 1.19844998467991074170e-02; /* 0x3F888B54, 0x5735151D */
/*
 * Coefficients for approximation to  erfc in [1.25,1/0.35]
 */
const RA0: f64 = -9.86494403484714822705e-03; /* 0xBF843412, 0x600D6435 */
const RA1: f64 = -6.93858572707181764372e-01; /* 0xBFE63416, 0xE4BA7360 */
const RA2: f64 = -1.05586262253232909814e+01; /* 0xC0251E04, 0x41B0E726 */
const RA3: f64 = -6.23753324503260060396e+01; /* 0xC04F300A, 0xE4CBA38D */
const RA4: f64 = -1.62396669462573470355e+02; /* 0xC0644CB1, 0x84282266 */
const RA5: f64 = -1.84605092906711035994e+02; /* 0xC067135C, 0xEBCCABB2 */
const RA6: f64 = -8.12874355063065934246e+01; /* 0xC0545265, 0x57E4D2F2 */
const RA7: f64 = -9.81432934416914548592e+00; /* 0xC023A0EF, 0xC69AC25C */
const SA1: f64 = 1.96512716674392571292e+01; /* 0x4033A6B9, 0xBD707687 */
const SA2: f64 = 1.37657754143519042600e+02; /* 0x4061350C, 0x526AE721 */
const SA3: f64 = 4.34565877475229228821e+02; /* 0x407B290D, 0xD58A1A71 */
const SA4: f64 = 6.45387271733267880336e+02; /* 0x40842B19, 0x21EC2868 */
const SA5: f64 = 4.29008140027567833386e+02; /* 0x407AD021, 0x57700314 */
const SA6: f64 = 1.08635005541779435134e+02; /* 0x405B28A3, 0xEE48AE2C */
const SA7: f64 = 6.57024977031928170135e+00; /* 0x401A47EF, 0x8E484A93 */
const SA8: f64 = -6.04244152148580987438e-02; /* 0xBFAEEFF2, 0xEE749A62 */
/*
 * Coefficients for approximation to  erfc in [1/.35,28]
 */
const RB0: f64 = -9.86494292470009928597e-03; /* 0xBF843412, 0x39E86F4A */
const RB1: f64 = -7.99283237680523006574e-01; /* 0xBFE993BA, 0x70C285DE */
const RB2: f64 = -1.77579549177547519889e+01; /* 0xC031C209, 0x555F995A */
const RB3: f64 = -1.60636384855821916062e+02; /* 0xC064145D, 0x43C5ED98 */
const RB4: f64 = -6.37566443368389627722e+02; /* 0xC083EC88, 0x1375F228 */
const RB5: f64 = -1.02509513161107724954e+03; /* 0xC0900461, 0x6A2E5992 */
const RB6: f64 = -4.83519191608651397019e+02; /* 0xC07E384E, 0x9BDC383F */
const SB1: f64 = 3.03380607434824582924e+01; /* 0x403E568B, 0x261D5190 */
const SB2: f64 = 3.25792512996573918826e+02; /* 0x40745CAE, 0x221B9F0A */
const SB3: f64 = 1.53672958608443695994e+03; /* 0x409802EB, 0x189D5118 */
const SB4: f64 = 3.19985821950859553908e+03; /* 0x40A8FFB7, 0x688C246A */
const SB5: f64 = 2.55305040643316442583e+03; /* 0x40A3F219, 0xCEDF3BE6 */
const SB6: f64 = 4.74528541206955367215e+02; /* 0x407DA874, 0xE79FE763 */
const SB7: f64 = -2.24409524465858183362e+01; /* 0xC03670E2, 0x42712D62 */

fn erfc1(x: f64) -> f64 {
    let s: f64;
    let p: f64;
    let q: f64;

    s = fabs(x) - 1.0;
    p = PA0 + s * (PA1 + s * (PA2 + s * (PA3 + s * (PA4 + s * (PA5 + s * PA6)))));
    q = 1.0 + s * (QA1 + s * (QA2 + s * (QA3 + s * (QA4 + s * (QA5 + s * QA6)))));

    1.0 - ERX - p / q
}

fn erfc2(ix: u32, mut x: f64) -> f64 {
    let s: f64;
    let r: f64;
    let big_s: f64;
    let z: f64;

    if ix < 0x3ff40000 {
        /* |x| < 1.25 */
        return erfc1(x);
    }

    x = fabs(x);
    s = 1.0 / (x * x);
    if ix < 0x4006db6d {
        /* |x| < 1/.35 ~ 2.85714 */
        r = RA0 + s * (RA1 + s * (RA2 + s * (RA3 + s * (RA4 + s * (RA5 + s * (RA6 + s * RA7))))));
        big_s = 1.0
            + s * (SA1
                + s * (SA2 + s * (SA3 + s * (SA4 + s * (SA5 + s * (SA6 + s * (SA7 + s * SA8)))))));
    } else {
        /* |x| > 1/.35 */
        r = RB0 + s * (RB1 + s * (RB2 + s * (RB3 + s * (RB4 + s * (RB5 + s * RB6)))));
        big_s =
            1.0 + s * (SB1 + s * (SB2 + s * (SB3 + s * (SB4 + s * (SB5 + s * (SB6 + s * SB7))))));
    }
    z = with_set_low_word(x, 0);

    exp(-z * z - 0.5625) * exp((z - x) * (z + x) + r / big_s) / x
}

/// Error function (f64)
///
/// Calculates an approximation to the “error function”, which estimates
/// the probability that an observation will fall within x standard
/// deviations of the mean (assuming a normal distribution).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn erf(x: f64) -> f64 {
    let r: f64;
    let s: f64;
    let z: f64;
    let y: f64;
    let mut ix: u32;
    let sign: usize;

    ix = get_high_word(x);
    sign = (ix >> 31) as usize;
    ix &= 0x7fffffff;
    if ix >= 0x7ff00000 {
        /* erf(nan)=nan, erf(+-inf)=+-1 */
        return 1.0 - 2.0 * (sign as f64) + 1.0 / x;
    }
    if ix < 0x3feb0000 {
        /* |x| < 0.84375 */
        if ix < 0x3e300000 {
            /* |x| < 2**-28 */
            /* avoid underflow */
            return 0.125 * (8.0 * x + EFX8 * x);
        }
        z = x * x;
        r = PP0 + z * (PP1 + z * (PP2 + z * (PP3 + z * PP4)));
        s = 1.0 + z * (QQ1 + z * (QQ2 + z * (QQ3 + z * (QQ4 + z * QQ5))));
        y = r / s;
        return x + x * y;
    }
    if ix < 0x40180000 {
        /* 0.84375 <= |x| < 6 */
        y = 1.0 - erfc2(ix, x);
    } else {
        let x1p_1022 = f64::from_bits(0x0010000000000000);
        y = 1.0 - x1p_1022;
    }

    if sign != 0 { -y } else { y }
}

/// Complementary error function (f64)
///
/// Calculates the complementary probability.
/// Is `1 - erf(x)`. Is computed directly, so that you can use it to avoid
/// the loss of precision that would result from subtracting
/// large probabilities (on large `x`) from 1.
pub fn erfc(x: f64) -> f64 {
    let r: f64;
    let s: f64;
    let z: f64;
    let y: f64;
    let mut ix: u32;
    let sign: usize;

    ix = get_high_word(x);
    sign = (ix >> 31) as usize;
    ix &= 0x7fffffff;
    if ix >= 0x7ff00000 {
        /* erfc(nan)=nan, erfc(+-inf)=0,2 */
        return 2.0 * (sign as f64) + 1.0 / x;
    }
    if ix < 0x3feb0000 {
        /* |x| < 0.84375 */
        if ix < 0x3c700000 {
            /* |x| < 2**-56 */
            return 1.0 - x;
        }
        z = x * x;
        r = PP0 + z * (PP1 + z * (PP2 + z * (PP3 + z * PP4)));
        s = 1.0 + z * (QQ1 + z * (QQ2 + z * (QQ3 + z * (QQ4 + z * QQ5))));
        y = r / s;
        if sign != 0 || ix < 0x3fd00000 {
            /* x < 1/4 */
            return 1.0 - (x + x * y);
        }
        return 0.5 - (x - 0.5 + x * y);
    }
    if ix < 0x403c0000 {
        /* 0.84375 <= |x| < 28 */
        if sign != 0 {
            return 2.0 - erfc2(ix, x);
        } else {
            return erfc2(ix, x);
        }
    }

    let x1p_1022 = f64::from_bits(0x0010000000000000);
    if sign != 0 {
        2.0 - x1p_1022
    } else {
        x1p_1022 * x1p_1022
    }
}
