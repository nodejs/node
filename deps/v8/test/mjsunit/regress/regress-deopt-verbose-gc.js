// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-deopt-verbose
// Flags: --deopt-every-n-times=1 --stress-compaction

for (let k = 0; k < 25; k++) {
  const fn = new Function(
      'x', 'const c' + k + ' = String.fromCharCode(x); return c' + k + ';');
  %PrepareFunctionForOptimization(fn);
  fn(65);
  fn(66);
  %OptimizeFunctionOnNextCall(fn);
  fn(65);
  const res = fn({
    valueOf() {
      return 67;
    }
  });
  assertEquals('C', res);
}
