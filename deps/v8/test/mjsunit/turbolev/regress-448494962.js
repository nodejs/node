// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function C1() { this.a = undefined; } // .a wil be at offset 12
let x = new C1();
x.b = undefined; // .b will be at offset  16

function C2() { }
let y = new C2();
y.a = undefined; // This will make C2 be initialized with a filler at offset 12

function foo() {
  let o1 = new C1();
  let o2 = new C2();
  o1.b = undefined; // This will be at offset 16

  return [ o1, o2 ]; // To inspect return values and prevent escape analysis
}


%PrepareFunctionForOptimization(C1);
%PrepareFunctionForOptimization(C2);
%PrepareFunctionForOptimization(foo);
foo();
foo();

%OptimizeFunctionOnNextCall(foo);
let [o1, o2] = foo();
assertEquals(undefined, o1.b);
