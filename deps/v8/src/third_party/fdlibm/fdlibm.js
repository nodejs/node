// The following is adapted from fdlibm (http://www.netlib.org/fdlibm),
//
// ====================================================
// Copyright (C) 1993-2004 by Sun Microsystems, Inc. All rights reserved.
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

// rempio2result is used as a container for return values of %RemPiO2. It is
// initialized to a two-element Float64Array during genesis.

(function(global, utils) {
  
"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalFloat64Array = global.Float64Array;
var GlobalMath = global.Math;
var MathAbs;
var MathExp;
var NaN = %GetRootNaN();
var rempio2result;

utils.Import(function(from) {
  MathAbs = from.MathAbs;
  MathExp = from.MathExp;
});

utils.CreateDoubleResultArray = function(global) {
  rempio2result = new GlobalFloat64Array(2);
};

// -------------------------------------------------------------------

define INVPIO2 = 6.36619772367581382433e-01;
define PIO2_1  = 1.57079632673412561417;
define PIO2_1T = 6.07710050650619224932e-11;
define PIO2_2  = 6.07710050630396597660e-11;
define PIO2_2T = 2.02226624879595063154e-21;
define PIO2_3  = 2.02226624871116645580e-21;
define PIO2_3T = 8.47842766036889956997e-32;
define PIO4    = 7.85398163397448278999e-01;
define PIO4LO  = 3.06161699786838301793e-17;

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
    n = %RemPiO2(X, rempio2result);
    y0 = rempio2result[0];
    y1 = rempio2result[1];
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
define S1 = -1.66666666666666324348e-01;
define S2 = 8.33333333332248946124e-03;
define S3 = -1.98412698298579493134e-04;
define S4 = 2.75573137070700676789e-06;
define S5 = -2.50507602534068634195e-08;
define S6 = 1.58969099521155010221e-10;

macro RETURN_KERNELSIN(X, Y, SIGN)
  var z = X * X;
  var v = z * X;
  var r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
  return (X - ((z * (0.5 * Y - v * r) - Y) - v * S1)) SIGN;
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
define C1 = 4.16666666666666019037e-02;
define C2 = -1.38888888888741095749e-03;
define C3 = 2.48015872894767294178e-05;
define C4 = -2.75573143513906633035e-07;
define C5 = 2.08757232129817482790e-09;
define C6 = -1.13596475577881948265e-11;

macro RETURN_KERNELCOS(X, Y, SIGN)
  var ix = %_DoubleHi(X) & 0x7fffffff;
  var z = X * X;
  var r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
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
define T00 = 3.33333333333334091986e-01;
define T01 = 1.33333333333201242699e-01;
define T02 = 5.39682539762260521377e-02;
define T03 = 2.18694882948595424599e-02;
define T04 = 8.86323982359930005737e-03;
define T05 = 3.59207910759131235356e-03;
define T06 = 1.45620945432529025516e-03;
define T07 = 5.88041240820264096874e-04;
define T08 = 2.46463134818469906812e-04;
define T09 = 7.81794442939557092300e-05;
define T10 = 7.14072491382608190305e-05;
define T11 = -1.85586374855275456654e-05;
define T12 = 2.59073051863633712884e-05;

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
  if (ix >= 0x3fe59428) {  // |x| > .6744
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
  var r = T01 + w * (T03 + w * (T05 +
                w * (T07 + w * (T09 + w * T11))));
  var v = z * (T02 + w * (T04 + w * (T06 +
                     w * (T08 + w * (T10 + w * T12)))));
  var s = z * x;
  r = y + z * (s * (r + v) + y);
  r = r + T00 * s;
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
  x = +x;  // Convert to number.
  if ((%_DoubleHi(x) & 0x7fffffff) <= 0x3fe921fb) {
    // |x| < pi/4, approximately.  No reduction needed.
    RETURN_KERNELSIN(x, 0, /* empty */);
  }
  return +MathSinSlow(x);
}

// ECMA 262 - 15.8.2.7
function MathCos(x) {
  x = +x;  // Convert to number.
  if ((%_DoubleHi(x) & 0x7fffffff) <= 0x3fe921fb) {
    // |x| < pi/4, approximately.  No reduction needed.
    RETURN_KERNELCOS(x, 0, /* empty */);
  }
  return +MathCosSlow(x);
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
//      Constants are found in fdlibm.cc. We assume the C++ compiler to convert
//      from decimal to binary accurately enough to produce the intended values.
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
define LN2_HI    = 6.93147180369123816490e-01;
define LN2_LO    = 1.90821492927058770002e-10;
define TWO_THIRD = 6.666666666666666666e-01;
define LP1 = 6.666666666666735130e-01;
define LP2 = 3.999999999940941908e-01;
define LP3 = 2.857142874366239149e-01;
define LP4 = 2.222219843214978396e-01;
define LP5 = 1.818357216161805012e-01;
define LP6 = 1.531383769920937332e-01;
define LP7 = 1.479819860511658591e-01;

// 2^54
define TWO54 = 18014398509481984;

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
        return NaN;  // log1p(x<-1) = NaN
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

  // Handle Infinity and NaN
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
  var R = z * (LP1 + z * (LP2 + z * (LP3 + z * (LP4 +
          z * (LP5 + z * (LP6 + z * LP7))))));
  if (k === 0) {
    return f - (hfsq - s * (hfsq + R));
  } else {
    return k * LN2_HI - ((hfsq - (s * (hfsq + R) + (k * LN2_LO + c))) - f);
  }
}

// ES6 draft 09-27-13, section 20.2.2.14.
// Math.expm1
// Returns exp(x)-1, the exponential of x minus 1.
//
// Method
//   1. Argument reduction:
//      Given x, find r and integer k such that
//
//               x = k*ln2 + r,  |r| <= 0.5*ln2 ~ 0.34658  
//
//      Here a correction term c will be computed to compensate 
//      the error in r when rounded to a floating-point number.
//
//   2. Approximating expm1(r) by a special rational function on
//      the interval [0,0.34658]:
//      Since
//          r*(exp(r)+1)/(exp(r)-1) = 2+ r^2/6 - r^4/360 + ...
//      we define R1(r*r) by
//          r*(exp(r)+1)/(exp(r)-1) = 2+ r^2/6 * R1(r*r)
//      That is,
//          R1(r**2) = 6/r *((exp(r)+1)/(exp(r)-1) - 2/r)
//                   = 6/r * ( 1 + 2.0*(1/(exp(r)-1) - 1/r))
//                   = 1 - r^2/60 + r^4/2520 - r^6/100800 + ...
//      We use a special Remes algorithm on [0,0.347] to generate 
//      a polynomial of degree 5 in r*r to approximate R1. The 
//      maximum error of this polynomial approximation is bounded 
//      by 2**-61. In other words,
//          R1(z) ~ 1.0 + Q1*z + Q2*z**2 + Q3*z**3 + Q4*z**4 + Q5*z**5
//      where   Q1  =  -1.6666666666666567384E-2,
//              Q2  =   3.9682539681370365873E-4,
//              Q3  =  -9.9206344733435987357E-6,
//              Q4  =   2.5051361420808517002E-7,
//              Q5  =  -6.2843505682382617102E-9;
//      (where z=r*r, and the values of Q1 to Q5 are listed below)
//      with error bounded by
//          |                  5           |     -61
//          | 1.0+Q1*z+...+Q5*z   -  R1(z) | <= 2 
//          |                              |
//
//      expm1(r) = exp(r)-1 is then computed by the following 
//      specific way which minimize the accumulation rounding error: 
//                             2     3
//                            r     r    [ 3 - (R1 + R1*r/2)  ]
//            expm1(r) = r + --- + --- * [--------------------]
//                            2     2    [ 6 - r*(3 - R1*r/2) ]
//
//      To compensate the error in the argument reduction, we use
//              expm1(r+c) = expm1(r) + c + expm1(r)*c 
//                         ~ expm1(r) + c + r*c 
//      Thus c+r*c will be added in as the correction terms for
//      expm1(r+c). Now rearrange the term to avoid optimization 
//      screw up:
//                      (      2                                    2 )
//                      ({  ( r    [ R1 -  (3 - R1*r/2) ]  )  }    r  )
//       expm1(r+c)~r - ({r*(--- * [--------------------]-c)-c} - --- )
//                      ({  ( 2    [ 6 - r*(3 - R1*r/2) ]  )  }    2  )
//                      (                                             )
//
//                 = r - E
//   3. Scale back to obtain expm1(x):
//      From step 1, we have
//         expm1(x) = either 2^k*[expm1(r)+1] - 1
//                  = or     2^k*[expm1(r) + (1-2^-k)]
//   4. Implementation notes:
//      (A). To save one multiplication, we scale the coefficient Qi
//           to Qi*2^i, and replace z by (x^2)/2.
//      (B). To achieve maximum accuracy, we compute expm1(x) by
//        (i)   if x < -56*ln2, return -1.0, (raise inexact if x!=inf)
//        (ii)  if k=0, return r-E
//        (iii) if k=-1, return 0.5*(r-E)-0.5
//        (iv)  if k=1 if r < -0.25, return 2*((r+0.5)- E)
//                     else          return  1.0+2.0*(r-E);
//        (v)   if (k<-2||k>56) return 2^k(1-(E-r)) - 1 (or exp(x)-1)
//        (vi)  if k <= 20, return 2^k((1-2^-k)-(E-r)), else
//        (vii) return 2^k(1-((E+2^-k)-r)) 
//
// Special cases:
//      expm1(INF) is INF, expm1(NaN) is NaN;
//      expm1(-INF) is -1, and
//      for finite argument, only expm1(0)=0 is exact.
//
// Accuracy:
//      according to an error analysis, the error is always less than
//      1 ulp (unit in the last place).
//
// Misc. info.
//      For IEEE double 
//          if x > 7.09782712893383973096e+02 then expm1(x) overflow
//
define KEXPM1_OVERFLOW = 7.09782712893383973096e+02;
define INVLN2          = 1.44269504088896338700;
define EXPM1_1 = -3.33333333333331316428e-02;
define EXPM1_2 = 1.58730158725481460165e-03;
define EXPM1_3 = -7.93650757867487942473e-05;
define EXPM1_4 = 4.00821782732936239552e-06;
define EXPM1_5 = -2.01099218183624371326e-07;

function MathExpm1(x) {
  x = x * 1;  // Convert to number.
  var y;
  var hi;
  var lo;
  var k;
  var t;
  var c;
    
  var hx = %_DoubleHi(x);
  var xsb = hx & 0x80000000;     // Sign bit of x
  var y = (xsb === 0) ? x : -x;  // y = |x|
  hx &= 0x7fffffff;              // High word of |x|

  // Filter out huge and non-finite argument
  if (hx >= 0x4043687a) {     // if |x| ~=> 56 * ln2
    if (hx >= 0x40862e42) {   // if |x| >= 709.78
      if (hx >= 0x7ff00000) {
        // expm1(inf) = inf; expm1(-inf) = -1; expm1(nan) = nan;
        return (x === -INFINITY) ? -1 : x;
      }
      if (x > KEXPM1_OVERFLOW) return INFINITY;  // Overflow
    }
    if (xsb != 0) return -1;  // x < -56 * ln2, return -1.
  }

  // Argument reduction
  if (hx > 0x3fd62e42) {    // if |x| > 0.5 * ln2
    if (hx < 0x3ff0a2b2) {  // and |x| < 1.5 * ln2
      if (xsb === 0) {
        hi = x - LN2_HI;
        lo = LN2_LO;
        k = 1;
      } else {
        hi = x + LN2_HI;
        lo = -LN2_LO;
        k = -1;
      }
    } else {
      k = (INVLN2 * x + ((xsb === 0) ? 0.5 : -0.5)) | 0;
      t = k;
      // t * ln2_hi is exact here.
      hi = x - t * LN2_HI;
      lo = t * LN2_LO;
    }
    x = hi - lo;
    c = (hi - x) - lo;
  } else if (hx < 0x3c900000)	{
    // When |x| < 2^-54, we can return x.
    return x;
  } else {
    // Fall through.
    k = 0;
  }

  // x is now in primary range
  var hfx = 0.5 * x;
  var hxs = x * hfx;
  var r1 = 1 + hxs * (EXPM1_1 + hxs * (EXPM1_2 + hxs *
                     (EXPM1_3 + hxs * (EXPM1_4 + hxs * EXPM1_5))));
  t = 3 - r1 * hfx;
  var e = hxs * ((r1 - t) / (6 - x * t));
  if (k === 0) {  // c is 0
    return x - (x*e - hxs);
  } else {
    e = (x * (e - c) - c);
    e -= hxs;
    if (k === -1) return 0.5 * (x - e) - 0.5;
    if (k === 1) {
      if (x < -0.25) return -2 * (e - (x + 0.5));
      return 1 + 2 * (x - e);
    }

    if (k <= -2 || k > 56) {
      // suffice to return exp(x) + 1
      y = 1 - (e - x);
      // Add k to y's exponent
      y = %_ConstructDouble(%_DoubleHi(y) + (k << 20), %_DoubleLo(y));
      return y - 1;
    }
    if (k < 20) {
      // t = 1 - 2^k
      t = %_ConstructDouble(0x3ff00000 - (0x200000 >> k), 0);
      y = t - (e - x);
      // Add k to y's exponent
      y = %_ConstructDouble(%_DoubleHi(y) + (k << 20), %_DoubleLo(y));
    } else {
      // t = 2^-k
      t = %_ConstructDouble((0x3ff - k) << 20, 0);
      y = x - (e + t);
      y += 1;
      // Add k to y's exponent
      y = %_ConstructDouble(%_DoubleHi(y) + (k << 20), %_DoubleLo(y));
    }
  }
  return y;
}


// ES6 draft 09-27-13, section 20.2.2.30.
// Math.sinh
// Method :
// mathematically sinh(x) if defined to be (exp(x)-exp(-x))/2
//      1. Replace x by |x| (sinh(-x) = -sinh(x)).
//      2.
//                                                  E + E/(E+1)
//          0        <= x <= 22     :  sinh(x) := --------------, E=expm1(x)
//                                                      2
//
//          22       <= x <= lnovft :  sinh(x) := exp(x)/2 
//          lnovft   <= x <= ln2ovft:  sinh(x) := exp(x/2)/2 * exp(x/2)
//          ln2ovft  <  x           :  sinh(x) := x*shuge (overflow)
//
// Special cases:
//      sinh(x) is |x| if x is +Infinity, -Infinity, or NaN.
//      only sinh(0)=0 is exact for finite x.
//
define KSINH_OVERFLOW = 710.4758600739439;
define TWO_M28 = 3.725290298461914e-9;  // 2^-28, empty lower half
define LOG_MAXD = 709.7822265625;  // 0x40862e42 00000000, empty lower half

function MathSinh(x) {
  x = x * 1;  // Convert to number.
  var h = (x < 0) ? -0.5 : 0.5;
  // |x| in [0, 22]. return sign(x)*0.5*(E+E/(E+1))
  var ax = MathAbs(x);
  if (ax < 22) {
    // For |x| < 2^-28, sinh(x) = x
    if (ax < TWO_M28) return x;
    var t = MathExpm1(ax);
    if (ax < 1) return h * (2 * t - t * t / (t + 1));
    return h * (t + t / (t + 1));
  }
  // |x| in [22, log(maxdouble)], return 0.5 * exp(|x|)
  if (ax < LOG_MAXD) return h * MathExp(ax);
  // |x| in [log(maxdouble), overflowthreshold]
  // overflowthreshold = 710.4758600739426
  if (ax <= KSINH_OVERFLOW) {
    var w = MathExp(0.5 * ax);
    var t = h * w;
    return t * w;
  }
  // |x| > overflowthreshold or is NaN.
  // Return Infinity of the appropriate sign or NaN.
  return x * INFINITY;
}


// ES6 draft 09-27-13, section 20.2.2.12.
// Math.cosh
// Method : 
// mathematically cosh(x) if defined to be (exp(x)+exp(-x))/2
//      1. Replace x by |x| (cosh(x) = cosh(-x)). 
//      2.
//                                                      [ exp(x) - 1 ]^2 
//          0        <= x <= ln2/2  :  cosh(x) := 1 + -------------------
//                                                         2*exp(x)
//
//                                                 exp(x) + 1/exp(x)
//          ln2/2    <= x <= 22     :  cosh(x) := -------------------
//                                                        2
//          22       <= x <= lnovft :  cosh(x) := exp(x)/2 
//          lnovft   <= x <= ln2ovft:  cosh(x) := exp(x/2)/2 * exp(x/2)
//          ln2ovft  <  x           :  cosh(x) := huge*huge (overflow)
//
// Special cases:
//      cosh(x) is |x| if x is +INF, -INF, or NaN.
//      only cosh(0)=1 is exact for finite x.
//
define KCOSH_OVERFLOW = 710.4758600739439;

function MathCosh(x) {
  x = x * 1;  // Convert to number.
  var ix = %_DoubleHi(x) & 0x7fffffff;
  // |x| in [0,0.5*log2], return 1+expm1(|x|)^2/(2*exp(|x|))
  if (ix < 0x3fd62e43) {
    var t = MathExpm1(MathAbs(x));
    var w = 1 + t;
    // For |x| < 2^-55, cosh(x) = 1
    if (ix < 0x3c800000) return w;
    return 1 + (t * t) / (w + w);
  }
  // |x| in [0.5*log2, 22], return (exp(|x|)+1/exp(|x|)/2
  if (ix < 0x40360000) {
    var t = MathExp(MathAbs(x));
    return 0.5 * t + 0.5 / t;
  }
  // |x| in [22, log(maxdouble)], return half*exp(|x|)
  if (ix < 0x40862e42) return 0.5 * MathExp(MathAbs(x));
  // |x| in [log(maxdouble), overflowthreshold]
  if (MathAbs(x) <= KCOSH_OVERFLOW) {
    var w = MathExp(0.5 * MathAbs(x));
    var t = 0.5 * w;
    return t * w;
  }
  if (NUMBER_IS_NAN(x)) return x;
  // |x| > overflowthreshold.
  return INFINITY;
}

// ES6 draft 09-27-13, section 20.2.2.33.
// Math.tanh(x)
// Method :
//                                    x    -x
//                                   e  - e
//     0. tanh(x) is defined to be -----------
//                                    x    -x
//                                   e  + e
//     1. reduce x to non-negative by tanh(-x) = -tanh(x).
//     2.  0      <= x <= 2**-55 : tanh(x) := x*(one+x)
//                                             -t
//         2**-55 <  x <=  1     : tanh(x) := -----; t = expm1(-2x)
//                                            t + 2
//                                                  2
//         1      <= x <=  22.0  : tanh(x) := 1-  ----- ; t = expm1(2x)
//                                                t + 2
//         22.0   <  x <= INF    : tanh(x) := 1.
//
// Special cases:
//     tanh(NaN) is NaN;
//     only tanh(0) = 0 is exact for finite argument.
//

define TWO_M55 = 2.77555756156289135105e-17;  // 2^-55, empty lower half

function MathTanh(x) {
  x = x * 1;  // Convert to number.
  // x is Infinity or NaN
  if (!NUMBER_IS_FINITE(x)) {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return x;
  }

  var ax = MathAbs(x);
  var z;
  // |x| < 22
  if (ax < 22) {
    if (ax < TWO_M55) {
      // |x| < 2^-55, tanh(small) = small.
      return x;
    }
    if (ax >= 1) {
      // |x| >= 1
      var t = MathExpm1(2 * ax);
      z = 1 - 2 / (t + 2);
    } else {
      var t = MathExpm1(-2 * ax);
      z = -t / (t + 2);
    }
  } else {
    // |x| > 22, return +/- 1
    z = 1;
  }
  return (x >= 0) ? z : -z;
}

// ES6 draft 09-27-13, section 20.2.2.21.
// Return the base 10 logarithm of x
//
// Method :
//      Let log10_2hi = leading 40 bits of log10(2) and
//          log10_2lo = log10(2) - log10_2hi,
//          ivln10   = 1/log(10) rounded.
//      Then
//              n = ilogb(x),
//              if(n<0)  n = n+1;
//              x = scalbn(x,-n);
//              log10(x) := n*log10_2hi + (n*log10_2lo + ivln10*log(x))
//
// Note 1:
//      To guarantee log10(10**n)=n, where 10**n is normal, the rounding
//      mode must set to Round-to-Nearest.
// Note 2:
//      [1/log(10)] rounded to 53 bits has error .198 ulps;
//      log10 is monotonic at all binary break points.
//
// Special cases:
//      log10(x) is NaN if x < 0;
//      log10(+INF) is +INF; log10(0) is -INF;
//      log10(NaN) is that NaN;
//      log10(10**N) = N  for N=0,1,...,22.
//

define IVLN10 = 4.34294481903251816668e-01;
define LOG10_2HI = 3.01029995663611771306e-01;
define LOG10_2LO = 3.69423907715893078616e-13;

function MathLog10(x) {
  x = x * 1;  // Convert to number.
  var hx = %_DoubleHi(x);
  var lx = %_DoubleLo(x);
  var k = 0;

  if (hx < 0x00100000) {
    // x < 2^-1022
    // log10(+/- 0) = -Infinity.
    if (((hx & 0x7fffffff) | lx) === 0) return -INFINITY;
    // log10 of negative number is NaN.
    if (hx < 0) return NaN;
    // Subnormal number. Scale up x.
    k -= 54;
    x *= TWO54;
    hx = %_DoubleHi(x);
    lx = %_DoubleLo(x);
  }

  // Infinity or NaN.
  if (hx >= 0x7ff00000) return x;

  k += (hx >> 20) - 1023;
  var i = (k & 0x80000000) >>> 31;
  hx = (hx & 0x000fffff) | ((0x3ff - i) << 20);
  var y = k + i;
  x = %_ConstructDouble(hx, lx);

  var z = y * LOG10_2LO + IVLN10 * %_MathLogRT(x);
  return z + y * LOG10_2HI;
}


// ES6 draft 09-27-13, section 20.2.2.22.
// Return the base 2 logarithm of x
//
// fdlibm does not have an explicit log2 function, but fdlibm's pow
// function does implement an accurate log2 function as part of the
// pow implementation.  This extracts the core parts of that as a
// separate log2 function.

// Method:
// Compute log2(x) in two pieces:
// log2(x) = w1 + w2
// where w1 has 53-24 = 29 bits of trailing zeroes.

define DP_H = 5.84962487220764160156e-01;
define DP_L = 1.35003920212974897128e-08;

// Polynomial coefficients for (3/2)*(log2(x) - 2*s - 2/3*s^3)
define LOG2_1 = 5.99999999999994648725e-01;
define LOG2_2 = 4.28571428578550184252e-01;
define LOG2_3 = 3.33333329818377432918e-01;
define LOG2_4 = 2.72728123808534006489e-01;
define LOG2_5 = 2.30660745775561754067e-01;
define LOG2_6 = 2.06975017800338417784e-01;

// cp = 2/(3*ln(2)). Note that cp_h + cp_l is cp, but with more accuracy.
define CP = 9.61796693925975554329e-01;
define CP_H = 9.61796700954437255859e-01;
define CP_L = -7.02846165095275826516e-09;
// 2^53
define TWO53 = 9007199254740992; 

function MathLog2(x) {
  x = x * 1;  // Convert to number.
  var ax = MathAbs(x);
  var hx = %_DoubleHi(x);
  var lx = %_DoubleLo(x);
  var ix = hx & 0x7fffffff;

  // Handle special cases.
  // log2(+/- 0) = -Infinity
  if ((ix | lx) == 0) return -INFINITY;

  // log(x) = NaN, if x < 0
  if (hx < 0) return NaN;

  // log2(Infinity) = Infinity, log2(NaN) = NaN
  if (ix >= 0x7ff00000) return x;

  var n = 0;

  // Take care of subnormal number.
  if (ix < 0x00100000) {
    ax *= TWO53;
    n -= 53;
    ix = %_DoubleHi(ax);
  }

  n += (ix >> 20) - 0x3ff;
  var j = ix & 0x000fffff;

  // Determine interval.
  ix = j | 0x3ff00000;  // normalize ix.

  var bp = 1;
  var dp_h = 0;
  var dp_l = 0;
  if (j > 0x3988e) {  // |x| > sqrt(3/2)
    if (j < 0xbb67a) {  // |x| < sqrt(3)
      bp = 1.5;
      dp_h = DP_H;
      dp_l = DP_L;
    } else {
      n += 1;
      ix -= 0x00100000;
    }
  }
 
  ax = %_ConstructDouble(ix, %_DoubleLo(ax));

  // Compute ss = s_h + s_l = (x - 1)/(x+1) or (x - 1.5)/(x + 1.5)
  var u = ax - bp;
  var v = 1 / (ax + bp);
  var ss = u * v;
  var s_h = %_ConstructDouble(%_DoubleHi(ss), 0);

  // t_h = ax + bp[k] High
  var t_h = %_ConstructDouble(%_DoubleHi(ax + bp), 0)
  var t_l = ax - (t_h - bp);
  var s_l = v * ((u - s_h * t_h) - s_h * t_l);

  // Compute log2(ax)
  var s2 = ss * ss;
  var r = s2 * s2 * (LOG2_1 + s2 * (LOG2_2 + s2 * (LOG2_3 + s2 * (
                     LOG2_4 + s2 * (LOG2_5 + s2 * LOG2_6)))));
  r += s_l * (s_h + ss);
  s2  = s_h * s_h;
  t_h = %_ConstructDouble(%_DoubleHi(3.0 + s2 + r), 0);
  t_l = r - ((t_h - 3.0) - s2);
  // u + v = ss * (1 + ...)
  u = s_h * t_h;
  v = s_l * t_h + t_l * ss;

  // 2 / (3 * log(2)) * (ss + ...)
  var p_h = %_ConstructDouble(%_DoubleHi(u + v), 0);
  var p_l = v - (p_h - u);
  var z_h = CP_H * p_h;
  var z_l = CP_L * p_h + p_l * CP + dp_l;

  // log2(ax) = (ss + ...) * 2 / (3 * log(2)) = n + dp_h + z_h + z_l
  var t = n;
  var t1 = %_ConstructDouble(%_DoubleHi(((z_h + z_l) + dp_h) + t), 0);
  var t2 = z_l - (((t1 - t) - dp_h) - z_h);

  // t1 + t2 = log2(ax), sum up because we do not care about extra precision.
  return t1 + t2;
}

//-------------------------------------------------------------------

utils.InstallFunctions(GlobalMath, DONT_ENUM, [
  "cos", MathCos,
  "sin", MathSin,
  "tan", MathTan,
  "sinh", MathSinh,
  "cosh", MathCosh,
  "tanh", MathTanh,
  "log10", MathLog10,
  "log2", MathLog2,
  "log1p", MathLog1p,
  "expm1", MathExpm1
]);

%SetForceInlineFlag(MathSin);
%SetForceInlineFlag(MathCos);

})
