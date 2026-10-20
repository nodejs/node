// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation --turbolev-future

function outer() {
  function inner(a, b) {
    for (let i = 0; i < 5; i++) {
      i++;
      Math.min(b, i) >>> 2;
      b = a;
    }
    return outer;
  }
  const result = inner();
  inner(31960, inner);
  return result;
}

%PrepareFunctionForOptimization(outer);
outer();
outer();
%OptimizeMaglevOnNextCall(outer);
%PrepareFunctionForOptimization(outer());
%OptimizeFunctionOnNextCall(outer());
outer();
