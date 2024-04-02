// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function foo() {
  const arr = Array(1000);

  function bar() {
    try { ({a: p4nda, b: arr.length}); } catch(e) {}
  }

  for (var i = 0; i < 25; i++) bar();

  /p4nda/.test({});  // Deopt here.

  arr.shift();
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
