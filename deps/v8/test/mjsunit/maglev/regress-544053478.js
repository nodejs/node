// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --maglev-non-eager-inlining --maglev-disable-builtin-reducers

let dummy;

function inner(a, b) {
  try {
    return Math.floor(a / b);
  } catch (e) {}
}

function outer() {
  return inner();
}

%PrepareFunctionForOptimization(outer);
inner("4");
outer();
%OptimizeMaglevOnNextCall(outer);
assertEquals(NaN, outer());
