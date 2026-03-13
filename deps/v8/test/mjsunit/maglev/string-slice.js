// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-turbofan

function do_slice(str, start, end) {
  return str.slice(start, end);
}

const s = "abcdefg";
%PrepareFunctionForOptimization(do_slice);
// Warmup to collect type feedback
for (let i = 0; i < 10; i++) {
  assertEquals(s, do_slice(s, 0, s.length));
  assertEquals("bc", do_slice(s, 1, 3));
}
%OptimizeMaglevOnNextCall(do_slice);

// Test optimized code.
assertEquals(s, do_slice(s, 0, s.length));
assertEquals("bc", do_slice(s, 1, 3));

// Edge cases.
assertEquals("fg", do_slice(s, -2, s.length)); // start < 0
assertEquals("f", do_slice(s, -2, -1)); // start < 0, end < 0
assertEquals("", do_slice(s, 5, 2)); // end <= start
assertEquals(s, do_slice(s, 0, 100)); // clamp end
assertEquals("ab", do_slice(s, -100, 2)); // clamp start
assertEquals( "", do_slice(s, -1, 0)); // empty

// Test with different string.
const s2 = "12345";
assertEquals("234", do_slice(s2, 1, 4));

// We should still be in optimized code.
assertTrue(isMaglevved(do_slice));

// Passing something else than a string deopts.
assertEquals("x", do_slice({slice: function() { return "x";}}));
assertFalse(isMaglevved(do_slice));
