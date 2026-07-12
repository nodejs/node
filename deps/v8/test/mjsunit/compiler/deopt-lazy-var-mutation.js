// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file

// Flags: --allow-natives-syntax

var x = 1;
var y = 1;

function g(a) {
 x = a;
 y = a;
}
%NeverOptimizeFunction(g);

function foo(a) {
 g(a);
 return x + y;
}

var o = 1;
%PrepareFunctionForOptimization(foo);
assertEquals(foo(o), 2);
assertEquals(foo(o), 2);
%OptimizeFunctionOnNextCall(foo);
o = 2;
assertEquals(foo(o), 4);
