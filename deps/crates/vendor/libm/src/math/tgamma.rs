/*
"A Precision Approximation of the Gamma Function" - Cornelius Lanczos (1964)
"Lanczos Implementation of the Gamma Function" - Paul Godfrey (2001)
"An Analysis of the Lanczos Gamma Approximation" - Glendon Ralph Pugh (2004)

approximation method:

                        (x - 0.5)         S(x)
Gamma(x) = (x + g - 0.5)         *  ----------------
                                    exp(x + g - 0.5)

with
                 a1      a2      a3            aN
S(x) ~= [ a0 + ----- + ----- + ----- + ... + ----- ]
               x + 1   x + 2   x + 3         x + N

with a0, a1, a2, a3,.. aN constants which depend on g.

for x < 0 the following reflection formula is used:

Gamma(x)*Gamma(-x) = -pi/(x sin(pi x))

most ideas and constants are from boost and python
*/
use super::{exp, floor, k_cos, k_sin, pow};

const PI: f64 = 3.141592653589793238462643383279502884;

/* sin(pi x) with x > 0x1p-100, if sin(pi*x)==0 the sign is arbitrary */
fn sinpi(mut x: f64) -> f64 {
    let mut n: isize;

    /* argument reduction: x = |x| mod 2 */
    /* spurious inexact when x is odd int */
    x = x * 0.5;
    x = 2.0 * (x - floor(x));

    /* reduce x into [-.25,.25] */
    n = (4.0 * x) as isize;
    n = div!(n + 1, 2);
    x -= (n as f64) * 0.5;

    x *= PI;
    match n {
        1 => k_cos(x, 0.0),
        2 => k_sin(-x, 0.0, 0),
        3 => -k_cos(x, 0.0),
        // 0
        _ => k_sin(x, 0.0, 0),
    }
}

const N: usize = 12;
//static const double g = 6.024680040776729583740234375;
const GMHALF: f64 = 5.524680040776729583740234375;
const SNUM: [f64; N + 1] = [
    23531376880.410759688572007674451636754734846804940,
    42919803642.649098768957899047001988850926355848959,
    35711959237.355668049440185451547166705960488635843,
    17921034426.037209699919755754458931112671403265390,
    6039542586.3520280050642916443072979210699388420708,
    1439720407.3117216736632230727949123939715485786772,
    248874557.86205415651146038641322942321632125127801,
    31426415.585400194380614231628318205362874684987640,
    2876370.6289353724412254090516208496135991145378768,
    186056.26539522349504029498971604569928220784236328,
    8071.6720023658162106380029022722506138218516325024,
    210.82427775157934587250973392071336271166969580291,
    2.5066282746310002701649081771338373386264310793408,
];
const SDEN: [f64; N + 1] = [
    0.0,
    39916800.0,
    120543840.0,
    150917976.0,
    105258076.0,
    45995730.0,
    13339535.0,
    2637558.0,
    357423.0,
    32670.0,
    1925.0,
    66.0,
    1.0,
];
/* n! for small integer n */
const FACT: [f64; 23] = [
    1.0,
    1.0,
    2.0,
    6.0,
    24.0,
    120.0,
    720.0,
    5040.0,
    40320.0,
    362880.0,
    3628800.0,
    39916800.0,
    479001600.0,
    6227020800.0,
    87178291200.0,
    1307674368000.0,
    20922789888000.0,
    355687428096000.0,
    6402373705728000.0,
    121645100408832000.0,
    2432902008176640000.0,
    51090942171709440000.0,
    1124000727777607680000.0,
];

/* S(x) rational function for positive x */
fn s(x: f64) -> f64 {
    let mut num: f64 = 0.0;
    let mut den: f64 = 0.0;

    /* to avoid overflow handle large x differently */
    if x < 8.0 {
        for i in (0..=N).rev() {
            num = num * x + i!(SNUM, i);
            den = den * x + i!(SDEN, i);
        }
    } else {
        for i in 0..=N {
            num = num / x + i!(SNUM, i);
            den = den / x + i!(SDEN, i);
        }
    }
    return num / den;
}

/// The [Gamma function](https://en.wikipedia.org/wiki/Gamma_function) (f64).
#[cfg_attr(all(test, assert_no_panic), no_panic::no_panic)]
pub fn tgamma(mut x: f64) -> f64 {
    let u: u64 = x.to_bits();
    let absx: f64;
    let mut y: f64;
    let mut dy: f64;
    let mut z: f64;
    let mut r: f64;
    let ix: u32 = ((u >> 32) as u32) & 0x7fffffff;
    let sign: bool = (u >> 63) != 0;

    /* special cases */
    if ix >= 0x7ff00000 {
        /* tgamma(nan)=nan, tgamma(inf)=inf, tgamma(-inf)=nan with invalid */
        return x + f64::INFINITY;
    }
    if ix < ((0x3ff - 54) << 20) {
        /* |x| < 2^-54: tgamma(x) ~ 1/x, +-0 raises div-by-zero */
        return 1.0 / x;
    }

    /* integer arguments */
    /* raise inexact when non-integer */
    if x == floor(x) {
        if sign {
            return 0.0 / 0.0;
        }
        if x <= FACT.len() as f64 {
            return i!(FACT, (x as usize) - 1);
        }
    }

    /* x >= 172: tgamma(x)=inf with overflow */
    /* x =< -184: tgamma(x)=+-0 with underflow */
    if ix >= 0x40670000 {
        /* |x| >= 184 */
        if sign {
            let x1p_126 = f64::from_bits(0x3810000000000000); // 0x1p-126 == 2^-126
            force_eval!((x1p_126 / x) as f32);
            if floor(x) * 0.5 == floor(x * 0.5) {
                return 0.0;
            } else {
                return -0.0;
            }
        }
        let x1p1023 = f64::from_bits(0x7fe0000000000000); // 0x1p1023 == 2^1023
        x *= x1p1023;
        return x;
    }

    absx = if sign { -x } else { x };

    /* handle the error of x + g - 0.5 */
    y = absx + GMHALF;
    if absx > GMHALF {
        dy = y - absx;
        dy -= GMHALF;
    } else {
        dy = y - GMHALF;
        dy -= absx;
    }

    z = absx - 0.5;
    r = s(absx) * exp(-y);
    if x < 0.0 {
        /* reflection formula for negative x */
        /* sinpi(absx) is not 0, integers are already handled */
        r = -PI / (sinpi(absx) * absx * r);
        dy = -dy;
        z = -z;
    }
    r += dy * (GMHALF + 0.5) * r / y;
    z = pow(y, 0.5 * z);
    y = r * z * z;
    return y;
}
