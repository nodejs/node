// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C2 extends Object {
  constructor() {
    for (let i5 = 100;
         this;
         (() => {
           new Map(this);
         })()) {
         }
  }
}
class C9 extends C2 {
  constructor() {
    super();
  }
}

%PrepareFunctionForOptimization(C2);
%PrepareFunctionForOptimization(C9);
assertThrows(() => new C9(), ReferenceError,
             "Must call super constructor in derived class before accessing 'this' or returning from derived constructor");

%OptimizeFunctionOnNextCall(C9);
assertThrows(() => new C9(), ReferenceError,
             "Must call super constructor in derived class before accessing 'this' or returning from derived constructor");
