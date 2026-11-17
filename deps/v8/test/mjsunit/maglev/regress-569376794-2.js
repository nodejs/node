// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TestDeepFunctionPrototypeCallReduction() {
  const c = Function.prototype.call;
  const g = () => {};
  const f = eval(`(() => (i) => i ? 0 : c.call(${'c,'.repeat(25000)}g))()`);
  %PrepareFunctionForOptimization(f);
  f(0);
  %OptimizeMaglevOnNextCall(f, 'concurrent');
  f(1);
  %FinalizeOptimization();
})();
