// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// A function with a for-in loop, that will be optimized.
function foo(o) {
  for (var i in o) {
    return o[i];
  }
}

var o = { x: 0.5 };

// Warm up foo with Double values in the enum cache.
%PrepareFunctionForOptimization(foo);
assertEquals(foo(o), 0.5);
assertEquals(foo(o), 0.5);
%OptimizeFunctionOnNextCall(foo);
assertEquals(foo(o), 0.5);

// Transition the double field to a tagged field
o.x = "abc";

// Make sure that the optimized code correctly loads the tagged field.
assertEquals(foo(o), "abc");
