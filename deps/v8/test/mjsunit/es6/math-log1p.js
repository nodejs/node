// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-maths

assertTrue(isNaN(Math.log1p(NaN)));
assertTrue(isNaN(Math.log1p(function() {})));
assertTrue(isNaN(Math.log1p({ toString: function() { return NaN; } })));
assertTrue(isNaN(Math.log1p({ valueOf: function() { return "abc"; } })));
assertEquals("Infinity", String(1/Math.log1p(0)));
assertEquals("-Infinity", String(1/Math.log1p(-0)));
assertEquals("Infinity", String(Math.log1p(Infinity)));
assertEquals("-Infinity", String(Math.log1p(-1)));
assertTrue(isNaN(Math.log1p(-2)));
assertTrue(isNaN(Math.log1p(-Infinity)));

for (var x = 1E300; x > 1E-1; x *= 0.8) {
  var expected = Math.log(x + 1);
  assertEqualsDelta(expected, Math.log1p(x), expected * 1E-14);
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
  assertEqualsDelta(expected, Math.log1p(x), expected * 1E-14);
}
