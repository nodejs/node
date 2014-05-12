// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = 0;

// Test that elements setters/getters on prototype chain set after the fact are
// property detected and don't lead to overzealous optimization.
var my_array_proto = {};
my_array_proto.__proto__ = [].__proto__;

function push_wrapper_2(array, value) {
  array.push(value);
}
array = [];
array.__proto__ = my_array_proto;
push_wrapper_2(array, 66);
assertEquals(1, array.length);
assertEquals(0, v);
assertEquals(66, array[0]);
push_wrapper_2(array, 77);
assertEquals(2, array.length);
assertEquals(0, v);
assertEquals(77, array[1]);
%OptimizeFunctionOnNextCall(push_wrapper_2);
push_wrapper_2(array, 88);
assertEquals(3, array.length);
assertEquals(0, v);
assertEquals(88, array[2]);
assertOptimized(push_wrapper_2);
// Defining accessor should deopt optimized push.
Object.defineProperty(my_array_proto, "3", {
get: function() { return "get " + v; },
set: function(value) { v += value; }
});
assertUnoptimized(push_wrapper_2);
push_wrapper_2(array, 99);
assertEquals(4, array.length);
assertEquals(99, v);
assertEquals("get 99", array[3]);
