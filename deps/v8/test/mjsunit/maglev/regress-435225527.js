// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-non-eager-inlining
// Flags: --max-maglev-inlined-bytecode-size-small=0
// Flags: --no-lazy-feedback-allocation

function f1() {
  try {
    return function (a, b) {
      return a + b;
    };
  } catch (e) {}
}
function f2(a, b,) {
  return f1()(a, b);
}

%PrepareFunctionForOptimization(f2);
f2();
f2();
%OptimizeMaglevOnNextCall(f2);
f2();
