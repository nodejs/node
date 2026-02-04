// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* bar() {
    yield* 1;
}

function foo() {
  try {
    bar();
  } catch (e) {}
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
