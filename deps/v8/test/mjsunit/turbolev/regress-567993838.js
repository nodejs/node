// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
//
// Inlining `inner(x, 0.5)` into `outer` feeds a non-safe-int constant `0.5`
// into `c + b` while both additions carry `AdditiveSafeInteger` feedback.
// Backward truncation propagation must reject `c + 0.5` and its input `x + 1`.

let should_throw = false;
function inner(x, b) {
  if (should_throw) throw 1;
  let c = x + 1;
  return (c + b) | 0;
}

%PrepareFunctionForOptimization(inner);
inner(0x100000000, 0x100000000);

function outer(x) {
  return inner(x, 0.5);
}

%PrepareFunctionForOptimization(outer);
should_throw = true;
try { outer(10); } catch {}
should_throw = false;

%OptimizeFunctionOnNextCall(outer);
assertEquals(11, outer(10));
assertEquals(-2147483648, outer(-2147483650));
