// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Check calling a class constructor via Reflect.apply.
const c = class C { };
function newC(arg1) {
  return Reflect.apply(c, arg1, arguments);
}
%PrepareFunctionForOptimization(newC);
assertThrows(newC, TypeError);
assertThrows(newC, TypeError);
%OptimizeFunctionOnNextCall(newC);
assertThrows(newC, TypeError);

// Check calling a class constructor with forwarded rest arguments to closure.
function newD(...args) {
  class D {}
  D(...args);
}
%PrepareFunctionForOptimization(newD);
assertThrows(newD, TypeError);
assertThrows(newD, TypeError);
%OptimizeFunctionOnNextCall(newD);
assertThrows(newD, TypeError);
