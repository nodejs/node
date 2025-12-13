// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar() {}
let o = {x:1, y: 2};
function foo() {
  let s;
  bar()
  for (let x in o) {
    s += x;
  }
  return s;
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
assertEquals("undefinedxy", foo());
