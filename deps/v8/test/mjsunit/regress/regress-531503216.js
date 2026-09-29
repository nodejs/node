// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// When a PlainPrimitiveToNumber result feeds an additive-safe-integer add whose
// result is truncated to word32, the conversion must keep the safe-integer
// check: ToNumber(undefined) is NaN, so (NaN + C) | 0 must be 0. Truncating
// undefined straight to word32 0 would drop the check and forge an integer.
const C = 0x40101000;
const TK = -0x40101000;

function f(route, stop) {
  let n = +(route ? TK : undefined);
  if (stop) return 0;
  return (n + C) | 0;
}

%PrepareFunctionForOptimization(f);
// Warm the ToNumber with undefined once, returning before the add so the add
// only ever sees in-range integers and keeps its additive-safe-integer type.
f(false, true);
for (let i = 0; i < 200; i++) f(true, false);
%OptimizeFunctionOnNextCall(f);

// undefined now flows into the add: the result must be 0, not a forged integer.
assertEquals(0, f(false, false));

// Same invariant for the sibling JSToNumber lowering: an effectful ToNumber
// (object with valueOf) that yields NaN must keep the safe-integer check too.
let vv = TK;
const obj = { valueOf() { return vv; } };

function g(route, stop) {
  let n = +(route ? TK : obj);
  if (stop) return 0;
  return (n + C) | 0;
}

%PrepareFunctionForOptimization(g);
g(false, true);
for (let i = 0; i < 200; i++) g(true, false);
%OptimizeFunctionOnNextCall(g);

vv = NaN;
assertEquals(0, g(false, false));
