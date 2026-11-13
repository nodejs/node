// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

// Warm `outer` with throwing calls on doubles so its parameters get Number
// feedback without polluting `inner`'s AdditiveSafeInteger / Int32 feedback.

// Non-speculating Float64SpeculateSafeAdd (`range().IsSafeInt()`): `big` has a
// static range of up to 2^53 - 2 (2^53 - 2^31 + 2 in the call below) and is
// kept untruncated by `escape = big`, while `small` could wrap modulo 2^32.
// Since no range check is emitted on `big`, `GetMaxExactInputValue` must charge
// its full static range rather than clamping it to 2^50, refusing truncation of
// `small`. Otherwise `small` (-2^31 - 1) wraps by +2^32 to 2^31 - 1 and
// `big + small` becomes 2^53 + 1 (rounds to 2^53 -> 0) instead of
// 2^53 - 2^32 + 1 (-> 1).
(function TestNonSpeculatingSafeAdd() {
  var escape = 1.1;
  var should_throw = false;

  function inner(big, x) {
    if (should_throw) throw 1;
    let small = x + (-4294967296);
    let res = (big + small) | 0;
    escape = big;
    return res;
  }

  %PrepareFunctionForOptimization(inner);
  inner(0x100000000, 1);

  function outer(x, y) {
    x = x ^ 1;
    y = y ^ 1;
    let big = y + 9007197107257343;
    return inner(big, x);
  }

  %PrepareFunctionForOptimization(outer);
  should_throw = true;
  try { outer(1.5, 1.5); } catch {}
  should_throw = false;

  %OptimizeFunctionOnNextCall(outer);
  assertEquals(1, outer(2147483646, 2));
})();

// Partially truncated Float64Add `c = small + big_safe` (`small` truncated to
// Int32Add, `big_safe` kept Float64 by `escape = big_safe`) feeding into a
// speculating Float64SpeculateSafeAdd `(c + unchecked) | 0`: even after `c`'s
// cached static range is invalidated by partial truncation, `c` still has
// `can_truncate_to_int32()` set and holds its value only modulo 2^32, so the
// outer addition must still speculate and range-check `unchecked`.
(function TestPartiallyTruncatedInputIntoSpeculateSafeAdd() {
  var escape = 1.1;
  var should_throw = false;

  function inner(x, y, unchecked) {
    unchecked = unchecked * 1;
    if (should_throw) throw 1;
    let small = x + (-4294967296);
    let big_safe = y + 4294967296;
    let c = small + big_safe;
    let res = (c + unchecked) | 0;
    escape = big_safe;
    return res;
  }

  %PrepareFunctionForOptimization(inner);
  inner(1, 1, 0x100000000);

  function outer(x, y, unchecked) {
    x = x ^ 1;
    y = y ^ 1;
    return inner(x, y, unchecked);
  }

  %PrepareFunctionForOptimization(outer);
  should_throw = true;
  try { outer(1.5, 1.5, 1.5); } catch {}
  should_throw = false;

  %OptimizeFunctionOnNextCall(outer);
  assertEquals(-2147483648, outer(2147483646, 1, 1));
  assertEquals(1, outer(2147483646, 1, 9007192812290050));
})();
