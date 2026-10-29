// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

result = Infinity;
function foo() {
  let carried = 1.1;
  let value = 4.07;
  for (let i = 0; i < 5; i++) {
    let shifted = 1.1 >> value;
    carried = value;
    value = Infinity;
  }
  result = carried;
}
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
assertEquals(Infinity, result);
