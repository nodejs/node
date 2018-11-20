// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function tests() {
  "use strict"
  function countArgs() { return arguments.length; }

  // Array params
  assertEquals(3, countArgs(...[1, 2, 3]));                       // Smi
  assertEquals(4, countArgs(...[1, 2, , 3]));                     // HoleySmi
  assertEquals(3, countArgs(...[1.1, 2, 3]));                     // Double
  assertEquals(4, countArgs(...[1.1, 2, , 3]));                   // HoleyDouble
  assertEquals(3, countArgs(...[{valueOf: () => 0}, 1.1, '2']));  // Object
  assertEquals(
      4, countArgs(...[{valueOf: () => 0}, 1.1, , '2']));  // HoleyObject

  // Smi param
  assertThrows(() => countArgs(...1), TypeError);

  // Object param
  assertThrows(() => countArgs(...{0: 0}), TypeError);

  // Strict arguments
  assertEquals(0, countArgs(...arguments));
}

tests();
tests();
%OptimizeFunctionOnNextCall(tests);
tests();

function testRest(...args) {
  function countArgs() { return arguments.length; }
  assertEquals(3, countArgs(...args));
  assertEquals(4, countArgs(1, ...args));
  assertEquals(5, countArgs(1, 2, ...args));
}
testRest(1, 2, 3);
testRest(1, 2, 3);
%OptimizeFunctionOnNextCall(testRest);
testRest(1, 2, 3);

function testRestAndArgs(a, b, ...args) {
  function countArgs() { return arguments.length; }
  assertEquals(1, countArgs(...args));
  assertEquals(2, countArgs(b, ...args));
  assertEquals(3, countArgs(a, b, ...args));
  assertEquals(4, countArgs(1, a, b, ...args));
  assertEquals(5, countArgs(1, 2, a, b, ...args));
}
testRestAndArgs(1, 2, 3);
testRestAndArgs(1, 2, 3);
%OptimizeFunctionOnNextCall(testRestAndArgs);
testRestAndArgs(1, 2, 3);

function testArgumentsStrict() {
  "use strict"
  function countArgs() { return arguments.length; }
  assertEquals(3, countArgs(...arguments));
  assertEquals(4, countArgs(1, ...arguments));
  assertEquals(5, countArgs(1, 2, ...arguments));
}
testArgumentsStrict(1, 2, 3);
testArgumentsStrict(1, 2, 3);
%OptimizeFunctionOnNextCall(testArgumentsStrict);
testArgumentsStrict(1, 2, 3);

function testArgumentsSloppy() {
  function countArgs() { return arguments.length; }
  assertEquals(3, countArgs(...arguments));
  assertEquals(4, countArgs(1, ...arguments));
  assertEquals(5, countArgs(1, 2, ...arguments));
}
testArgumentsSloppy(1, 2, 3);
testArgumentsSloppy(1, 2, 3);
%OptimizeFunctionOnNextCall(testArgumentsSloppy);
testArgumentsSloppy(1, 2, 3);
