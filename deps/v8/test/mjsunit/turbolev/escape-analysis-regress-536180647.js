// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-range-analysis
// Flags: --turbolev --turbolev-escape-analysis --no-maglev

function foo() {
  for (let i = 0; i < 5; i++) {
    %OptimizeOsr();
    const x = i ?? 10;
    Promise.resolve(x);
    function inner() { }
  }
}

%PrepareFunctionForOptimization(foo);
foo();
