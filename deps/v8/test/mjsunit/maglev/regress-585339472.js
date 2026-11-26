// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Keep enough int32 values live to exhaust allocatable registers on x64, and
// use idx2 and cp as shift counts (fixed rcx on x64) so that the codePointAt
// result register is allocated to the same register as its index input.
function foo(s, idx, a, b, c, d, e, f, g, h, i, j, k, l, m, z) {
  let s_len = s.length;
  let v1 = a + 1;
  let v2 = b + 2;
  let v3 = c + 3;
  let v4 = d + 4;
  let v5 = e + 5;
  let v6 = f + 6;
  let v7 = g + 7;
  let v8 = h + 8;
  let v9 = i + 9;
  let v10 = j + 10;
  let v11 = k + 11;
  let v12 = l + 12;
  let v13 = m + 13;
  let idx2 = idx + 1;
  let sh = v1 << idx2;
  let cp = s.codePointAt(idx2);
  let sh2 = sh << cp;
  let dummy = sh2 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 +
              v12 + v13 + s_len;
  return cp + (dummy & z);
}

function test(expected, str) {
  assertEquals(
      expected, foo(str, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0));
}

%PrepareFunctionForOptimization(foo);
test(0x10000, "a\uD800\uDC00");
%OptimizeFunctionOnNextCall(foo);
test(0x10000, "a\uD800\uDC00");
test(0xD800, "a\uD800");
test(0xD800, "a\uD800b");
