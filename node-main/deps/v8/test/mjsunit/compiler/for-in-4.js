// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

// Ensure that we properly check for properties on the prototypes.
function foo(o) {
  var s = "";
  for (var i in o) s += i;
  return s;
}

var a = [];
%PrepareFunctionForOptimization(foo);
assertEquals("", foo(a));
assertEquals("", foo(a));
%OptimizeFunctionOnNextCall(foo);
assertEquals("", foo(a));
Array.prototype.x = 4;
assertEquals("x", foo(a));
