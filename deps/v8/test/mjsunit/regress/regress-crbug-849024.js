// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

this.__proto__ = new Proxy({}, {});

function test1() {
  eval("bla");
}

assertThrows(test1, ReferenceError);
assertThrows(test1, ReferenceError);
assertThrows(test1, ReferenceError);

function test2() {
  gc();
  eval("bla");
}

assertThrows(test2, ReferenceError);
assertThrows(test2, ReferenceError);
assertThrows(test2, ReferenceError);

function foo() {
  try {
    eval("bla");
  } catch(e) {
    return;
  }
  throw 1337;
}

function test3() {
  gc();
  foo();
  foo();
}

assertDoesNotThrow(test3);
assertDoesNotThrow(test3);
assertDoesNotThrow(test3);
