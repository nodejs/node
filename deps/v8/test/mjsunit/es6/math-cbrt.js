// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

assertTrue(isNaN(Math.cbrt(NaN)));
assertTrue(isNaN(Math.cbrt(function() {})));
assertTrue(isNaN(Math.cbrt({ toString: function() { return NaN; } })));
assertTrue(isNaN(Math.cbrt({ valueOf: function() { return "abc"; } })));
assertEquals("Infinity", String(1/Math.cbrt(0)));
assertEquals("-Infinity", String(1/Math.cbrt(-0)));
assertEquals("Infinity", String(Math.cbrt(Infinity)));
assertEquals("-Infinity", String(Math.cbrt(-Infinity)));

for (var i = 1E-100; i < 1E100; i *= Math.PI) {
  assertEqualsDelta(i, Math.cbrt(i*i*i), i * 1E-15);
}

for (var i = -1E-100; i > -1E100; i *= Math.E) {
  assertEqualsDelta(i, Math.cbrt(i*i*i), -i * 1E-15);
}

// Let's be exact at least for small integers.
for (var i = 2; i < 10000; i++) {
  assertEquals(i, Math.cbrt(i*i*i));
}
