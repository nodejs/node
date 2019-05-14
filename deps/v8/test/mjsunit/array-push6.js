// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function push_wrapper(array, value) {
  array.push(value);
}

// Test that optimization of Array.push() for non-Arrays works correctly.
var object = { x : 8, length: 3 };
object[18] = 5;
object.__proto__ = Array.prototype;
push_wrapper(object, 1);
push_wrapper(object, 1);
assertEquals(5, object.length);
%OptimizeFunctionOnNextCall(push_wrapper);
push_wrapper(object, 1);
push_wrapper(object, 1);
assertEquals(8, object.x);
assertEquals(7, object.length);
