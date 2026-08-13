// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev
// Flags: --no-maglev-optimistic-peeled-loops

function bar() {}
%NeverOptimizeFunction(bar);

function foo() {
  for (let i = 0; i < 5; i++) {

    for (let j = 0; j < 5; j++) {
       %OptimizeOsr();
    }

    for (let k = 0; k < 5;) {
      typeof k;
      k = NaN;
      bar() ?? bar;
      function inner() { }
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo();
