// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

function inline(arg0, arg1) {
    arg1[1] = 13.37;
}
function foo() {
    const obj = {
        a: 1.1,             // allocate HeapNumber
        b: [1.1, 2.2, 3.3], // allocate JSArray, allocate backing store
    };
  const alloc = Array(0);
  inline(obj, alloc);
  inline(obj, alloc);
}

%PrepareFunctionForOptimization(inline);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
