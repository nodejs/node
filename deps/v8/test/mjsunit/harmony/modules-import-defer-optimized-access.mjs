// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval --allow-natives-syntax --turbofan

// Regression test for the optimizing compilers folding deferred module
// namespace reads into direct cell loads (src/compiler/access-info.cc). The
// fast path must (a) preserve lazy evaluation -- importing a deferred module
// must not evaluate it, and the first property access must trigger evaluation
// exactly once -- and (b) read the correct values once optimized.

globalThis.eval_list = [];

import defer * as ns from './modules-skip-import-defer-optimized-access.mjs';

// Importing a deferred module must not evaluate it.
assertEquals(0, globalThis.eval_list.length);

function read() {
  return ns.value + ns.other;
}

// The first property access triggers synchronous evaluation, exactly once.
assertEquals(42 + 7, read());
assertArrayEquals(['defer-optimized'], globalThis.eval_list);

// Warm up the inline caches now that the module is evaluated, then optimize.
// This exercises the fast deferred-namespace cell-load path in optimized code.
%PrepareFunctionForOptimization(read);
read();
read();
%OptimizeFunctionOnNextCall(read);
assertEquals(42 + 7, read());
assertOptimized(read);

// Optimized reads must return correct values without re-evaluating the module.
assertEquals(42 + 7, read());
assertArrayEquals(['defer-optimized'], globalThis.eval_list);
