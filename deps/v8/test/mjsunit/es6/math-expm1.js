// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-fast-math

assertTrue(isNaN(Math.expm1(NaN)));
assertTrue(isNaN(Math.expm1(function() {})));
assertTrue(isNaN(Math.expm1({ toString: function() { return NaN; } })));
assertTrue(isNaN(Math.expm1({ valueOf: function() { return "abc"; } })));
assertEquals("Infinity", String(1/Math.expm1(0)));
assertEquals("-Infinity", String(1/Math.expm1(-0)));
assertEquals("Infinity", String(Math.expm1(Infinity)));
assertEquals(-1, Math.expm1(-Infinity));

for (var x = 0.1; x < 700; x += 0.1) {
  var expected = Math.exp(x) - 1;
  assertEqualsDelta(expected, Math.expm1(x), expected * 1E-14);
  expected = Math.exp(-x) - 1;
  assertEqualsDelta(expected, Math.expm1(-x), -expected * 1E-14);
}

// Values close to 0:
// Use six terms of Taylor expansion at 0 for exp(x) as test expectation:
// exp(x) - 1 == exp(0) + exp(0) * x + x * x / 2 + ... - 1
//            == x + x * x / 2 + x * x * x / 6 + ...
function expm1(x) {
  return x * (1 + x * (1/2 + x * (
              1/6 + x * (1/24 + x * (
              1/120 + x * (1/720 + x * (
              1/5040 + x * (1/40320 + x*(
              1/362880 + x * (1/3628800))))))))));
}

for (var x = 1E-1; x > 1E-300; x *= 0.8) {
  var expected = expm1(x);
  assertEqualsDelta(expected, Math.expm1(x), expected * 1E-14);
}
