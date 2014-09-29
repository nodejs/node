// The following is adapted from fdlibm (http://www.netlib.org/fdlibm),
//
// ====================================================
// Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
//
// Developed at SunSoft, a Sun Microsystems, Inc. business.
// Permission to use, copy, modify, and distribute this
// software is freely granted, provided that this notice
// is preserved.
// ====================================================
//
// The original source code covered by the above license above has been
// modified significantly by Google Inc.
// Copyright 2014 the V8 project authors. All rights reserved.
//
// The following is a straightforward translation of fdlibm routines
// by Raymond Toy (rtoy@google.com).


var kMath;  // Initialized to a Float64Array during genesis and is not writable.

const INVPIO2 = kMath[0];
const PIO2_1  = kMath[1];
const PIO2_1T = kMath[2];
const PIO2_2  = kMath[3];
const PIO2_2T = kMath[4];
const PIO2_3  = kMath[5];
const PIO2_3T = kMath[6];
const PIO4    = kMath[32];
const PIO4LO  = kMath[33];

// Compute k and r such that x - k*pi/2 = r where |r| < pi/4. For
// precision, r is returned as two values y0 and y1 such that r = y0 + y1
// to more than double precision.
macro REMPIO2(X)
  var n, y0, y1;
  var hx = %_DoubleHi(X);
  var ix = hx & 0x7fffffff;

  if (ix < 0x4002d97c) {
    // |X| ~< 3*pi/4, special case with n = +/- 1
    if (hx > 0) {
      var z = X - PIO2_1;
      if (ix != 0x3ff921fb) {
        // 33+53 bit pi is good enough
        y0 = z - PIO2_1T;
        y1 = (z - y0) - PIO2_1T;
      } else {
        // near pi/2, use 33+33+53 bit pi
        z -= PIO2_2;
        y0 = z - PIO2_2T;
        y1 = (z - y0) - PIO2_2T;
      }
      n = 1;
    } else {
      // Negative X
      var z = X + PIO2_1;
      if (ix != 0x3ff921fb) {
        // 33+53 bit pi is good enough
        y0 = z + PIO2_1T;
        y1 = (z - y0) + PIO2_1T;
      } else {
        // near pi/2, use 33+33+53 bit pi
        z += PIO2_2;
        y0 = z + PIO2_2T;
        y1 = (z - y0) + PIO2_2T;
      }
      n = -1;
    }
  } else if (ix <= 0x413921fb) {
    // |X| ~<= 2^19*(pi/2), medium size
    var t = MathAbs(X);
    n = (t * INVPIO2 + 0.5) | 0;
    var r = t - n * PIO2_1;
    var w = n * PIO2_1T;
    // First round good to 85 bit
    y0 = r - w;
    if (ix - (%_DoubleHi(y0) & 0x7ff00000) > 0x1000000) {
      // 2nd iteration needed, good to 118
      t = r;
      w = n * PIO2_2;
      r = t - w;
      w = n * PIO2_2T - ((t - r) - w);
      y0 = r - w;
      if (ix - (%_DoubleHi(y0) & 0x7ff00000) > 0x3100000) {
        // 3rd iteration needed. 151 bits accuracy
        t = r;
        w = n * PIO2_3;
        r = t - w;
        w = n * PIO2_3T - ((t - r) - w);
        y0 = r - w;
      }
    }
    y1 = (r - y0) - w;
    if (hx < 0) {
      n = -n;
      y0 = -y0;
      y1 = -y1;
    }
  } else {
    // Need to do full Payne-Hanek reduction here.
    var r = %RemPiO2(X);
    n = r[0];
    y0 = r[1];
    y1 = r[2];
  }
endmacro


// __kernel_sin(X, Y, IY)
// kernel sin function on [-pi/4, pi/4], pi/4 ~ 0.7854
// Input X is assumed to be bounded by ~pi/4 in magnitude.
// Input Y is the tail of X so that x = X + Y.
//
// Algorithm
//  1. Since ieee_sin(-x) = -ieee_sin(x), we need only to consider positive x.
//  2. ieee_sin(x) is approximated by a polynomial of degree 13 on
//     [0,pi/4]
//                           3            13
//          sin(x) ~ x + S1*x + ... + S6*x
//     where
//
//    |ieee_sin(x)    2     4     6     8     10     12  |     -58
//    |----- - (1+S1*x +S2*x +S3*x +S4*x +S5*x  +S6*x   )| <= 2
//    |  x                                               |
//
//  3. ieee_sin(X+Y) = ieee_sin(X) + sin'(X')*Y
//              ~ ieee_sin(X) + (1-X*X/2)*Y
//     For better accuracy, let
//               3      2      2      2      2
//          r = X *(S2+X *(S3+X *(S4+X *(S5+X *S6))))
//     then                   3    2
//          sin(x) = X + (S1*X + (X *(r-Y/2)+Y))
//
macro KSIN(x)
kMath[7+x]
endmacro

macro RETURN_KERNELSIN(X, Y, SIGN)
  var z = X * X;
  var v = z * X;
  var r = KSIN(1) + z * (KSIN(2) + z * (KSIN(3) +
                    z * (KSIN(4) + z * KSIN(5))));
  return (X - ((z * (0.5 * Y - v * r) - Y) - v * KSIN(0))) SIGN;
endmacro

// __kernel_cos(X, Y)
// kernel cos function on [-pi/4, pi/4], pi/4 ~ 0.785398164
// Input X is assumed to be bounded by ~pi/4 in magnitude.
// Input Y is the tail of X so that x = X + Y.
//
// Algorithm
//  1. Since ieee_cos(-x) = ieee_cos(x), we need only to consider positive x.
//  2. ieee_cos(x) is approximated by a polynomial of degree 14 on
//     [0,pi/4]
//                                   4            14
//          cos(x) ~ 1 - x*x/2 + C1*x + ... + C6*x
//     where the remez error is
//
//  |                   2     4     6     8     10    12     14 |     -58
//  |ieee_cos(x)-(1-.5*x +C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  )| <= 2
//  |                                                           |
//
//                 4     6     8     10    12     14
//  3. let r = C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  , then
//         ieee_cos(x) = 1 - x*x/2 + r
//     since ieee_cos(X+Y) ~ ieee_cos(X) - ieee_sin(X)*Y
//                    ~ ieee_cos(X) - X*Y,
//     a correction term is necessary in ieee_cos(x) and hence
//         cos(X+Y) = 1 - (X*X/2 - (r - X*Y))
//     For better accuracy when x > 0.3, let qx = |x|/4 with
//     the last 32 bits mask off, and if x > 0.78125, let qx = 0.28125.
//     Then
//         cos(X+Y) = (1-qx) - ((X*X/2-qx) - (r-X*Y)).
//     Note that 1-qx and (X*X/2-qx) is EXACT here, and the
//     magnitude of the latter is at least a quarter of X*X/2,
//     thus, reducing the rounding error in the subtraction.
//
macro KCOS(x)
kMath[13+x]
endmacro

macro RETURN_KERNELCOS(X, Y, SIGN)
  var ix = %_DoubleHi(X) & 0x7fffffff;
  var z = X * X;
  var r = z * (KCOS(0) + z * (KCOS(1) + z * (KCOS(2)+
          z * (KCOS(3) + z * (KCOS(4) + z * KCOS(5))))));
  if (ix < 0x3fd33333) {  // |x| ~< 0.3
    return (1 - (0.5 * z - (z * r - X * Y))) SIGN;
  } else {
    var qx;
    if (ix > 0x3fe90000) {  // |x| > 0.78125
      qx = 0.28125;
    } else {
      qx = %_ConstructDouble(%_DoubleHi(0.25 * X), 0);
    }
    var hz = 0.5 * z - qx;
    return (1 - qx - (hz - (z * r - X * Y))) SIGN;
  }
endmacro


// kernel tan function on [-pi/4, pi/4], pi/4 ~ 0.7854
// Input x is assumed to be bounded by ~pi/4 in magnitude.
// Input y is the tail of x.
// Input k indicates whether ieee_tan (if k = 1) or -1/tan (if k = -1)
// is returned.
//
// Algorithm
//  1. Since ieee_tan(-x) = -ieee_tan(x), we need only to consider positive x.
//  2. if x < 2^-28 (hx<0x3e300000 0), return x with inexact if x!=0.
//  3. ieee_tan(x) is approximated by a odd polynomial of degree 27 on
//     [0,0.67434]
//                           3             27
//          tan(x) ~ x + T1*x + ... + T13*x
//     where
//
//     |ieee_tan(x)    2     4            26   |     -59.2
//     |----- - (1+T1*x +T2*x +.... +T13*x    )| <= 2
//     |  x                                    |
//
//     Note: ieee_tan(x+y) = ieee_tan(x) + tan'(x)*y
//                    ~ ieee_tan(x) + (1+x*x)*y
//     Therefore, for better accuracy in computing ieee_tan(x+y), let
//               3      2      2       2       2
//          r = x *(T2+x *(T3+x *(...+x *(T12+x *T13))))
//     then
//                              3    2
//          tan(x+y) = x + (T1*x + (x *(r+y)+y))
//
//  4. For x in [0.67434,pi/4],  let y = pi/4 - x, then
//          tan(x) = ieee_tan(pi/4-y) = (1-ieee_tan(y))/(1+ieee_tan(y))
//                 = 1 - 2*(ieee_tan(y) - (ieee_tan(y)^2)/(1+ieee_tan(y)))
//
// Set returnTan to 1 for tan; -1 for cot.  Anything else is illegal
// and will cause incorrect results.
//
macro KTAN(x)
kMath[19+x]
endmacro

function KernelTan(x, y, returnTan) {
  var z;
  var w;
  var hx = %_DoubleHi(x);
  var ix = hx & 0x7fffffff;

  if (ix < 0x3e300000) {  // |x| < 2^-28
    if (((ix | %_DoubleLo(x)) | (returnTan + 1)) == 0) {
      // x == 0 && returnTan = -1
      return 1 / MathAbs(x);
    } else {
      if (returnTan == 1) {
        return x;
      } else {
        // Compute -1/(x + y) carefully
        var w = x + y;
        var z = %_ConstructDouble(%_DoubleHi(w), 0);
        var v = y - (z - x);
        var a = -1 / w;
        var t = %_ConstructDouble(%_DoubleHi(a), 0);
        var s = 1 + t * z;
        return t + a * (s + t * v);
      }
    }
  }
  if (ix >= 0x3fe59429) {  // |x| > .6744
    if (x < 0) {
      x = -x;
      y = -y;
    }
    z = PIO4 - x;
    w = PIO4LO - y;
    x = z + w;
    y = 0;
  }
  z = x * x;
  w = z * z;

  // Break x^5 * (T1 + x^2*T2 + ...) into
  // x^5 * (T1 + x^4*T3 + ... + x^20*T11) +
  // x^5 * (x^2 * (T2 + x^4*T4 + ... + x^22*T12))
  var r = KTAN(1) + w * (KTAN(3) + w * (KTAN(5) +
                    w * (KTAN(7) + w * (KTAN(9) + w * KTAN(11)))));
  var v = z * (KTAN(2) + w * (KTAN(4) + w * (KTAN(6) +
                         w * (KTAN(8) + w * (KTAN(10) + w * KTAN(12))))));
  var s = z * x;
  r = y + z * (s * (r + v) + y);
  r = r + KTAN(0) * s;
  w = x + r;
  if (ix >= 0x3fe59428) {
    return (1 - ((hx >> 30) & 2)) *
      (returnTan - 2.0 * (x - (w * w / (w + returnTan) - r)));
  }
  if (returnTan == 1) {
    return w;
  } else {
    z = %_ConstructDouble(%_DoubleHi(w), 0);
    v = r - (z - x);
    var a = -1 / w;
    var t = %_ConstructDouble(%_DoubleHi(a), 0);
    s = 1 + t * z;
    return t + a * (s + t * v);
  }
}

function MathSinSlow(x) {
  REMPIO2(x);
  var sign = 1 - (n & 2);
  if (n & 1) {
    RETURN_KERNELCOS(y0, y1, * sign);
  } else {
    RETURN_KERNELSIN(y0, y1, * sign);
  }
}

function MathCosSlow(x) {
  REMPIO2(x);
  if (n & 1) {
    var sign = (n & 2) - 1;
    RETURN_KERNELSIN(y0, y1, * sign);
  } else {
    var sign = 1 - (n & 2);
    RETURN_KERNELCOS(y0, y1, * sign);
  }
}

// ECMA 262 - 15.8.2.16
function MathSin(x) {
  x = x * 1;  // Convert to number.
  if ((%_DoubleHi(x) & 0x7fffffff) <= 0x3fe921fb) {
    // |x| < pi/4, approximately.  No reduction needed.
    RETURN_KERNELSIN(x, 0, /* empty */);
  }
  return MathSinSlow(x);
}

// ECMA 262 - 15.8.2.7
function MathCos(x) {
  x = x * 1;  // Convert to number.
  if ((%_DoubleHi(x) & 0x7fffffff) <= 0x3fe921fb) {
    // |x| < pi/4, approximately.  No reduction needed.
    RETURN_KERNELCOS(x, 0, /* empty */);
  }
  return MathCosSlow(x);
}

// ECMA 262 - 15.8.2.18
function MathTan(x) {
  x = x * 1;  // Convert to number.
  if ((%_DoubleHi(x) & 0x7fffffff) <= 0x3fe921fb) {
    // |x| < pi/4, approximately.  No reduction needed.
    return KernelTan(x, 0, 1);
  }
  REMPIO2(x);
  return KernelTan(y0, y1, (n & 1) ? -1 : 1);
}

// ES6 draft 09-27-13, section 20.2.2.20.
// Math.log1p
//
// Method :                  
//   1. Argument Reduction: find k and f such that 
//                      1+x = 2^k * (1+f), 
//         where  sqrt(2)/2 < 1+f < sqrt(2) .
//
//      Note. If k=0, then f=x is exact. However, if k!=0, then f
//      may not be representable exactly. In that case, a correction
//      term is need. Let u=1+x rounded. Let c = (1+x)-u, then
//      log(1+x) - log(u) ~ c/u. Thus, we proceed to compute log(u),
//      and add back the correction term c/u.
//      (Note: when x > 2**53, one can simply return log(x))
//
//   2. Approximation of log1p(f).
//      Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
//            = 2s + 2/3 s**3 + 2/5 s**5 + .....,
//            = 2s + s*R
//      We use a special Reme algorithm on [0,0.1716] to generate 
//      a polynomial of degree 14 to approximate R The maximum error 
//      of this polynomial approximation is bounded by 2**-58.45. In
//      other words,
//                      2      4      6      8      10      12      14
//          R(z) ~ Lp1*s +Lp2*s +Lp3*s +Lp4*s +Lp5*s  +Lp6*s  +Lp7*s
//      (the values of Lp1 to Lp7 are listed in the program)
//      and
//          |      2          14          |     -58.45
//          | Lp1*s +...+Lp7*s    -  R(z) | <= 2 
//          |                             |
//      Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
//      In order to guarantee error in log below 1ulp, we compute log
//      by
//              log1p(f) = f - (hfsq - s*(hfsq+R)).
//
//      3. Finally, log1p(x) = k*ln2 + log1p(f).  
//                           = k*ln2_hi+(f-(hfsq-(s*(hfsq+R)+k*ln2_lo)))
//         Here ln2 is split into two floating point number: 
//                      ln2_hi + ln2_lo,
//         where n*ln2_hi is always exact for |n| < 2000.
//
// Special cases:
//      log1p(x) is NaN with signal if x < -1 (including -INF) ; 
//      log1p(+INF) is +INF; log1p(-1) is -INF with signal;
//      log1p(NaN) is that NaN with no signal.
//
// Accuracy:
//      according to an error analysis, the error is always less than
//      1 ulp (unit in the last place).
//
// Constants:
// The hexadecimal values are the intended ones for the following 
// constants. The decimal values may be used, provided that the 
// compiler will convert from decimal to binary accurately enough 
// to produce the hexadecimal values shown.
//
// Note: Assuming log() return accurate answer, the following
//       algorithm can be used to compute log1p(x) to within a few ULP:
//
//              u = 1+x;
//              if (u==1.0) return x ; else
//                          return log(u)*(x/(u-1.0));
//
//       See HP-15C Advanced Functions Handbook, p.193.
//
const LN2_HI    = kMath[34];
const LN2_LO    = kMath[35];
const TWO54     = kMath[36];
const TWO_THIRD = kMath[37];
macro KLOGP1(x)
(kMath[38+x])
endmacro

function MathLog1p(x) {
  x = x * 1;  // Convert to number.
  var hx = %_DoubleHi(x);
  var ax = hx & 0x7fffffff;
  var k = 1;
  var f = x;
  var hu = 1;
  var c = 0;
  var u = x;

  if (hx < 0x3fda827a) {
    // x < 0.41422
    if (ax >= 0x3ff00000) {  // |x| >= 1
      if (x === -1) {
        return -INFINITY;  // log1p(-1) = -inf
      } else {
        return NAN;  // log1p(x<-1) = NaN
      }
    } else if (ax < 0x3c900000)  {
      // For |x| < 2^-54 we can return x.
      return x;
    } else if (ax < 0x3e200000) {
      // For |x| < 2^-29 we can use a simple two-term Taylor series.
      return x - x * x * 0.5;
    }

    if ((hx > 0) || (hx <= -0x402D413D)) {  // (int) 0xbfd2bec3 = -0x402d413d
      // -.2929 < x < 0.41422
      k = 0;
    }
  }

  // Handle Infinity and NAN
  if (hx >= 0x7ff00000) return x;

  if (k !== 0) {
    if (hx < 0x43400000) {
      // x < 2^53
      u = 1 + x;
      hu = %_DoubleHi(u);
      k = (hu >> 20) - 1023;
      c = (k > 0) ? 1 - (u - x) : x - (u - 1);
      c = c / u;
    } else {
      hu = %_DoubleHi(u);
      k = (hu >> 20) - 1023;
    }
    hu = hu & 0xfffff;
    if (hu < 0x6a09e) {
      u = %_ConstructDouble(hu | 0x3ff00000, %_DoubleLo(u));  // Normalize u.
    } else {
      ++k;
      u = %_ConstructDouble(hu | 0x3fe00000, %_DoubleLo(u));  // Normalize u/2.
      hu = (0x00100000 - hu) >> 2;
    }
    f = u - 1;
  }

  var hfsq = 0.5 * f * f;
  if (hu === 0) {
    // |f| < 2^-20;
    if (f === 0) {
      if (k === 0) {
        return 0.0;
      } else {
        return k * LN2_HI + (c + k * LN2_LO);
      }
    }
    var R = hfsq * (1 - TWO_THIRD * f);
    if (k === 0) {
      return f - R;
    } else {
      return k * LN2_HI - ((R - (k * LN2_LO + c)) - f);
    }
  }

  var s = f / (2 + f); 
  var z = s * s;
  var R = z * (KLOGP1(0) + z * (KLOGP1(1) + z *
              (KLOGP1(2) + z * (KLOGP1(3) + z *
              (KLOGP1(4) + z * (KLOGP1(5) + z * KLOGP1(6)))))));
  if (k === 0) {
    return f - (hfsq - s * (hfsq + R));
  } else {
    return k * LN2_HI - ((hfsq - (s * (hfsq + R) + (k * LN2_LO + c))) - f);
  }
}
