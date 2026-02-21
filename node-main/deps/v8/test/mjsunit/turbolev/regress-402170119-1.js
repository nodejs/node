// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

function bar() {
  return bar;
}
class C extends bar {
  constructor(x) {
    return x;
  }
}

function foo() {
  new C(-8);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(foo);
assertThrows(() => foo(), TypeError,
             "Derived constructors may only return object or undefined");

%OptimizeFunctionOnNextCall(foo);
assertThrows(() => foo(), TypeError,
             "Derived constructors may only return object or undefined");
