// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --max-maglev-hard-inline-depth=2200
// Flags: --max-maglev-inlined-bytecode-size-small-total=30000

(function TestDeepNestedForEach() {
  let body = '';
  for (let i = 0; i < 250; i++) {
    body = `arr.forEach(() => { ${body} });`;
  }
  const f = new Function('arr', body);
  %PrepareFunctionForOptimization(f);
  f([1]);
  %OptimizeMaglevOnNextCall(f, 'concurrent');
  f([1]);
  %FinalizeOptimization();
})();

(function TestMutuallyRecursiveForEachClosures() {
  let f;
  for (let i = 0; i < 3; i++) {
    const prev = f, a = i ? [1] : [];
    f = () => a.forEach(() => prev());
  }
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeMaglevOnNextCall(f, 'concurrent');
  f();
  %FinalizeOptimization();
})();
