// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --print-turbolev-frontend

let arr = [ {x: 1}, {x: 2}, {x: 3}, {x: 4} ];
function foo(a, b) {
  let sum = a + b | 0;
  let index = sum - 2147483645 | 0;
  let val = arr[index];
}
%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 20000; i++) {
  foo(1073741822, 1073741823);
}
%OptimizeFunctionOnNextCall(foo);
foo();
