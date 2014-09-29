// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

Object.defineProperty(Array.prototype, "1", {
  get: function() { return "element 1"; },
  set: function(value) { }
});
function test(array) {
  array.shift();
  return array;
}
assertEquals(["element 1",2], test(["0",,2]));
assertEquals(["element 1",{}], test([{},,{}]));
%OptimizeFunctionOnNextCall(test);
assertEquals(["element 1",0], test([{},,0]));
