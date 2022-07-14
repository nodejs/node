// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test calling a class constructor on a polymorphic object throws a TypeError.
function f(o) {
  o.get();
}

let obj = new Map();
%PrepareFunctionForOptimization(f);
f(obj);
f(obj);

obj.get = class C {};
assertThrows(() => f(obj), TypeError);
%OptimizeFunctionOnNextCall(f);
assertThrows(() => f(obj), TypeError);

// Test calling a closure of a class constructor throws a TypeError.
function g(a) {
  var f;
  f = class {};
  if (a == 1) {
    f = function() {};
  }
  f();
}

%PrepareFunctionForOptimization(g);
assertThrows(g, TypeError);
assertThrows(g, TypeError);
%OptimizeFunctionOnNextCall(g);
assertThrows(g, TypeError);
