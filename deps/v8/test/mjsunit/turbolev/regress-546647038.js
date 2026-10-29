// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-lazy-feedback-allocation

const max = Math.max;
let limit;

function foo(x) {
  const empty = [];
  let i = 0;
  for (; i < limit; i++) {
  }
  return max(x, i, ...empty);
}

assertEquals(NaN, foo());
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
