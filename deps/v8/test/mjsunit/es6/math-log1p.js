// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(isNaN(Math.log1p(NaN)));
assertTrue(isNaN(Math.log1p(function() {})));
assertTrue(isNaN(Math.log1p({ toString: function() { return NaN; } })));
assertTrue(isNaN(Math.log1p({ valueOf: function() { return "abc"; } })));
assertEquals(Infinity, 1/Math.log1p(0));
assertEquals(-Infinity, 1/Math.log1p(-0));
assertEquals(Infinity, Math.log1p(Infinity));
assertEquals(-Infinity, Math.log1p(-1));
assertTrue(isNaN(Math.log1p(-2)));
assertTrue(isNaN(Math.log1p(-Infinity)));

for (var x = 1E300; x > 1E16; x *= 0.8) {
  var expected = Math.log(x + 1);
  assertEqualsDelta(expected, Math.log1p(x), expected * 1E-16);
}

// Values close to 0:
// Use Taylor expansion at 1 for log(x) as test expectation:
// log1p(x) == log(x + 1) == 0 + x / 1 - x^2 / 2 + x^3 / 3 - ...
function log1p(x) {
  var terms = [];
  var prod = x;
  for (var i = 1; i <= 20; i++) {
    terms.push(prod / i);
    prod *= -x;
  }
  var sum = 0;
  while (terms.length > 0) sum += terms.pop();
  return sum;
}

for (var x = 1E-1; x > 1E-300; x *= 0.8) {
  var expected = log1p(x);
  assertEqualsDelta(expected, Math.log1p(x), expected * 1E-16);
}

// Issue 3481.
assertEquals(6.9756137364252422e-03,
             Math.log1p(8070450532247929/Math.pow(2,60)));

// Tests related to the fdlibm implementation.
// Test largest double value.
assertEquals(709.782712893384, Math.log1p(1.7976931348623157e308));
// Test small values.
assertEquals(Math.pow(2, -55), Math.log1p(Math.pow(2, -55)));
assertEquals(9.313225741817976e-10, Math.log1p(Math.pow(2, -30)));
// Cover various code paths.
// -.2929 < x < .41422, k = 0
assertEquals(-0.2876820724517809, Math.log1p(-0.25));
assertEquals(0.22314355131420976, Math.log1p(0.25));
// 0.41422 < x < 9.007e15
assertEquals(2.3978952727983707, Math.log1p(10));
// x > 9.007e15
assertEquals(36.841361487904734, Math.log1p(10e15));
// Normalize u.
assertEquals(37.08337388996168, Math.log1p(12738099905822720));
// Normalize u/2.
assertEquals(37.08336444902049, Math.log1p(12737979646738432));
// |f| = 0, k != 0
assertEquals(1.3862943611198906, Math.log1p(3));
// |f| != 0, k != 0
assertEquals(1.3862945995384413, Math.log1p(3 + Math.pow(2,-20)));
// final if-clause: k = 0
assertEquals(0.5596157879354227, Math.log1p(0.75));
// final if-clause: k != 0
assertEquals(0.8109302162163288, Math.log1p(1.25));
