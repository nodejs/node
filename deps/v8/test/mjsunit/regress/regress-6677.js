// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Assignment to const variable inside with-statement should fail.
const x = 0;
assertThrows(() => { with ({}) { x = 1; } }, TypeError);
assertEquals(0, x);

assertThrows(() => { with ({}) { eval("x = 1"); } }, TypeError);
assertEquals(0, x);

// Assignment to name of named function expression inside with-statement
// should not throw (but also not succeed).
assertEquals('function', function f() {
  with ({}) { f = 1 }
  return typeof f;
}());

// But we should throw an exception if the assignment is itself in strict
// code.
assertEquals('function', function f() {
  with ({}) {
    assertThrows(function() { "use strict"; f = 1 }, TypeError);
  }
  return typeof f;
}());
