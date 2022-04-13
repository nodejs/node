// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

async function foo() {
  for (let i = 0; i < 1; i++) {
    while (i !== i) {
      await 42;
      function unused() {}
      return;
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
