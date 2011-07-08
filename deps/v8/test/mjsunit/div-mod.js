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

// Flags: --allow-natives-syntax

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


// Side-effect-free expressions containing bit operations use
// an optimized compiler with int32 values.   Ensure that modulus
// produces negative zeros correctly.
function negative_zero_modulus_test() {
  var x = 4;
  var y = -4;
  x = x + x - x;
  y = y + y - y;
  var z = (y | y | y | y) % x;
  assertEquals(-1 / 0, 1 / z);
  z = (x | x | x | x) % x;
  assertEquals(1 / 0, 1 / z);
  z = (y | y | y | y) % y;
  assertEquals(-1 / 0, 1 / z);
  z = (x | x | x | x) % y;
  assertEquals(1 / 0, 1 / z);
}

negative_zero_modulus_test();


function lithium_integer_mod() {
  var left_operands = [
    0,
    305419896,  // 0x12345678
  ];

  // Test the standard lithium code for modulo opeartions.
  var mod_func;
  for (var i = 0; i < left_operands.length; i++) {
    for (var j = 0; j < divisors.length; j++) {
      mod_func = this.eval("(function(left) { return left % " + divisors[j]+ "; })");
      assertEquals((mod_func)(left_operands[i]), left_operands[i] % divisors[j]);
      assertEquals((mod_func)(-left_operands[i]), -left_operands[i] % divisors[j]);
    }
  }

  var results_powers_of_two = [
    // 0
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    // 305419896 == 0x12345678
    [0, 0, 0, 8, 24, 56, 120, 120, 120, 632, 1656, 1656, 5752, 5752, 22136, 22136, 22136, 22136, 284280, 284280, 1332856, 3430008, 3430008, 3430008, 3430008, 36984440, 36984440, 36984440, 305419896, 305419896, 305419896],
  ];

  // Test the lithium code for modulo operations with a variable power of two
  // right hand side operand.
  for (var i = 0; i < left_operands.length; i++) {
    for (var j = 0; j < 31; j++) {
      assertEquals(results_powers_of_two[i][j], left_operands[i] % (2 << j));
      assertEquals(results_powers_of_two[i][j], left_operands[i] % -(2 << j));
      assertEquals(-results_powers_of_two[i][j], -left_operands[i] % (2 << j));
      assertEquals(-results_powers_of_two[i][j], -left_operands[i] % -(2 << j));
    }
  }

  // Test the lithium code for modulo operations with a constant power of two
  // right hand side operand.
  for (var i = 0; i < left_operands.length; i++) {
    // With positive left hand side operand.
    assertEquals(results_powers_of_two[i][0], left_operands[i] % -(2 << 0));
    assertEquals(results_powers_of_two[i][1], left_operands[i] % (2 << 1));
    assertEquals(results_powers_of_two[i][2], left_operands[i] % -(2 << 2));
    assertEquals(results_powers_of_two[i][3], left_operands[i] % (2 << 3));
    assertEquals(results_powers_of_two[i][4], left_operands[i] % -(2 << 4));
    assertEquals(results_powers_of_two[i][5], left_operands[i] % (2 << 5));
    assertEquals(results_powers_of_two[i][6], left_operands[i] % -(2 << 6));
    assertEquals(results_powers_of_two[i][7], left_operands[i] % (2 << 7));
    assertEquals(results_powers_of_two[i][8], left_operands[i] % -(2 << 8));
    assertEquals(results_powers_of_two[i][9], left_operands[i] % (2 << 9));
    assertEquals(results_powers_of_two[i][10], left_operands[i] % -(2 << 10));
    assertEquals(results_powers_of_two[i][11], left_operands[i] % (2 << 11));
    assertEquals(results_powers_of_two[i][12], left_operands[i] % -(2 << 12));
    assertEquals(results_powers_of_two[i][13], left_operands[i] % (2 << 13));
    assertEquals(results_powers_of_two[i][14], left_operands[i] % -(2 << 14));
    assertEquals(results_powers_of_two[i][15], left_operands[i] % (2 << 15));
    assertEquals(results_powers_of_two[i][16], left_operands[i] % -(2 << 16));
    assertEquals(results_powers_of_two[i][17], left_operands[i] % (2 << 17));
    assertEquals(results_powers_of_two[i][18], left_operands[i] % -(2 << 18));
    assertEquals(results_powers_of_two[i][19], left_operands[i] % (2 << 19));
    assertEquals(results_powers_of_two[i][20], left_operands[i] % -(2 << 20));
    assertEquals(results_powers_of_two[i][21], left_operands[i] % (2 << 21));
    assertEquals(results_powers_of_two[i][22], left_operands[i] % -(2 << 22));
    assertEquals(results_powers_of_two[i][23], left_operands[i] % (2 << 23));
    assertEquals(results_powers_of_two[i][24], left_operands[i] % -(2 << 24));
    assertEquals(results_powers_of_two[i][25], left_operands[i] % (2 << 25));
    assertEquals(results_powers_of_two[i][26], left_operands[i] % -(2 << 26));
    assertEquals(results_powers_of_two[i][27], left_operands[i] % (2 << 27));
    assertEquals(results_powers_of_two[i][28], left_operands[i] % -(2 << 28));
    assertEquals(results_powers_of_two[i][29], left_operands[i] % (2 << 29));
    assertEquals(results_powers_of_two[i][30], left_operands[i] % -(2 << 30));
    // With negative left hand side operand.
    assertEquals(-results_powers_of_two[i][0], -left_operands[i] % -(2 << 0));
    assertEquals(-results_powers_of_two[i][1], -left_operands[i] % (2 << 1));
    assertEquals(-results_powers_of_two[i][2], -left_operands[i] % -(2 << 2));
    assertEquals(-results_powers_of_two[i][3], -left_operands[i] % (2 << 3));
    assertEquals(-results_powers_of_two[i][4], -left_operands[i] % -(2 << 4));
    assertEquals(-results_powers_of_two[i][5], -left_operands[i] % (2 << 5));
    assertEquals(-results_powers_of_two[i][6], -left_operands[i] % -(2 << 6));
    assertEquals(-results_powers_of_two[i][7], -left_operands[i] % (2 << 7));
    assertEquals(-results_powers_of_two[i][8], -left_operands[i] % -(2 << 8));
    assertEquals(-results_powers_of_two[i][9], -left_operands[i] % (2 << 9));
    assertEquals(-results_powers_of_two[i][10], -left_operands[i] % -(2 << 10));
    assertEquals(-results_powers_of_two[i][11], -left_operands[i] % (2 << 11));
    assertEquals(-results_powers_of_two[i][12], -left_operands[i] % -(2 << 12));
    assertEquals(-results_powers_of_two[i][13], -left_operands[i] % (2 << 13));
    assertEquals(-results_powers_of_two[i][14], -left_operands[i] % -(2 << 14));
    assertEquals(-results_powers_of_two[i][15], -left_operands[i] % (2 << 15));
    assertEquals(-results_powers_of_two[i][16], -left_operands[i] % -(2 << 16));
    assertEquals(-results_powers_of_two[i][17], -left_operands[i] % (2 << 17));
    assertEquals(-results_powers_of_two[i][18], -left_operands[i] % -(2 << 18));
    assertEquals(-results_powers_of_two[i][19], -left_operands[i] % (2 << 19));
    assertEquals(-results_powers_of_two[i][20], -left_operands[i] % -(2 << 20));
    assertEquals(-results_powers_of_two[i][21], -left_operands[i] % (2 << 21));
    assertEquals(-results_powers_of_two[i][22], -left_operands[i] % -(2 << 22));
    assertEquals(-results_powers_of_two[i][23], -left_operands[i] % (2 << 23));
    assertEquals(-results_powers_of_two[i][24], -left_operands[i] % -(2 << 24));
    assertEquals(-results_powers_of_two[i][25], -left_operands[i] % (2 << 25));
    assertEquals(-results_powers_of_two[i][26], -left_operands[i] % -(2 << 26));
    assertEquals(-results_powers_of_two[i][27], -left_operands[i] % (2 << 27));
    assertEquals(-results_powers_of_two[i][28], -left_operands[i] % -(2 << 28));
    assertEquals(-results_powers_of_two[i][29], -left_operands[i] % (2 << 29));
    assertEquals(-results_powers_of_two[i][30], -left_operands[i] % -(2 << 30));
  }

}

lithium_integer_mod();
%OptimizeFunctionOnNextCall(lithium_integer_mod)
lithium_integer_mod();
