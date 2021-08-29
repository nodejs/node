// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --interrupt-budget=1024

const x = {a: 4.2};

function foo() {
  const z = {a: [], b: NaN};
  function bar(arg) {
    const y = {a: 42};
    y.a = 42;
    arg.a = NaN;
    for (let i = 0; i < 10; i++) {
      [].toLocaleString();
    }
  }
  const obj = {e: {}};
  bar(obj);
  bar(obj);
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
