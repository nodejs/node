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
// The following is a straightforward translation of fdlibm routines for
// sin, cos, and tan, by Raymond Toy (rtoy@google.com).


var kTrig;  // Initialized to a Float64Array during genesis and is not writable.

const INVPIO2 = kTrig[0];
const PIO2_1  = kTrig[1];
const PIO2_1T = kTrig[2];
const PIO2_2  = kTrig[3];
const PIO2_2T = kTrig[4];
const PIO2_3  = kTrig[5];
const PIO2_3T = kTrig[6];
const PIO4    = kTrig[32];
const PIO4LO  = kTrig[33];

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
kTrig[7+x]
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
kTrig[13+x]
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
kTrig[19+x]
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
