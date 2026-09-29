// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis
// Flags: --no-maglev-optimistic-peeled-loops
function foo() {
  for (let i = 0; i < 5; i++) {
    const o = {};
    o.x = i;
    ({
      "f": i,
    } = "c");
    function bar() {}
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
