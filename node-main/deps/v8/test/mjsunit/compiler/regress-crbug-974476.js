// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function use(x) { return x; }
%NeverOptimizeFunction(use);

function foo() {
  let result = undefined;
  (function () {
    const a = {};
    for (_ of [0]) {
      const empty = [];
      (function () {
        result = 42;
        for (_ of [0]) {
          for (_ of [0]) {
            use(empty);
          }
        }
        result = a;
      })();
    }
  })();
  return result;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
