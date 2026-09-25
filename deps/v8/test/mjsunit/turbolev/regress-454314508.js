// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --turbolev-non-eager-inlining --turbolev --turbofan

function wrap(f, permissive = true) { // The default param is load-bearing
  try {
    return f();
  } catch (e) {
  }
}
%PrepareFunctionForOptimization(wrap);

let result = undefined;

function prettyPrint(value) {
  let hash = 0;
  for (let i = 0; i < 1; i++) {
    hash = hash << 5; // This is load-bearing.
  }
  result = value;
};
%PrepareFunctionForOptimization(prettyPrint);

prettyPrint({});

global = {};

function foo(v1) {
  let v2 = v1 | 0;
  let v3 = wrap(() => 3 - v2);
  global++;
  const v4 = wrap(() => v3 + v3);
  prettyPrint(v4);
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo(2147483648);
assertEquals(4294967302, result);
