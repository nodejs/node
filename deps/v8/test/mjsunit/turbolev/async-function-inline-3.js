// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

let count = 0;

async function baz() { count++; }
%NeverOptimizeFunction(baz);

async function bar() {
  for (let j = 0; j < 42; j++) {
    if (j > 15 && j < 25) {
      await baz();
    } else if (j > 33) {
      await baz();
    }
  }
}

async function foo() {
  for (let i = 0; i < 42; i++) {
    if (i % 2 == 0) {
      await bar();
    }
  }
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo().then(function() {
  assertEquals(357, count);
  count = 0;
      %OptimizeFunctionOnNextCall(foo)
  return foo()
}).then(() => assertEquals(357, count));
