// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  Error();
}

class A {}
class B extends A {
    constructor() {
        glb = new.target;        // -> new target is constant
        super();                 // -> call BuildInlinedAllocation
        foo();                   // -> trigger CHECK statement
        return 1;                // -> remove all uses of `this`
    }
}
var glb = B;
%PrepareFunctionForOptimization(B);
%PrepareFunctionForOptimization(foo);
assertThrows(()=>new B());
%OptimizeMaglevOnNextCall(B);
assertThrows(()=>new B());
