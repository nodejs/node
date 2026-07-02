// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-turbofan

function do_substring(str, start, end) {
  return str.substring(start, end);
}

const s = "abcdefg";
%PrepareFunctionForOptimization(do_substring);
for (let i = 0; i < 10; i++) {
  assertEquals(s, do_substring(s, 0, s.length));
  assertEquals("bc", do_substring(s, 1, 3));
}
%OptimizeMaglevOnNextCall(do_substring);

// Test optimized code.
assertEquals(s, do_substring(s, 0, s.length));
assertEquals("bc", do_substring(s, 1, 3));

// Edge cases.
assertEquals("f", do_substring(s, 5, 6));
assertEquals("abc", do_substring(s, 3, 0)); // end <= start (swaps)
assertEquals(s, do_substring(s, 0, 100)); // clamp end to length
assertEquals("ab", do_substring(s, -100, 2)); // clamp start to 0
assertEquals("bc", do_substring(s, 3, 1)); // swap and clamp

// Test with 1 argument
function do_substring_1(str, start) {
  return str.substring(start);
}

%PrepareFunctionForOptimization(do_substring_1);
for (let i = 0; i < 10; i++) {
  assertEquals(s.substring(1), do_substring_1(s, 1));
}
%OptimizeMaglevOnNextCall(do_substring_1);

assertEquals(s.substring(1), do_substring_1(s, 1));
assertEquals(s.substring(-1), do_substring_1(s, -1)); // clamp to 0
assertEquals(s.substring(100), do_substring_1(s, 100)); // clamp to length

// We should still be in optimized code.
assertTrue(isMaglevved(do_substring));
assertTrue(isMaglevved(do_substring_1));

// Passing something else than a string deopts.
assertEquals("x", do_substring({substring: function() { return "x";}}, 0, 1));
assertFalse(isMaglevved(do_substring));
