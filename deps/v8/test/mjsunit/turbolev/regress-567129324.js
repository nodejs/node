// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --max-turbolev-eager-inlined-bytecode-size=0

// The graph optimizer must not replace a TruncateChecked...ToInt32[Number]
// with a cached truncation that only assumed NumberOrOddball: the Number check
// narrows `x` (Smi|Boolean) to Smi, which the Smi-field store relies on.
// `alt` must not be eagerly inlined, so that its truncation is only added to
// the graph by the inliner, after `x | y` has been built.

function alt(x) {
  return Math.imul(x, 3);
}

const obj = { b: 123 };

function f(flag, y, obj) {
  const x = flag ? true : 123;
  const a = alt(x);
  const b = x | y;
  obj.b = x;
  return b + a;
}

%PrepareFunctionForOptimization(alt);
%PrepareFunctionForOptimization(f);
f(false, 0.125, obj);
%OptimizeFunctionOnNextCall(f);
assertEquals(123 + 369, f(false, 0.125, obj));
assertOptimized(f);

assertEquals(1 + 3, f(true, 0.125, obj));
assertUnoptimized(f);
assertEquals(true, obj.b);
%HeapObjectVerify(obj);
