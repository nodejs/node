// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-assert --single-threaded
// Flags: --turbolev --turbolev-escape-analysis

function foo() {
  for (let i = 0; i < 5; i++) {
    const o = {};
    function inner_fun() { }
    const o_alias = [inner_fun, o][1];
    for (let j = 0; j < 5; j++) { }
    "p".slice(o_alias);
        %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(foo);
foo();
