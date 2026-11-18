// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis

function* foo() {
  for (let i = 0; i < 4; i++) {
    const o = { y: "abc" };
    for (let j = 0; j < 5; j++) {
      o.y = {};
    }
    if (i < 1) {
      o.y = "def";
    }
    for (let k = 0; k < 2; k++) {
      for (let c = 0; c;) {
        yield;
      }
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo().next();
%OptimizeFunctionOnNextCall(foo);
foo().next();
