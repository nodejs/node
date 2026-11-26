// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --max-optimized-bytecode-size=300000

function count(...xs) {
  return xs.length;
}

// JSCallForwardVarargs with arity >= 2^15 (target + receiver + 32766
// arguments).
(function TestCallForwardVarargsLargeArity() {
  const args = new Array(32766).fill(0).join(',');
  const f = new Function(`return count(${args}, ...arguments);`);
  %PrepareFunctionForOptimization(f);
  assertEquals(32768, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(32768, f(1, 2));
})();

// JSCallForwardVarargs with start_index >= 2^15 (rest parameter after
// 32768 formal parameters).
(function TestCallForwardVarargsLargeStartIndex() {
  let params = '';
  for (let i = 0; i < 32768; i++) params += 'a' + i + ',';
  const f = new Function(params + '...rest', 'return count(...rest);');
  %PrepareFunctionForOptimization(f);
  assertEquals(0, f(1, 2));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f(1, 2));
})();
