// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = 0;
var my_array_proto = {};
my_array_proto.__proto__ = [].__proto__;
Object.defineProperty(my_array_proto, "0", {
get: function() { return "get " + v; },
set: function(value) { v += value; }
});


// Test that element accessors are called in standard push cases.
array = [];
array.__proto__ = my_array_proto;

array[0] = 10;
assertEquals(0, array.length);
assertEquals(10, v);
assertEquals("get 10", array[0]);

Array.prototype.push.call(array, 100);
assertEquals(1, array.length);
assertEquals(110, v);
assertEquals("get 110", array[0]);

array = [];
array.__proto__ = my_array_proto;

assertEquals(0, array.length);
array.push(110);
assertEquals(1, array.length);
assertEquals(220, v);
assertEquals("get 220", array[0]);

// Test that elements setters/getters on prototype chain are property detected
// and don't lead to overzealous optimization.
v = 0;
function push_wrapper_1(array, value) {
  array.push(value);
}
array = [];
array.__proto__ = my_array_proto;
push_wrapper_1(array, 100);
assertEquals(1, array.length);
assertEquals(100, v);
push_wrapper_1(array, 100);
assertEquals(2, array.length);
assertEquals(100, v);
assertEquals("get 100", array[0]);
%OptimizeFunctionOnNextCall(push_wrapper_1);
array = [];
array.__proto__ = my_array_proto;
push_wrapper_1(array, 100);
assertEquals(1, array.length);
assertEquals(200, v);
assertEquals("get 200", array[0]);
