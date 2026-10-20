// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function foo() {
  function f() {}
  for (let j = 0; j < 5; j++) {
    let uninitialized;
    const a = [uninitialized][0];
    f["!"] = a;
    Array(a);
  }
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
assertUnoptimized(foo);
