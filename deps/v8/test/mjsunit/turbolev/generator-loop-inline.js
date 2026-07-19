// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* gen() {
  for (let i = 0; i < 42; i++) {
    if (i > 15) {
      yield i;
    }
  }
}

function foo() {
  return gen();
}

%PrepareFunctionForOptimization(gen);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
