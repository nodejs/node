/* origin: FreeBSD /usr/src/lib/msun/src/e_jn.c */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */
/*
 * jn(n, x), yn(n, x)
 * floating point Bessel's function of the 1st and 2nd kind
 * of order n
 *
 * Special cases:
 *      y0(0)=y1(0)=yn(n,0) = -inf with division by zero signal;
 *      y0(-ve)=y1(-ve)=yn(n,-ve) are NaN with invalid signal.
 * Note 2. About jn(n,x), yn(n,x)
 *      For n=0, j0(x) is called,
 *      for n=1, j1(x) is called,
 *      for n<=x, forward recursion is used starting
 *      from values of j0(x) and j1(x).
 *      for n>x, a continued fraction approximation to
 *      j(n,x)/j(n-1,x) is evaluated and then backward
 *      recursion is used starting from a supposed value
 *      for j(n,x). The resulting value of j(0,x) is
 *      compared with the actual value to correct the
 *      supposed value of j(n,x).
 *
 *      yn(n,x) is similar in all respects, except
 *      that forward recursion is used for all
 *      values of n>1.
 */

use super::{cos, fabs, get_high_word, get_low_word, j0, j1, log, sin, sqrt, y0, y1};

const INVSQRTPI: f64 = 5.64189583547756279280e-01; /* 0x3FE20DD7, 0x50429B6D */

/// Integer order of the [Bessel function](https://en.wikipedia.org/wiki/Bessel_function) of the first kind (f64).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn jn(n: i32, mut x: f64) -> f64 {
    let mut ix: u32;
    let lx: u32;
    let nm1: i32;
    let mut i: i32;
    let mut sign: bool;
    let mut a: f64;
    let mut b: f64;
    let mut temp: f64;

    ix = get_high_word(x);
    lx = get_low_word(x);
    sign = (ix >> 31) != 0;
    ix &= 0x7fffffff;

    // -lx == !lx + 1
    if ix | ((lx | (!lx).wrapping_add(1)) >> 31) > 0x7ff00000 {
        /* nan */
        return x;
    }

    /* J(-n,x) = (-1)^n * J(n, x), J(n, -x) = (-1)^n * J(n, x)
     * Thus, J(-n,x) = J(n,-x)
     */
    /* nm1 = |n|-1 is used instead of |n| to handle n==INT_MIN */
    if n == 0 {
        return j0(x);
    }
    if n < 0 {
        nm1 = -(n + 1);
        x = -x;
        sign = !sign;
    } else {
        nm1 = n - 1;
    }
    if nm1 == 0 {
        return j1(x);
    }

    sign &= (n & 1) != 0; /* even n: 0, odd n: signbit(x) */
    x = fabs(x);
    if (ix | lx) == 0 || ix == 0x7ff00000 {
        /* if x is 0 or inf */
        b = 0.0;
    } else if (nm1 as f64) < x {
        /* Safe to use J(n+1,x)=2n/x *J(n,x)-J(n-1,x) */
        if ix >= 0x52d00000 {
            /* x > 2**302 */
            /* (x >> n**2)
             *      Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
             *      Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
             *      Let s=sin(x), c=cos(x),
             *          xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
             *
             *             n    sin(xn)*sqt2    cos(xn)*sqt2
             *          ----------------------------------
             *             0     s-c             c+s
             *             1    -s-c            -c+s
             *             2    -s+c            -c-s
             *             3     s+c             c-s
             */
            temp = match nm1 & 3 {
                0 => -cos(x) + sin(x),
                1 => -cos(x) - sin(x),
                2 => cos(x) - sin(x),
                // 3
                _ => cos(x) + sin(x),
            };
            b = INVSQRTPI * temp / sqrt(x);
        } else {
            a = j0(x);
            b = j1(x);
            i = 0;
            while i < nm1 {
                i += 1;
                temp = b;
                b = b * (2.0 * (i as f64) / x) - a; /* avoid underflow */
                a = temp;
            }
        }
    } else if ix < 0x3e100000 {
        /* x < 2**-29 */
        /* x is tiny, return the first Taylor expansion of J(n,x)
         * J(n,x) = 1/n!*(x/2)^n  - ...
         */
        if nm1 > 32 {
            /* underflow */
            b = 0.0;
        } else {
            temp = x * 0.5;
            b = temp;
            a = 1.0;
            i = 2;
            while i <= nm1 + 1 {
                a *= i as f64; /* a = n! */
                b *= temp; /* b = (x/2)^n */
                i += 1;
            }
            b = b / a;
        }
    } else {
        /* use backward recurrence */
        /*                      x      x^2      x^2
         *  J(n,x)/J(n-1,x) =  ----   ------   ------   .....
         *                      2n  - 2(n+1) - 2(n+2)
         *
         *                      1      1        1
         *  (for large x)   =  ----  ------   ------   .....
         *                      2n   2(n+1)   2(n+2)
         *                      -- - ------ - ------ -
         *                       x     x         x
         *
         * Let w = 2n/x and h=2/x, then the above quotient
         * is equal to the continued fraction:
         *                  1
         *      = -----------------------
         *                     1
         *         w - -----------------
         *                        1
         *              w+h - ---------
         *                     w+2h - ...
         *
         * To determine how many terms needed, let
         * Q(0) = w, Q(1) = w(w+h) - 1,
         * Q(k) = (w+k*h)*Q(k-1) - Q(k-2),
         * When Q(k) > 1e4      good for single
         * When Q(k) > 1e9      good for double
         * When Q(k) > 1e17     good for quadruple
         */
        /* determine k */
        let mut t: f64;
        let mut q0: f64;
        let mut q1: f64;
        let mut w: f64;
        let h: f64;
        let mut z: f64;
        let mut tmp: f64;
        let nf: f64;

        let mut k: i32;

        nf = (nm1 as f64) + 1.0;
        w = 2.0 * nf / x;
        h = 2.0 / x;
        z = w + h;
        q0 = w;
        q1 = w * z - 1.0;
        k = 1;
        while q1 < 1.0e9 {
            k += 1;
            z += h;
            tmp = z * q1 - q0;
            q0 = q1;
            q1 = tmp;
        }
        t = 0.0;
        i = k;
        while i >= 0 {
            t = 1.0 / (2.0 * ((i as f64) + nf) / x - t);
            i -= 1;
        }
        a = t;
        b = 1.0;
        /*  estimate log((2/x)^n*n!) = n*log(2/x)+n*ln(n)
         *  Hence, if n*(log(2n/x)) > ...
         *  single 8.8722839355e+01
         *  double 7.09782712893383973096e+02
         *  long double 1.1356523406294143949491931077970765006170e+04
         *  then recurrent value may overflow and the result is
         *  likely underflow to zero
         */
        tmp = nf * log(fabs(w));
        if tmp < 7.09782712893383973096e+02 {
            i = nm1;
            while i > 0 {
                temp = b;
                b = b * (2.0 * (i as f64)) / x - a;
                a = temp;
                i -= 1;
            }
        } else {
            i = nm1;
            while i > 0 {
                temp = b;
                b = b * (2.0 * (i as f64)) / x - a;
                a = temp;
                /* scale b to avoid spurious overflow */
                let x1p500 = f64::from_bits(0x5f30000000000000); // 0x1p500 == 2^500
                if b > x1p500 {
                    a /= b;
                    t /= b;
                    b = 1.0;
                }
                i -= 1;
            }
        }
        z = j0(x);
        w = j1(x);
        if fabs(z) >= fabs(w) {
            b = t * z / b;
        } else {
            b = t * w / a;
        }
    }

    if sign { -b } else { b }
}

/// Integer order of the [Bessel function](https://en.wikipedia.org/wiki/Bessel_function) of the second kind (f64).
#[cfg_attr(assert_no_panic, no_panic::no_panic)]
pub fn yn(n: i32, x: f64) -> f64 {
    let mut ix: u32;
    let lx: u32;
    let mut ib: u32;
    let nm1: i32;
    let mut sign: bool;
    let mut i: i32;
    let mut a: f64;
    let mut b: f64;
    let mut temp: f64;

    ix = get_high_word(x);
    lx = get_low_word(x);
    sign = (ix >> 31) != 0;
    ix &= 0x7fffffff;

    // -lx == !lx + 1
    if ix | ((lx | (!lx).wrapping_add(1)) >> 31) > 0x7ff00000 {
        /* nan */
        return x;
    }
    if sign && (ix | lx) != 0 {
        /* x < 0 */
        return 0.0 / 0.0;
    }
    if ix == 0x7ff00000 {
        return 0.0;
    }

    if n == 0 {
        return y0(x);
    }
    if n < 0 {
        nm1 = -(n + 1);
        sign = (n & 1) != 0;
    } else {
        nm1 = n - 1;
        sign = false;
    }
    if nm1 == 0 {
        if sign {
            return -y1(x);
        } else {
            return y1(x);
        }
    }

    if ix >= 0x52d00000 {
        /* x > 2**302 */
        /* (x >> n**2)
         *      Jn(x) = cos(x-(2n+1)*pi/4)*sqrt(2/x*pi)
         *      Yn(x) = sin(x-(2n+1)*pi/4)*sqrt(2/x*pi)
         *      Let s=sin(x), c=cos(x),
         *          xn=x-(2n+1)*pi/4, sqt2 = sqrt(2),then
         *
         *             n    sin(xn)*sqt2    cos(xn)*sqt2
         *          ----------------------------------
         *             0     s-c             c+s
         *             1    -s-c            -c+s
         *             2    -s+c            -c-s
         *             3     s+c             c-s
         */
        temp = match nm1 & 3 {
            0 => -sin(x) - cos(x),
            1 => -sin(x) + cos(x),
            2 => sin(x) + cos(x),
            // 3
            _ => sin(x) - cos(x),
        };
        b = INVSQRTPI * temp / sqrt(x);
    } else {
        a = y0(x);
        b = y1(x);
        /* quit if b is -inf */
        ib = get_high_word(b);
        i = 0;
        while i < nm1 && ib != 0xfff00000 {
            i += 1;
            temp = b;
            b = (2.0 * (i as f64) / x) * b - a;
            ib = get_high_word(b);
            a = temp;
        }
    }

    if sign { -b } else { b }
}
