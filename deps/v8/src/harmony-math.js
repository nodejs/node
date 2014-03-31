// Copyright 2013 the V8 project authors. All rights reserved.
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

'use strict';

// ES6 draft 09-27-13, section 20.2.2.28.
function MathSign(x) {
  x = TO_NUMBER_INLINE(x);
  if (x > 0) return 1;
  if (x < 0) return -1;
  if (x === 0) return x;
  return NAN;
}


// ES6 draft 09-27-13, section 20.2.2.34.
function MathTrunc(x) {
  x = TO_NUMBER_INLINE(x);
  if (x > 0) return MathFloor(x);
  if (x < 0) return MathCeil(x);
  if (x === 0) return x;
  return NAN;
}


// ES6 draft 09-27-13, section 20.2.2.30.
function MathSinh(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  // Idempotent for NaN, +/-0 and +/-Infinity.
  if (x === 0 || !NUMBER_IS_FINITE(x)) return x;
  return (MathExp(x) - MathExp(-x)) / 2;
}


// ES6 draft 09-27-13, section 20.2.2.12.
function MathCosh(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  if (!NUMBER_IS_FINITE(x)) return MathAbs(x);
  return (MathExp(x) + MathExp(-x)) / 2;
}


// ES6 draft 09-27-13, section 20.2.2.33.
function MathTanh(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  // Idempotent for +/-0.
  if (x === 0) return x;
  // Returns +/-1 for +/-Infinity.
  if (!NUMBER_IS_FINITE(x)) return MathSign(x);
  var exp1 = MathExp(x);
  var exp2 = MathExp(-x);
  return (exp1 - exp2) / (exp1 + exp2);
}


// ES6 draft 09-27-13, section 20.2.2.5.
function MathAsinh(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  // Idempotent for NaN, +/-0 and +/-Infinity.
  if (x === 0 || !NUMBER_IS_FINITE(x)) return x;
  if (x > 0) return MathLog(x + MathSqrt(x * x + 1));
  // This is to prevent numerical errors caused by large negative x.
  return -MathLog(-x + MathSqrt(x * x + 1));
}


// ES6 draft 09-27-13, section 20.2.2.3.
function MathAcosh(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  if (x < 1) return NAN;
  // Idempotent for NaN and +Infinity.
  if (!NUMBER_IS_FINITE(x)) return x;
  return MathLog(x + MathSqrt(x + 1) * MathSqrt(x - 1));
}


// ES6 draft 09-27-13, section 20.2.2.7.
function MathAtanh(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  // Idempotent for +/-0.
  if (x === 0) return x;
  // Returns NaN for NaN and +/- Infinity.
  if (!NUMBER_IS_FINITE(x)) return NAN;
  return 0.5 * MathLog((1 + x) / (1 - x));
}


// ES6 draft 09-27-13, section 20.2.2.21.
function MathLog10(x) {
  return MathLog(x) * 0.434294481903251828;  // log10(x) = log(x)/log(10).
}


// ES6 draft 09-27-13, section 20.2.2.22.
function MathLog2(x) {
  return MathLog(x) * 1.442695040888963407;  // log2(x) = log(x)/log(2).
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
    if (!IS_NUMBER(n)) n = NonNumberToNumber(n);
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
  return MathSqrt(sum) * max;
}


// ES6 draft 09-27-13, section 20.2.2.16.
function MathFround(x) {
  return %Math_fround(TO_NUMBER_INLINE(x));
}


function MathClz32(x) {
  x = ToUint32(TO_NUMBER_INLINE(x));
  if (x == 0) return 32;
  var result = 0;
  // Binary search.
  if ((x & 0xFFFF0000) === 0) { x <<= 16; result += 16; };
  if ((x & 0xFF000000) === 0) { x <<=  8; result +=  8; };
  if ((x & 0xF0000000) === 0) { x <<=  4; result +=  4; };
  if ((x & 0xC0000000) === 0) { x <<=  2; result +=  2; };
  if ((x & 0x80000000) === 0) { x <<=  1; result +=  1; };
  return result;
}


// ES6 draft 09-27-13, section 20.2.2.9.
// Cube root approximation, refer to: http://metamerist.com/cbrt/cbrt.htm
// Using initial approximation adapted from Kahan's cbrt and 4 iterations
// of Newton's method.
function MathCbrt(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  if (x == 0 || !NUMBER_IS_FINITE(x)) return x;
  return x >= 0 ? CubeRoot(x) : -CubeRoot(-x);
}

macro NEWTON_ITERATION_CBRT(x, approx)
  (1.0 / 3.0) * (x / (approx * approx) + 2 * approx);
endmacro

function CubeRoot(x) {
  var approx_hi = MathFloor(%_DoubleHi(x) / 3) + 0x2A9F7893;
  var approx = %_ConstructDouble(approx_hi, 0);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  approx = NEWTON_ITERATION_CBRT(x, approx);
  return NEWTON_ITERATION_CBRT(x, approx);
}



// ES6 draft 09-27-13, section 20.2.2.14.
// Use Taylor series to approximate.
// exp(x) - 1 at 0 == -1 + exp(0) + exp'(0)*x/1! + exp''(0)*x^2/2! + ...
//                 == x/1! + x^2/2! + x^3/3! + ...
// The closer x is to 0, the fewer terms are required.
function MathExpm1(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  var xabs = MathAbs(x);
  if (xabs < 2E-7) {
    return x * (1 + x * (1/2));
  } else if (xabs < 6E-5) {
    return x * (1 + x * (1/2 + x * (1/6)));
  } else if (xabs < 2E-2) {
    return x * (1 + x * (1/2 + x * (1/6 +
           x * (1/24 + x * (1/120 + x * (1/720))))));
  } else {  // Use regular exp if not close enough to 0.
    return MathExp(x) - 1;
  }
}


// ES6 draft 09-27-13, section 20.2.2.20.
// Use Taylor series to approximate. With y = x + 1;
// log(y) at 1 == log(1) + log'(1)(y-1)/1! + log''(1)(y-1)^2/2! + ...
//             == 0 + x - x^2/2 + x^3/3 ...
// The closer x is to 0, the fewer terms are required.
function MathLog1p(x) {
  if (!IS_NUMBER(x)) x = NonNumberToNumber(x);
  var xabs = MathAbs(x);
  if (xabs < 1E-7) {
    return x * (1 - x * (1/2));
  } else if (xabs < 3E-5) {
    return x * (1 - x * (1/2 - x * (1/3)));
  } else if (xabs < 7E-3) {
    return x * (1 - x * (1/2 - x * (1/3 - x * (1/4 -
           x * (1/5 - x * (1/6 - x * (1/7)))))));
  } else {  // Use regular log if not close enough to 0.
    return MathLog(1 + x);
  }
}


function ExtendMath() {
  %CheckIsBootstrapping();

  // Set up the non-enumerable functions on the Math object.
  InstallFunctions($Math, DONT_ENUM, $Array(
    "sign", MathSign,
    "trunc", MathTrunc,
    "sinh", MathSinh,
    "cosh", MathCosh,
    "tanh", MathTanh,
    "asinh", MathAsinh,
    "acosh", MathAcosh,
    "atanh", MathAtanh,
    "log10", MathLog10,
    "log2", MathLog2,
    "hypot", MathHypot,
    "fround", MathFround,
    "clz32", MathClz32,
    "cbrt", MathCbrt,
    "log1p", MathLog1p,
    "expm1", MathExpm1
  ));
}


ExtendMath();
