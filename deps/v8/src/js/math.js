// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

// The first two slots are reserved to persist PRNG state.
define kRandomNumberStart = 2;

var GlobalFloat64Array = global.Float64Array;
var GlobalMath = global.Math;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var NaN = %GetRootNaN();
var nextRandomIndex = 0;
var randomNumbers = UNDEFINED;
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

//-------------------------------------------------------------------

// ECMA 262 - 15.8.2.1
function MathAbs(x) {
  x = +x;
  return (x > 0) ? x : 0 - x;
}

// ECMA 262 - 15.8.2.5
// The naming of y and x matches the spec, as does the order in which
// ToNumber (valueOf) is called.
function MathAtan2JS(y, x) {
  y = +y;
  x = +x;
  return %MathAtan2(y, x);
}

// ECMA 262 - 15.8.2.8
function MathExp(x) {
  return %MathExpRT(TO_NUMBER(x));
}

// ECMA 262 - 15.8.2.10
function MathLog(x) {
  return %_MathLogRT(TO_NUMBER(x));
}

// ECMA 262 - 15.8.2.13
function MathPowJS(x, y) {
  return %_MathPow(TO_NUMBER(x), TO_NUMBER(y));
}

// ECMA 262 - 15.8.2.14
function MathRandom() {
  // While creating a startup snapshot, %GenerateRandomNumbers returns a
  // normal array containing a single random number, and has to be called for
  // every new random number.
  // Otherwise, it returns a pre-populated typed array of random numbers. The
  // first two elements are reserved for the PRNG state.
  if (nextRandomIndex <= kRandomNumberStart) {
    randomNumbers = %GenerateRandomNumbers(randomNumbers);
    nextRandomIndex = randomNumbers.length;
  }
  return randomNumbers[--nextRandomIndex];
}

function MathRandomRaw() {
  if (nextRandomIndex <= kRandomNumberStart) {
    randomNumbers = %GenerateRandomNumbers(randomNumbers);
    nextRandomIndex = randomNumbers.length;
  }
  return %_DoubleLo(randomNumbers[--nextRandomIndex]) & 0x3FFFFFFF;
}

// ES6 draft 09-27-13, section 20.2.2.28.
function MathSign(x) {
  x = +x;
  if (x > 0) return 1;
  if (x < 0) return -1;
  // -0, 0 or NaN.
  return x;
}

// ES6 draft 09-27-13, section 20.2.2.5.
function MathAsinh(x) {
  x = TO_NUMBER(x);
  // Idempotent for NaN, +/-0 and +/-Infinity.
  if (x === 0 || !NUMBER_IS_FINITE(x)) return x;
  if (x > 0) return MathLog(x + %math_sqrt(x * x + 1));
  // This is to prevent numerical errors caused by large negative x.
  return -MathLog(-x + %math_sqrt(x * x + 1));
}

// ES6 draft 09-27-13, section 20.2.2.3.
function MathAcosh(x) {
  x = TO_NUMBER(x);
  if (x < 1) return NaN;
  // Idempotent for NaN and +Infinity.
  if (!NUMBER_IS_FINITE(x)) return x;
  return MathLog(x + %math_sqrt(x + 1) * %math_sqrt(x - 1));
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
  var length = arguments.length;
  var max = 0;
  for (var i = 0; i < length; i++) {
    var n = MathAbs(arguments[i]);
    if (n > max) max = n;
    arguments[i] = n;
  }
  if (max === INFINITY) return INFINITY;

  // Kahan summation to avoid rounding errors.
  // Normalize the numbers to the largest one to avoid overflow.
  if (max === 0) max = 1;
  var sum = 0;
  var compensation = 0;
  for (var i = 0; i < length; i++) {
    var n = arguments[i] / max;
    var summand = n * n - compensation;
    var preliminary = sum + summand;
    compensation = (preliminary - sum) - summand;
    sum = preliminary;
  }
  return %math_sqrt(sum) * max;
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
  var approx_hi = %math_floor(%_DoubleHi(x) / 3) + 0x2A9F7893;
  var approx = %_ConstructDouble(approx_hi | 0, 0);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  return NEWTON_ITERATION_CBRT(x, approx);
}

// -------------------------------------------------------------------

%InstallToContext([
  "math_pow", MathPowJS,
]);

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
  "exp", MathExp,
  "log", MathLog,
  "atan2", MathAtan2JS,
  "pow", MathPowJS,
  "sign", MathSign,
  "asinh", MathAsinh,
  "acosh", MathAcosh,
  "atanh", MathAtanh,
  "hypot", MathHypot,
  "cbrt", MathCbrt
]);

%SetForceInlineFlag(MathAbs);
%SetForceInlineFlag(MathAtan2JS);
%SetForceInlineFlag(MathRandom);
%SetForceInlineFlag(MathSign);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.MathAbs = MathAbs;
  to.MathExp = MathExp;
  to.IntRandom = MathRandomRaw;
});

})
