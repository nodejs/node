// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap --turbo-store-elimination

// Check that transitioning stores are not eliminated.

let obj = { a: 42 }

function foo() {
  // Force GC on the next allocation to trigger heap verification.
  %SimulateNewspaceFull();

  // Transitioning store. Must not be eliminated.
  this.f = obj;

  this.f = {
    a: 43
  };
}

%PrepareFunctionForOptimization(foo);
var a;
a = new foo();
a = new foo();
%OptimizeFunctionOnNextCall(foo);
a = new foo();
assertEquals(43, a.f.a);
