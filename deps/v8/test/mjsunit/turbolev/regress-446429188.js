// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function wrap(f) {
  try {
    return f();
  } catch (e) {}
}

function bar(f) {
  try {
    wrap(f);
  } catch (e) {}
}

function foo() {
  for (let i = 0; i < 5; i++) {
    bar();
    const s = "p";
    s[4] |= i;
  }
}
%PrepareFunctionForOptimization(wrap);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo(foo());
%OptimizeFunctionOnNextCall(foo);
foo();
