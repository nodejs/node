// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test fast div and mod.

function divmod(div_func, mod_func, x, y) {
  var div_answer = (div_func)(x);
  assertEquals(x / y, div_answer, x + "/" + y);
  var mod_answer = (mod_func)(x);
  assertEquals(x % y, mod_answer, x + "%" + y);
  var minus_div_answer = (div_func)(-x);
  assertEquals(-x / y, minus_div_answer, "-" + x + "/" + y);
  var minus_mod_answer = (mod_func)(-x);
  assertEquals(-x % y, minus_mod_answer, "-" + x + "%" + y);
}


function run_tests_for(divisor) {
  print("(function(left) { return left / " + divisor + "; })");
  var div_func = this.eval("(function(left) { return left / " + divisor + "; })");
  var mod_func = this.eval("(function(left) { return left % " + divisor + "; })");
  var exp;
  // Strange number test.
  divmod(div_func, mod_func, 0, divisor);
  divmod(div_func, mod_func, 1 / 0, divisor);
  // Floating point number test.
  for (exp = -1024; exp <= 1024; exp += 8) {
    divmod(div_func, mod_func, Math.pow(2, exp), divisor);
    divmod(div_func, mod_func, 0.9999999 * Math.pow(2, exp), divisor);
    divmod(div_func, mod_func, 1.0000001 * Math.pow(2, exp), divisor);
  }
  // Integer number test.
  for (exp = 0; exp <= 32; exp++) {
    divmod(div_func, mod_func, 1 << exp, divisor);
    divmod(div_func, mod_func, (1 << exp) + 1, divisor);
    divmod(div_func, mod_func, (1 << exp) - 1, divisor);
  }
  divmod(div_func, mod_func, Math.floor(0x1fffffff / 3), divisor);
  divmod(div_func, mod_func, Math.floor(-0x20000000 / 3), divisor);
}


var divisors = [
  0,
  1,
  2,
  3,
  4,
  5,
  6,
  7,
  8,
  9,
  10,
  0x1000000,
  0x40000000,
  12,
  60,
  100,
  1000 * 60 * 60 * 24];

for (var i = 0; i < divisors.length; i++) {
  run_tests_for(divisors[i]);
}

// Test extreme corner cases of modulo.

// Computes the modulo by slow but lossless operations.
function compute_mod(dividend, divisor) {
  // Return NaN if either operand is NaN, if divisor is 0 or
  // dividend is an infinity. Return dividend if divisor is an infinity.
  if (isNaN(dividend) || isNaN(divisor) || divisor == 0) { return NaN; }
  var sign = 1;
  if (dividend < 0) { dividend = -dividend; sign = -1; }
  if (dividend == Infinity) { return NaN; }
  if (divisor < 0) { divisor = -divisor; }
  if (divisor == Infinity) { return sign * dividend; }
  function rec_mod(a, b) {
    // Subtracts maximal possible multiplum of b from a.
    if (a >= b) {
      a = rec_mod(a, 2 * b);
      if (a >= b) { a -= b; }
    }
    return a;
  }
  return sign * rec_mod(dividend, divisor);
}

(function () {
  var large_non_smi = 1234567891234.12245;
  var small_non_smi = 43.2367243;
  var repeating_decimal = 0.3;
  var finite_decimal = 0.5;
  var smi = 43;
  var power_of_two = 64;
  var min_normal = Number.MIN_VALUE * Math.pow(2, 52);
  var max_denormal = Number.MIN_VALUE * (Math.pow(2, 52) - 1);

  // All combinations of NaN, Infinity, normal, denormal and zero.
  var example_numbers = [
    NaN,
    0,
    Number.MIN_VALUE,
    3 * Number.MIN_VALUE,
    max_denormal,
    min_normal,
    repeating_decimal,
    finite_decimal,
    smi,
    power_of_two,
    small_non_smi,
    large_non_smi,
    Number.MAX_VALUE,
    Infinity
  ];

  function doTest(a, b) {
    var exp = compute_mod(a, b);
    var act = a % b;
    assertEquals(exp, act, a + " % " + b);
  }

  for (var i = 0; i < example_numbers.length; i++) {
    for (var j = 0; j < example_numbers.length; j++) {
      var a = example_numbers[i];
      var b = example_numbers[j];
      doTest(a,b);
      doTest(-a,b);
      doTest(a,-b);
      doTest(-a,-b);
    }
  }
})();


(function () {
  // Edge cases
  var zero = 0;
  var minsmi32 = -0x40000000;
  var minsmi64 = -0x80000000;
  var somenum = 3532;
  assertEquals(-0, zero / -1, "0 / -1");
  assertEquals(1, minsmi32 / -0x40000000, "minsmi/minsmi-32");
  assertEquals(1, minsmi64 / -0x80000000, "minsmi/minsmi-64");
  assertEquals(somenum, somenum % -0x40000000, "%minsmi-32");
  assertEquals(somenum, somenum % -0x80000000, "%minsmi-64");
})();
