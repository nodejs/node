// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax  --no-lazy-feedback-allocation
// Flags: --maglev-non-eager-inlining --max-maglev-inlined-bytecode-size-small=0

function wrap(f) {
  return f();
}

function foo(value) {
  return (value instanceof RegExp);
};

function bar() {
  let arr = wrap(() => []);
  arr.__proto__ = RegExp();
  return foo(arr);
}

%PrepareFunctionForOptimization(bar);
bar();
bar();
%OptimizeMaglevOnNextCall(bar);
assertTrue(bar());
