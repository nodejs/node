// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(a, i) {
  return a[i] + 0.5;
}
var arr = [,0.0,2.5];
%PrepareFunctionForOptimization(f1);
assertEquals(0.5, f1(arr, 1));
assertEquals(0.5, f1(arr, 1));
%OptimizeFunctionOnNextCall(f1);
assertEquals(0.5, f1(arr, 1));

// Trick crankshaft into accepting feedback with the array prototype
// map even though a call on that map was never made. optopush should
// refuse to inline the push call based on the danger that it's modifying
// the array prototype.
var push = Array.prototype.push;
var array_prototype = Array.prototype;

function optopush(a) {
  push.call(a, 1);
}

function foo() {
  optopush(array_prototype);
}

%PrepareFunctionForOptimization(foo);
optopush([]);
optopush([]);
optopush([]);
%OptimizeFunctionOnNextCall(foo);
foo();
assertEquals(1.5, f1(arr, 0));
