// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing

function foo() {
  Object.defineProperty(this, "x", {
    configurable: true,
    enumerable: true,
    set: assertUnreachable,
  });
}

class C extends foo {
  x = 153;
}

%PrepareFunctionForOptimization(C);
var c = new C();
assertEquals(Object.getOwnPropertyDescriptor(c, "x").value, 153);
%OptimizeFunctionOnNextCall(C);
var c = new C();
assertEquals(Object.getOwnPropertyDescriptor(c, "x").value, 153);
