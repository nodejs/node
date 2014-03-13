// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Object = global.Object;

// Keep reference to original values of some global properties.  This
// has the added benefit that the code in this file is isolated from
// changes to these properties.
var $floor = MathFloor;
var $abs = MathAbs;

// Instance class name can only be set on functions. That is the only
// purpose for MathConstructor.
function MathConstructor() {}
var $Math = new MathConstructor();

// -------------------------------------------------------------------

// ECMA 262 - 15.8.2.1
function MathAbs(x) {
  if (%_IsSmi(x)) return x >= 0 ? x : -x;
  x = TO_NUMBER_INLINE(x);
  if (x === 0) return 0;  // To handle -0.
  return x > 0 ? x : -x;
}

// ECMA 262 - 15.8.2.2
function MathAcos(x) {
  return %Math_acos(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.3
function MathAsin(x) {
  return %Math_asin(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.4
function MathAtan(x) {
  return %Math_atan(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.5
// The naming of y and x matches the spec, as does the order in which
// ToNumber (valueOf) is called.
function MathAtan2(y, x) {
  return %Math_atan2(TO_NUMBER_INLINE(y), TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.6
function MathCeil(x) {
  return -MathFloor(-x);
}

// ECMA 262 - 15.8.2.7
function MathCos(x) {
  x = MathAbs(x);  // Convert to number and get rid of -0.
  return TrigonometricInterpolation(x, 1);
}

// ECMA 262 - 15.8.2.8
function MathExp(x) {
  return %Math_exp(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.9
function MathFloor(x) {
  x = TO_NUMBER_INLINE(x);
  // It's more common to call this with a positive number that's out
  // of range than negative numbers; check the upper bound first.
  if (x < 0x80000000 && x > 0) {
    // Numbers in the range [0, 2^31) can be floored by converting
    // them to an unsigned 32-bit value using the shift operator.
    // We avoid doing so for -0, because the result of Math.floor(-0)
    // has to be -0, which wouldn't be the case with the shift.
    return TO_UINT32(x);
  } else {
    return %Math_floor(x);
  }
}

// ECMA 262 - 15.8.2.10
function MathLog(x) {
  return %_MathLog(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.11
function MathMax(arg1, arg2) {  // length == 2
  var length = %_ArgumentsLength();
  if (length == 2) {
    arg1 = TO_NUMBER_INLINE(arg1);
    arg2 = TO_NUMBER_INLINE(arg2);
    if (arg2 > arg1) return arg2;
    if (arg1 > arg2) return arg1;
    if (arg1 == arg2) {
      // Make sure -0 is considered less than +0.
      return (arg1 === 0 && %_IsMinusZero(arg1)) ? arg2 : arg1;
    }
    // All comparisons failed, one of the arguments must be NaN.
    return NAN;
  }
  var r = -INFINITY;
  for (var i = 0; i < length; i++) {
    var n = %_Arguments(i);
    if (!IS_NUMBER(n)) n = NonNumberToNumber(n);
    // Make sure +0 is considered greater than -0.
    if (NUMBER_IS_NAN(n) || n > r || (r === 0 && n === 0 && %_IsMinusZero(r))) {
      r = n;
    }
  }
  return r;
}

// ECMA 262 - 15.8.2.12
function MathMin(arg1, arg2) {  // length == 2
  var length = %_ArgumentsLength();
  if (length == 2) {
    arg1 = TO_NUMBER_INLINE(arg1);
    arg2 = TO_NUMBER_INLINE(arg2);
    if (arg2 > arg1) return arg1;
    if (arg1 > arg2) return arg2;
    if (arg1 == arg2) {
      // Make sure -0 is considered less than +0.
      return (arg1 === 0 && %_IsMinusZero(arg1)) ? arg1 : arg2;
    }
    // All comparisons failed, one of the arguments must be NaN.
    return NAN;
  }
  var r = INFINITY;
  for (var i = 0; i < length; i++) {
    var n = %_Arguments(i);
    if (!IS_NUMBER(n)) n = NonNumberToNumber(n);
    // Make sure -0 is considered less than +0.
    if (NUMBER_IS_NAN(n) || n < r || (r === 0 && n === 0 && %_IsMinusZero(n))) {
      r = n;
    }
  }
  return r;
}

// ECMA 262 - 15.8.2.13
function MathPow(x, y) {
  return %_MathPow(TO_NUMBER_INLINE(x), TO_NUMBER_INLINE(y));
}

// ECMA 262 - 15.8.2.14
var rngstate;  // Initialized to a Uint32Array during genesis.
function MathRandom() {
  var r0 = (MathImul(18273, rngstate[0] & 0xFFFF) + (rngstate[0] >>> 16)) | 0;
  rngstate[0] = r0;
  var r1 = (MathImul(36969, rngstate[1] & 0xFFFF) + (rngstate[1] >>> 16)) | 0;
  rngstate[1] = r1;
  var x = ((r0 << 16) + (r1 & 0xFFFF)) | 0;
  // Division by 0x100000000 through multiplication by reciprocal.
  return (x < 0 ? (x + 0x100000000) : x) * 2.3283064365386962890625e-10;
}

// ECMA 262 - 15.8.2.15
function MathRound(x) {
  return %RoundNumber(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.16
function MathSin(x) {
  x = x * 1;  // Convert to number and deal with -0.
  if (%_IsMinusZero(x)) return x;
  return TrigonometricInterpolation(x, 0);
}

// ECMA 262 - 15.8.2.17
function MathSqrt(x) {
  return %_MathSqrt(TO_NUMBER_INLINE(x));
}

// ECMA 262 - 15.8.2.18
function MathTan(x) {
  return MathSin(x) / MathCos(x);
}

// Non-standard extension.
function MathImul(x, y) {
  return %NumberImul(TO_NUMBER_INLINE(x), TO_NUMBER_INLINE(y));
}


var kInversePiHalf      = 0.636619772367581343;      // 2 / pi
var kInversePiHalfS26   = 9.48637384723993156e-9;    // 2 / pi / (2^26)
var kS26                = 1 << 26;
var kTwoStepThreshold   = 1 << 27;
// pi / 2 rounded up
var kPiHalf             = 1.570796326794896780;      // 0x192d4454fb21f93f
// We use two parts for pi/2 to emulate a higher precision.
// pi_half_1 only has 26 significant bits for mantissa.
// Note that pi_half > pi_half_1 + pi_half_2
var kPiHalf1            = 1.570796325802803040;      // 0x00000054fb21f93f
var kPiHalf2            = 9.920935796805404252e-10;  // 0x3326a611460b113e

var kSamples;            // Initialized to a number during genesis.
var kIndexConvert;       // Initialized to kSamples / (pi/2) during genesis.
var kSinTable;           // Initialized to a Float64Array during genesis.
var kCosXIntervalTable;  // Initialized to a Float64Array during genesis.

// This implements sine using the following algorithm.
// 1) Multiplication takes care of to-number conversion.
// 2) Reduce x to the first quadrant [0, pi/2].
//    Conveniently enough, in case of +/-Infinity, we get NaN.
//    Note that we try to use only 26 instead of 52 significant bits for
//    mantissa to avoid rounding errors when multiplying.  For very large
//    input we therefore have additional steps.
// 3) Replace x by (pi/2-x) if x was in the 2nd or 4th quadrant.
// 4) Do a table lookup for the closest samples to the left and right of x.
// 5) Find the derivatives at those sampling points by table lookup:
//    dsin(x)/dx = cos(x) = sin(pi/2-x) for x in [0, pi/2].
// 6) Use cubic spline interpolation to approximate sin(x).
// 7) Negate the result if x was in the 3rd or 4th quadrant.
// 8) Get rid of -0 by adding 0.
function TrigonometricInterpolation(x, phase) {
  if (x < 0 || x > kPiHalf) {
    var multiple;
    while (x < -kTwoStepThreshold || x > kTwoStepThreshold) {
      // Let's assume this loop does not terminate.
      // All numbers x in each loop forms a set S.
      // (1) abs(x) > 2^27 for all x in S.
      // (2) abs(multiple) != 0 since (2^27 * inverse_pi_half_s26) > 1
      // (3) multiple is rounded down in 2^26 steps, so the rounding error is
      //     at most max(ulp, 2^26).
      // (4) so for x > 2^27, we subtract at most (1+pi/4)x and at least
      //     (1-pi/4)x
      // (5) The subtraction results in x' so that abs(x') <= abs(x)*pi/4.
      //     Note that this difference cannot be simply rounded off.
      // Set S cannot exist since (5) violates (1).  Loop must terminate.
      multiple = MathFloor(x * kInversePiHalfS26) * kS26;
      x = x - multiple * kPiHalf1 - multiple * kPiHalf2;
    }
    multiple = MathFloor(x * kInversePiHalf);
    x = x - multiple * kPiHalf1 - multiple * kPiHalf2;
    phase += multiple;
  }
  var double_index = x * kIndexConvert;
  if (phase & 1) double_index = kSamples - double_index;
  var index = double_index | 0;
  var t1 = double_index - index;
  var t2 = 1 - t1;
  var y1 = kSinTable[index];
  var y2 = kSinTable[index + 1];
  var dy = y2 - y1;
  return (t2 * y1 + t1 * y2 +
              t1 * t2 * ((kCosXIntervalTable[index] - dy) * t2 +
                         (dy - kCosXIntervalTable[index + 1]) * t1))
         * (1 - (phase & 2)) + 0;
}

// -------------------------------------------------------------------

function SetUpMath() {
  %CheckIsBootstrapping();

  %SetPrototype($Math, $Object.prototype);
  %SetProperty(global, "Math", $Math, DONT_ENUM);
  %FunctionSetInstanceClassName(MathConstructor, 'Math');

  // Set up math constants.
  InstallConstants($Math, $Array(
    // ECMA-262, section 15.8.1.1.
    "E", 2.7182818284590452354,
    // ECMA-262, section 15.8.1.2.
    "LN10", 2.302585092994046,
    // ECMA-262, section 15.8.1.3.
    "LN2", 0.6931471805599453,
    // ECMA-262, section 15.8.1.4.
    "LOG2E", 1.4426950408889634,
    "LOG10E", 0.4342944819032518,
    "PI", 3.1415926535897932,
    "SQRT1_2", 0.7071067811865476,
    "SQRT2", 1.4142135623730951
  ));

  // Set up non-enumerable functions of the Math object and
  // set their names.
  InstallFunctions($Math, DONT_ENUM, $Array(
    "random", MathRandom,
    "abs", MathAbs,
    "acos", MathAcos,
    "asin", MathAsin,
    "atan", MathAtan,
    "ceil", MathCeil,
    "cos", MathCos,
    "exp", MathExp,
    "floor", MathFloor,
    "log", MathLog,
    "round", MathRound,
    "sin", MathSin,
    "sqrt", MathSqrt,
    "tan", MathTan,
    "atan2", MathAtan2,
    "pow", MathPow,
    "max", MathMax,
    "min", MathMin,
    "imul", MathImul
  ));

  %SetInlineBuiltinFlag(MathCeil);
  %SetInlineBuiltinFlag(MathRandom);
  %SetInlineBuiltinFlag(MathSin);
  %SetInlineBuiltinFlag(MathCos);
  %SetInlineBuiltinFlag(MathTan);
  %SetInlineBuiltinFlag(TrigonometricInterpolation);
}

SetUpMath();
