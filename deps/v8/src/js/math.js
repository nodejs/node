// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

define kRandomBatchSize = 64;
// The first two slots are reserved to persist PRNG state.
define kRandomNumberStart = 2;

var GlobalFloat64Array = global.Float64Array;
var GlobalMath = global.Math;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var NaN = %GetRootNaN();
var nextRandomIndex = kRandomBatchSize;
var randomNumbers = UNDEFINED;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

//-------------------------------------------------------------------

// ECMA 262 - 15.8.2.1
function MathAbs(x) {
  x = +x;
  return (x > 0) ? x : 0 - x;
}

// ECMA 262 - 15.8.2.2
function MathAcosJS(x) {
  return %_MathAcos(+x);
}

// ECMA 262 - 15.8.2.3
function MathAsinJS(x) {
  return %_MathAsin(+x);
}

// ECMA 262 - 15.8.2.4
function MathAtanJS(x) {
  return %_MathAtan(+x);
}

// ECMA 262 - 15.8.2.5
// The naming of y and x matches the spec, as does the order in which
// ToNumber (valueOf) is called.
function MathAtan2JS(y, x) {
  y = +y;
  x = +x;
  return %_MathAtan2(y, x);
}

// ECMA 262 - 15.8.2.6
function MathCeil(x) {
  return -%_MathFloor(-x);
}

// ECMA 262 - 15.8.2.8
function MathExp(x) {
  return %MathExpRT(TO_NUMBER(x));
}

// ECMA 262 - 15.8.2.9
function MathFloorJS(x) {
  return %_MathFloor(+x);
}

// ECMA 262 - 15.8.2.10
function MathLog(x) {
  return %_MathLogRT(TO_NUMBER(x));
}

// ECMA 262 - 15.8.2.11
function MathMax(arg1, arg2) {  // length == 2
  var length = %_ArgumentsLength();
  if (length == 2) {
    arg1 = TO_NUMBER(arg1);
    arg2 = TO_NUMBER(arg2);
    if (arg2 > arg1) return arg2;
    if (arg1 > arg2) return arg1;
    if (arg1 == arg2) {
      // Make sure -0 is considered less than +0.
      return (arg1 === 0 && %_IsMinusZero(arg1)) ? arg2 : arg1;
    }
    // All comparisons failed, one of the arguments must be NaN.
    return NaN;
  }
  var r = -INFINITY;
  for (var i = 0; i < length; i++) {
    var n = %_Arguments(i);
    n = TO_NUMBER(n);
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
    arg1 = TO_NUMBER(arg1);
    arg2 = TO_NUMBER(arg2);
    if (arg2 > arg1) return arg1;
    if (arg1 > arg2) return arg2;
    if (arg1 == arg2) {
      // Make sure -0 is considered less than +0.
      return (arg1 === 0 && %_IsMinusZero(arg1)) ? arg1 : arg2;
    }
    // All comparisons failed, one of the arguments must be NaN.
    return NaN;
  }
  var r = INFINITY;
  for (var i = 0; i < length; i++) {
    var n = %_Arguments(i);
    n = TO_NUMBER(n);
    // Make sure -0 is considered less than +0.
    if (NUMBER_IS_NAN(n) || n < r || (r === 0 && n === 0 && %_IsMinusZero(n))) {
      r = n;
    }
  }
  return r;
}

// ECMA 262 - 15.8.2.13
function MathPowJS(x, y) {
  return %_MathPow(TO_NUMBER(x), TO_NUMBER(y));
}

// ECMA 262 - 15.8.2.14
function MathRandom() {
  if (nextRandomIndex >= kRandomBatchSize) {
    randomNumbers = %GenerateRandomNumbers(randomNumbers);
    nextRandomIndex = kRandomNumberStart;
  }
  return randomNumbers[nextRandomIndex++];
}

function MathRandomRaw() {
  if (nextRandomIndex >= kRandomBatchSize) {
    randomNumbers = %GenerateRandomNumbers(randomNumbers);
    nextRandomIndex = kRandomNumberStart;
  }
  return %_DoubleLo(randomNumbers[nextRandomIndex++]) & 0x3FFFFFFF;
}

// ECMA 262 - 15.8.2.15
function MathRound(x) {
  return %RoundNumber(TO_NUMBER(x));
}

// ECMA 262 - 15.8.2.17
function MathSqrtJS(x) {
  return %_MathSqrt(+x);
}

// Non-standard extension.
function MathImul(x, y) {
  return %NumberImul(TO_NUMBER(x), TO_NUMBER(y));
}

// ES6 draft 09-27-13, section 20.2.2.28.
function MathSign(x) {
  x = +x;
  if (x > 0) return 1;
  if (x < 0) return -1;
  // -0, 0 or NaN.
  return x;
}

// ES6 draft 09-27-13, section 20.2.2.34.
function MathTrunc(x) {
  x = +x;
  if (x > 0) return %_MathFloor(x);
  if (x < 0) return -%_MathFloor(-x);
  // -0, 0 or NaN.
  return x;
}

// ES6 draft 09-27-13, section 20.2.2.5.
function MathAsinh(x) {
  x = TO_NUMBER(x);
  // Idempotent for NaN, +/-0 and +/-Infinity.
  if (x === 0 || !NUMBER_IS_FINITE(x)) return x;
  if (x > 0) return MathLog(x + %_MathSqrt(x * x + 1));
  // This is to prevent numerical errors caused by large negative x.
  return -MathLog(-x + %_MathSqrt(x * x + 1));
}

// ES6 draft 09-27-13, section 20.2.2.3.
function MathAcosh(x) {
  x = TO_NUMBER(x);
  if (x < 1) return NaN;
  // Idempotent for NaN and +Infinity.
  if (!NUMBER_IS_FINITE(x)) return x;
  return MathLog(x + %_MathSqrt(x + 1) * %_MathSqrt(x - 1));
}

// ES6 draft 09-27-13, section 20.2.2.7.
function MathAtanh(x) {
  x = TO_NUMBER(x);
  // Idempotent for +/-0.
  if (x === 0) return x;
  // Returns NaN for NaN and +/- Infinity.
  if (!NUMBER_IS_FINITE(x)) return NaN;
  return 0.5 * MathLog((1 + x) / (1 - x));
}

// ES6 draft 09-27-13, section 20.2.2.17.
function MathHypot(x, y) {  // Function length is 2.
  // We may want to introduce fast paths for two arguments and when
  // normalization to avoid overflow is not necessary.  For now, we
  // simply assume the general case.
  var length = %_ArgumentsLength();
  var args = new InternalArray(length);
  var max = 0;
  for (var i = 0; i < length; i++) {
    var n = %_Arguments(i);
    n = TO_NUMBER(n);
    if (n === INFINITY || n === -INFINITY) return INFINITY;
    n = MathAbs(n);
    if (n > max) max = n;
    args[i] = n;
  }

  // Kahan summation to avoid rounding errors.
  // Normalize the numbers to the largest one to avoid overflow.
  if (max === 0) max = 1;
  var sum = 0;
  var compensation = 0;
  for (var i = 0; i < length; i++) {
    var n = args[i] / max;
    var summand = n * n - compensation;
    var preliminary = sum + summand;
    compensation = (preliminary - sum) - summand;
    sum = preliminary;
  }
  return %_MathSqrt(sum) * max;
}

// ES6 draft 09-27-13, section 20.2.2.16.
function MathFroundJS(x) {
  return %MathFround(TO_NUMBER(x));
}

// ES6 draft 07-18-14, section 20.2.2.11
function MathClz32JS(x) {
  return %_MathClz32(x >>> 0);
}

// ES6 draft 09-27-13, section 20.2.2.9.
// Cube root approximation, refer to: http://metamerist.com/cbrt/cbrt.htm
// Using initial approximation adapted from Kahan's cbrt and 4 iterations
// of Newton's method.
function MathCbrt(x) {
  x = TO_NUMBER(x);
  if (x == 0 || !NUMBER_IS_FINITE(x)) return x;
  return x >= 0 ? CubeRoot(x) : -CubeRoot(-x);
}

macro NEWTON_ITERATION_CBRT(x, approx)
  (1.0 / 3.0) * (x / (approx * approx) + 2 * approx);
endmacro

function CubeRoot(x) {
  var approx_hi = MathFloorJS(%_DoubleHi(x) / 3) + 0x2A9F7893;
  var approx = %_ConstructDouble(approx_hi | 0, 0);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  return NEWTON_ITERATION_CBRT(x, approx);
}

// -------------------------------------------------------------------

%AddNamedProperty(GlobalMath, toStringTagSymbol, "Math", READ_ONLY | DONT_ENUM);

// Set up math constants.
utils.InstallConstants(GlobalMath, [
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
]);

// Set up non-enumerable functions of the Math object and
// set their names.
utils.InstallFunctions(GlobalMath, DONT_ENUM, [
  "random", MathRandom,
  "abs", MathAbs,
  "acos", MathAcosJS,
  "asin", MathAsinJS,
  "atan", MathAtanJS,
  "ceil", MathCeil,
  "exp", MathExp,
  "floor", MathFloorJS,
  "log", MathLog,
  "round", MathRound,
  "sqrt", MathSqrtJS,
  "atan2", MathAtan2JS,
  "pow", MathPowJS,
  "max", MathMax,
  "min", MathMin,
  "imul", MathImul,
  "sign", MathSign,
  "trunc", MathTrunc,
  "asinh", MathAsinh,
  "acosh", MathAcosh,
  "atanh", MathAtanh,
  "hypot", MathHypot,
  "fround", MathFroundJS,
  "clz32", MathClz32JS,
  "cbrt", MathCbrt
]);

%SetForceInlineFlag(MathAbs);
%SetForceInlineFlag(MathAcosJS);
%SetForceInlineFlag(MathAsinJS);
%SetForceInlineFlag(MathAtanJS);
%SetForceInlineFlag(MathAtan2JS);
%SetForceInlineFlag(MathCeil);
%SetForceInlineFlag(MathClz32JS);
%SetForceInlineFlag(MathFloorJS);
%SetForceInlineFlag(MathRandom);
%SetForceInlineFlag(MathSign);
%SetForceInlineFlag(MathSqrtJS);
%SetForceInlineFlag(MathTrunc);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.MathAbs = MathAbs;
  to.MathExp = MathExp;
  to.MathFloor = MathFloorJS;
  to.IntRandom = MathRandomRaw;
  to.MathMax = MathMax;
  to.MathMin = MathMin;
});

})
