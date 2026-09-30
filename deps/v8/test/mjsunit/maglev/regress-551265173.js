// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert

function bar() {
  throw 0;
}
%NeverOptimizeFunction(bar);

async function foo() {
  for (let i = 0; i < 2; i++) {
    try {
      bar(i);
    } catch {
      [i];
      while (i > 10) {
        await 0;
      }
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeMaglevOnNextCall(foo);
foo();
