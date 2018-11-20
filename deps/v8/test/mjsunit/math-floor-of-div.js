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

// Flags: --allow-natives-syntax --enable-sudiv

// Use this function as reference. Make sure it is not inlined.
function div(a, b) {
  return a / b;
}

var limit = 0x1000000;
var exhaustive_limit = 100;
var step = 10;
var values = [0x10000001,
              0x12345678,
              -0x789abcdf,  // 0x87654321
              0x01234567,
              0x76543210,
              -0x80000000,  // 0x80000000
              0x7fffffff,
              -0x0fffffff,  // 0xf0000001
              0x00000010,
              -0x01000000   // 0xff000000
              ];

function test_div() {
  var c = 0;
  for (var k = 0; k <= limit; k++) {
    if (k > exhaustive_limit) { c += step; k += c; }
    assertEquals(Math.floor(div(k,   1)), Math.floor(k /   1));
    assertEquals(Math.floor(div(k,   -1)), Math.floor(k /   -1));
    assertEquals(Math.floor(div(k,   2)), Math.floor(k /   2));
    assertEquals(Math.floor(div(k,   -2)), Math.floor(k /   -2));
    assertEquals(Math.floor(div(k,   3)), Math.floor(k /   3));
    assertEquals(Math.floor(div(k,   -3)), Math.floor(k /   -3));
    assertEquals(Math.floor(div(k,   4)), Math.floor(k /   4));
    assertEquals(Math.floor(div(k,   -4)), Math.floor(k /   -4));
    assertEquals(Math.floor(div(k,   5)), Math.floor(k /   5));
    assertEquals(Math.floor(div(k,   -5)), Math.floor(k /   -5));
    assertEquals(Math.floor(div(k,   6)), Math.floor(k /   6));
    assertEquals(Math.floor(div(k,   -6)), Math.floor(k /   -6));
    assertEquals(Math.floor(div(k,   7)), Math.floor(k /   7));
    assertEquals(Math.floor(div(k,   -7)), Math.floor(k /   -7));
    assertEquals(Math.floor(div(k,   8)), Math.floor(k /   8));
    assertEquals(Math.floor(div(k,   -8)), Math.floor(k /   -8));
    assertEquals(Math.floor(div(k,   9)), Math.floor(k /   9));
    assertEquals(Math.floor(div(k,   -9)), Math.floor(k /   -9));
    assertEquals(Math.floor(div(k,  10)), Math.floor(k /  10));
    assertEquals(Math.floor(div(k,  -10)), Math.floor(k /  -10));
    assertEquals(Math.floor(div(k,  11)), Math.floor(k /  11));
    assertEquals(Math.floor(div(k,  -11)), Math.floor(k /  -11));
    assertEquals(Math.floor(div(k,  12)), Math.floor(k /  12));
    assertEquals(Math.floor(div(k,  -12)), Math.floor(k /  -12));
    assertEquals(Math.floor(div(k,  13)), Math.floor(k /  13));
    assertEquals(Math.floor(div(k,  -13)), Math.floor(k /  -13));
    assertEquals(Math.floor(div(k,  14)), Math.floor(k /  14));
    assertEquals(Math.floor(div(k,  -14)), Math.floor(k /  -14));
    assertEquals(Math.floor(div(k,  15)), Math.floor(k /  15));
    assertEquals(Math.floor(div(k,  -15)), Math.floor(k /  -15));
    assertEquals(Math.floor(div(k,  16)), Math.floor(k /  16));
    assertEquals(Math.floor(div(k,  -16)), Math.floor(k /  -16));
    assertEquals(Math.floor(div(k,  17)), Math.floor(k /  17));
    assertEquals(Math.floor(div(k,  -17)), Math.floor(k /  -17));
    assertEquals(Math.floor(div(k,  18)), Math.floor(k /  18));
    assertEquals(Math.floor(div(k,  -18)), Math.floor(k /  -18));
    assertEquals(Math.floor(div(k,  19)), Math.floor(k /  19));
    assertEquals(Math.floor(div(k,  -19)), Math.floor(k /  -19));
    assertEquals(Math.floor(div(k,  20)), Math.floor(k /  20));
    assertEquals(Math.floor(div(k,  -20)), Math.floor(k /  -20));
    assertEquals(Math.floor(div(k,  21)), Math.floor(k /  21));
    assertEquals(Math.floor(div(k,  -21)), Math.floor(k /  -21));
    assertEquals(Math.floor(div(k,  22)), Math.floor(k /  22));
    assertEquals(Math.floor(div(k,  -22)), Math.floor(k /  -22));
    assertEquals(Math.floor(div(k,  23)), Math.floor(k /  23));
    assertEquals(Math.floor(div(k,  -23)), Math.floor(k /  -23));
    assertEquals(Math.floor(div(k,  24)), Math.floor(k /  24));
    assertEquals(Math.floor(div(k,  -24)), Math.floor(k /  -24));
    assertEquals(Math.floor(div(k,  25)), Math.floor(k /  25));
    assertEquals(Math.floor(div(k,  -25)), Math.floor(k /  -25));
    assertEquals(Math.floor(div(k, 125)), Math.floor(k / 125));
    assertEquals(Math.floor(div(k, -125)), Math.floor(k / -125));
    assertEquals(Math.floor(div(k, 625)), Math.floor(k / 625));
    assertEquals(Math.floor(div(k, -625)), Math.floor(k / -625));
  }
  c = 0;
  for (var k = 0; k <= limit; k++) {
    if (k > exhaustive_limit) { c += step; k += c; }
    assertEquals(Math.floor(div(-k,   1)), Math.floor(-k /   1));
    assertEquals(Math.floor(div(-k,   -1)), Math.floor(-k /   -1));
    assertEquals(Math.floor(div(-k,   2)), Math.floor(-k /   2));
    assertEquals(Math.floor(div(-k,   -2)), Math.floor(-k /   -2));
    assertEquals(Math.floor(div(-k,   3)), Math.floor(-k /   3));
    assertEquals(Math.floor(div(-k,   -3)), Math.floor(-k /   -3));
    assertEquals(Math.floor(div(-k,   4)), Math.floor(-k /   4));
    assertEquals(Math.floor(div(-k,   -4)), Math.floor(-k /   -4));
    assertEquals(Math.floor(div(-k,   5)), Math.floor(-k /   5));
    assertEquals(Math.floor(div(-k,   -5)), Math.floor(-k /   -5));
    assertEquals(Math.floor(div(-k,   6)), Math.floor(-k /   6));
    assertEquals(Math.floor(div(-k,   -6)), Math.floor(-k /   -6));
    assertEquals(Math.floor(div(-k,   7)), Math.floor(-k /   7));
    assertEquals(Math.floor(div(-k,   -7)), Math.floor(-k /   -7));
    assertEquals(Math.floor(div(-k,   8)), Math.floor(-k /   8));
    assertEquals(Math.floor(div(-k,   -8)), Math.floor(-k /   -8));
    assertEquals(Math.floor(div(-k,   9)), Math.floor(-k /   9));
    assertEquals(Math.floor(div(-k,   -9)), Math.floor(-k /   -9));
    assertEquals(Math.floor(div(-k,  10)), Math.floor(-k /  10));
    assertEquals(Math.floor(div(-k,  -10)), Math.floor(-k /  -10));
    assertEquals(Math.floor(div(-k,  11)), Math.floor(-k /  11));
    assertEquals(Math.floor(div(-k,  -11)), Math.floor(-k /  -11));
    assertEquals(Math.floor(div(-k,  12)), Math.floor(-k /  12));
    assertEquals(Math.floor(div(-k,  -12)), Math.floor(-k /  -12));
    assertEquals(Math.floor(div(-k,  13)), Math.floor(-k /  13));
    assertEquals(Math.floor(div(-k,  -13)), Math.floor(-k /  -13));
    assertEquals(Math.floor(div(-k,  14)), Math.floor(-k /  14));
    assertEquals(Math.floor(div(-k,  -14)), Math.floor(-k /  -14));
    assertEquals(Math.floor(div(-k,  15)), Math.floor(-k /  15));
    assertEquals(Math.floor(div(-k,  -15)), Math.floor(-k /  -15));
    assertEquals(Math.floor(div(-k,  16)), Math.floor(-k /  16));
    assertEquals(Math.floor(div(-k,  -16)), Math.floor(-k /  -16));
    assertEquals(Math.floor(div(-k,  17)), Math.floor(-k /  17));
    assertEquals(Math.floor(div(-k,  -17)), Math.floor(-k /  -17));
    assertEquals(Math.floor(div(-k,  18)), Math.floor(-k /  18));
    assertEquals(Math.floor(div(-k,  -18)), Math.floor(-k /  -18));
    assertEquals(Math.floor(div(-k,  19)), Math.floor(-k /  19));
    assertEquals(Math.floor(div(-k,  -19)), Math.floor(-k /  -19));
    assertEquals(Math.floor(div(-k,  20)), Math.floor(-k /  20));
    assertEquals(Math.floor(div(-k,  -20)), Math.floor(-k /  -20));
    assertEquals(Math.floor(div(-k,  21)), Math.floor(-k /  21));
    assertEquals(Math.floor(div(-k,  -21)), Math.floor(-k /  -21));
    assertEquals(Math.floor(div(-k,  22)), Math.floor(-k /  22));
    assertEquals(Math.floor(div(-k,  -22)), Math.floor(-k /  -22));
    assertEquals(Math.floor(div(-k,  23)), Math.floor(-k /  23));
    assertEquals(Math.floor(div(-k,  -23)), Math.floor(-k /  -23));
    assertEquals(Math.floor(div(-k,  24)), Math.floor(-k /  24));
    assertEquals(Math.floor(div(-k,  -24)), Math.floor(-k /  -24));
    assertEquals(Math.floor(div(-k,  25)), Math.floor(-k /  25));
    assertEquals(Math.floor(div(-k,  -25)), Math.floor(-k /  -25));
    assertEquals(Math.floor(div(-k, 125)), Math.floor(-k / 125));
    assertEquals(Math.floor(div(-k, -125)), Math.floor(-k / -125));
    assertEquals(Math.floor(div(-k, 625)), Math.floor(-k / 625));
    assertEquals(Math.floor(div(-k, -625)), Math.floor(-k / -625));
  }
  // Test for edge cases.
  // Use (values[key] | 0) to force the integer type.
  for (var i = 0; i < values.length; i++) {
    for (var j = 0; j < values.length; j++) {
      assertEquals(Math.floor(div((values[i] | 0), (values[j] | 0))),
                   Math.floor((values[i] | 0) / (values[j] | 0)));
      assertEquals(Math.floor(div(-(values[i] | 0), (values[j] | 0))),
                   Math.floor(-(values[i] | 0) / (values[j] | 0)));
      assertEquals(Math.floor(div((values[i] | 0), -(values[j] | 0))),
                   Math.floor((values[i] | 0) / -(values[j] | 0)));
      assertEquals(Math.floor(div(-(values[i] | 0), -(values[j] | 0))),
                   Math.floor(-(values[i] | 0) / -(values[j] | 0)));
    }
  }
}

test_div();
%OptimizeFunctionOnNextCall(test_div);
test_div();

// Test for flooring correctness.
var values2 = [1, 3, 10, 99, 100, 101, 0x7fffffff];
function test_div2() {
  for (var i = 0; i < values2.length; i++) {
    for (var j = 0; j < values2.length; j++) {
      assertEquals(Math.floor(div((values2[i] | 0), (values2[j] | 0))),
                   Math.floor((values2[i] | 0) / (values2[j] | 0)));
      assertEquals(Math.floor(div(-(values2[i] | 0), (values2[j] | 0))),
                   Math.floor(-(values2[i] | 0) / (values2[j] | 0)));
      assertEquals(Math.floor(div((values2[i] | 0), -(values2[j] | 0))),
                   Math.floor((values2[i] | 0) / -(values2[j] | 0)));
      assertEquals(Math.floor(div(-(values2[i] | 0), -(values2[j] | 0))),
                   Math.floor(-(values2[i] | 0) / -(values2[j] | 0)));
    }
  }
}

test_div2();
%OptimizeFunctionOnNextCall(test_div2);
test_div2();


// Test for negative zero, overflow and division by 0.
// Separate the tests to prevent deoptimizations from making the other optimized
// test unreachable.

// We box the value in an array to avoid constant propagation.
var neg_one_in_array = [-1];
var zero_in_array = [0];
var min_int_in_array = [-2147483648];

// Test for dividing by constant.
function IsNegativeZero(x) {
  assertTrue(x == 0);  // Is 0 or -0.
  var y = 1 / x;
  assertFalse(isFinite(y));
  return y < 0;
}

function test_div_deopt_minus_zero() {
  for (var i = 0; i < 2; ++i) {
    assertTrue(IsNegativeZero(Math.floor((zero_in_array[0] | 0) / -1)));
  }
}

function test_div_deopt_overflow() {
  for (var i = 0; i < 2; ++i) {
    // We use '| 0' to force the representation to int32.
    assertEquals(-min_int_in_array[0],
                 Math.floor((min_int_in_array[0] | 0) / -1));
  }
}

function test_div_deopt_div_by_zero() {
  for (var i = 0; i < 2; ++i) {
    assertEquals(div(i, 0),
                 Math.floor(i / 0));
  }
}

test_div_deopt_minus_zero();
test_div_deopt_overflow();
test_div_deopt_div_by_zero();
%OptimizeFunctionOnNextCall(test_div_deopt_minus_zero);
%OptimizeFunctionOnNextCall(test_div_deopt_overflow);
%OptimizeFunctionOnNextCall(test_div_deopt_div_by_zero);
test_div_deopt_minus_zero();
test_div_deopt_overflow();
test_div_deopt_div_by_zero();

// Test for dividing by variable.
function test_div_deopt_minus_zero_v() {
  for (var i = 0; i < 2; ++i) {
    assertTrue(IsNegativeZero(Math.floor((zero_in_array[0] | 0) /
               neg_one_in_array[0])));
  }
}

function test_div_deopt_overflow_v() {
  for (var i = 0; i < 2; ++i) {
    // We use '| 0' to force the representation to int32.
    assertEquals(-min_int_in_array[0],
                 Math.floor((min_int_in_array[0] | 0) / neg_one_in_array[0]));
  }
}

function test_div_deopt_div_by_zero_v() {
  for (var i = 0; i < 2; ++i) {
    assertEquals(div(i, 0),
                 Math.floor(i / zero_in_array[0]));
  }
}

test_div_deopt_minus_zero_v();
test_div_deopt_overflow_v();
test_div_deopt_div_by_zero_v();
%OptimizeFunctionOnNextCall(test_div_deopt_minus_zero_v);
%OptimizeFunctionOnNextCall(test_div_deopt_overflow_v);
%OptimizeFunctionOnNextCall(test_div_deopt_div_by_zero_v);
test_div_deopt_minus_zero_v();
test_div_deopt_overflow_v();
test_div_deopt_div_by_zero_v();


// Test for flooring division with negative dividend.
function flooring_div_by_3(y) {
  return Math.floor(y / 3);
}

assertEquals(-1, flooring_div_by_3(-2));
assertEquals(-1, flooring_div_by_3(-2));
%OptimizeFunctionOnNextCall(flooring_div_by_3);
assertEquals(-1, flooring_div_by_3(-2));
