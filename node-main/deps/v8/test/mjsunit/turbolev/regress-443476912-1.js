// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --no-lazy-feedback-allocation
// Flags: --turbolev --turbolev-non-eager-inlining

function bar(f, permissive = true) {
  try {
    return f();
  } catch (e) {
  }
}

function foo() {
  const x = bar(() => 0.427);
  const y = bar(() => x);
  "foo"[y];
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
