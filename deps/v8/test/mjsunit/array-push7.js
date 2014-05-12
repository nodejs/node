// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var v = 0;

function push_wrapper(array, value) {
  array.push(value);
}
function pop_wrapper(array) {
  return array.pop();
}

// Test that Object.observe() notification events are properly sent from
// Array.push() and Array.pop() both from optimized and un-optimized code.
var array = [];

function somethingChanged(changes) {
  v++;
}

Object.observe(array, somethingChanged);
push_wrapper(array, 1);
%RunMicrotasks();
assertEquals(1, array.length);
assertEquals(1, v);
push_wrapper(array, 1);
%RunMicrotasks();
assertEquals(2, array.length);
assertEquals(2, v);
%OptimizeFunctionOnNextCall(push_wrapper);
push_wrapper(array, 1);
%RunMicrotasks();
assertEquals(3, array.length);
assertEquals(3, v);
push_wrapper(array, 1);
%RunMicrotasks();
assertEquals(4, array.length);
assertEquals(4, v);

pop_wrapper(array);
%RunMicrotasks();
assertEquals(3, array.length);
assertEquals(5, v);
pop_wrapper(array);
%RunMicrotasks();
assertEquals(2, array.length);
assertEquals(6, v);
%OptimizeFunctionOnNextCall(pop_wrapper);
pop_wrapper(array);
%RunMicrotasks();
assertEquals(1, array.length);
assertEquals(7, v);
pop_wrapper(array);
%RunMicrotasks();
assertEquals(0, array.length);
assertEquals(8, v);
