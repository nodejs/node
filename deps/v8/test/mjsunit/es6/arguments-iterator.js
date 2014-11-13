// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


// Note in general that "arguments.foo" and "var o = arguments; o.foo"
// are treated differently by full-codegen, and so both cases need to be
// tested.

function TestDirectArgumentsIteratorProperty() {
  assertTrue(arguments.hasOwnProperty(Symbol.iterator));
  assertFalse(arguments.propertyIsEnumerable(Symbol.iterator));
  var descriptor = Object.getOwnPropertyDescriptor(arguments, Symbol.iterator);
  assertTrue(descriptor.writable);
  assertFalse(descriptor.enumerable);
  assertTrue(descriptor.configurable);
  assertEquals(descriptor.value, [][Symbol.iterator]);
  assertEquals(arguments[Symbol.iterator], [][Symbol.iterator]);
}
TestDirectArgumentsIteratorProperty();


function TestIndirectArgumentsIteratorProperty() {
  var o = arguments;
  assertTrue(o.hasOwnProperty(Symbol.iterator));
  assertFalse(o.propertyIsEnumerable(Symbol.iterator));
  assertEquals(o[Symbol.iterator], [][Symbol.iterator]);
}
TestIndirectArgumentsIteratorProperty();


function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}


function TestDirectValues1(a, b, c) {
  var iterator = arguments[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());
  assertIteratorResult(c, false, iterator.next());
  assertIteratorResult(undefined, true, iterator.next());
}
TestDirectValues1(1, 2, 3);


function TestIndirectValues1(a, b, c) {
  var args = arguments;
  var iterator = args[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());
  assertIteratorResult(c, false, iterator.next());
  assertIteratorResult(undefined, true, iterator.next());
}
TestIndirectValues1(1, 2, 3);


function TestDirectValues2(a, b, c) {
  var iterator = arguments[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());
  assertIteratorResult(c, false, iterator.next());
  assertIteratorResult(undefined, true, iterator.next());

  arguments[3] = 4;
  arguments.length = 4;
  assertIteratorResult(undefined, true, iterator.next());
}
TestDirectValues2(1, 2, 3);


function TestIndirectValues2(a, b, c) {
  var args = arguments;
  var iterator = args[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());
  assertIteratorResult(c, false, iterator.next());
  assertIteratorResult(undefined, true, iterator.next());

  arguments[3] = 4;
  arguments.length = 4;
  assertIteratorResult(undefined, true, iterator.next());
}
TestIndirectValues2(1, 2, 3);


function TestDirectValues3(a, b, c) {
  var iterator = arguments[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());

  arguments.length = 2;
  assertIteratorResult(undefined, true, iterator.next());
}
TestDirectValues3(1, 2, 3);


function TestIndirectValues3(a, b, c) {
  var args = arguments;
  var iterator = args[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());

  arguments.length = 2;
  assertIteratorResult(undefined, true, iterator.next());
}
TestIndirectValues3(1, 2, 3);


function TestDirectValues4(a, b, c) {
  var iterator = arguments[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());
  assertIteratorResult(c, false, iterator.next());

  arguments.length = 4;
  assertIteratorResult(undefined, false, iterator.next());
  assertIteratorResult(undefined, true, iterator.next());
}
TestDirectValues4(1, 2, 3);


function TestIndirectValues4(a, b, c) {
  var args = arguments;
  var iterator = args[Symbol.iterator]();
  assertIteratorResult(a, false, iterator.next());
  assertIteratorResult(b, false, iterator.next());
  assertIteratorResult(c, false, iterator.next());

  arguments.length = 4;
  assertIteratorResult(undefined, false, iterator.next());
  assertIteratorResult(undefined, true, iterator.next());
}
TestIndirectValues4(1, 2, 3);


function TestForOf() {
  var i = 0;
  for (var value of arguments) {
    assertEquals(arguments[i++], value);
  }

  assertEquals(arguments.length, i);
}
TestForOf(1, 2, 3, 4, 5);


function TestAssignmentToIterator() {
  var i = 0;
  arguments[Symbol.iterator] = [].entries;
  for (var entry of arguments) {
    assertEquals([i, arguments[i]], entry);
    i++;
  }

  assertEquals(arguments.length, i);
}
TestAssignmentToIterator(1, 2, 3, 4, 5);


function TestArgumentsMutation() {
  var i = 0;
  for (var x of arguments) {
    assertEquals(arguments[i], x);
    arguments[i+1] *= 2;
    i++;
  }

  assertEquals(arguments.length, i);
}
TestArgumentsMutation(1, 2, 3, 4, 5);


function TestSloppyArgumentsAliasing(a0, a1, a2, a3, a4) {
  var i = 0;
  for (var x of arguments) {
    assertEquals(arguments[i], x);
    a0 = a1; a1 = a2; a3 = a4;
    i++;
  }

  assertEquals(arguments.length, i);
}
TestSloppyArgumentsAliasing(1, 2, 3, 4, 5);


function TestStrictArgumentsAliasing(a0, a1, a2, a3, a4) {
  "use strict";
  var i = 0;
  for (var x of arguments) {
    a0 = a1; a1 = a2; a3 = a4;
    assertEquals(arguments[i], x);
    i++;
  }

  assertEquals(arguments.length, i);
}
TestStrictArgumentsAliasing(1, 2, 3, 4, 5);


function TestArgumentsAsProto() {
  "use strict";

  var o = {__proto__:arguments};
  assertSame([][Symbol.iterator], o[Symbol.iterator]);
  // Make o dict-mode.
  %OptimizeObjectForAddingMultipleProperties(o, 0);
  assertFalse(o.hasOwnProperty(Symbol.iterator));
  assertSame([][Symbol.iterator], o[Symbol.iterator]);
  o[Symbol.iterator] = 10;
  assertTrue(o.hasOwnProperty(Symbol.iterator));
  assertEquals(10, o[Symbol.iterator]);
  assertSame([][Symbol.iterator], arguments[Symbol.iterator]);

  // Frozen o.
  o = Object.freeze({__proto__:arguments});
  assertSame([][Symbol.iterator], o[Symbol.iterator]);
  assertFalse(o.hasOwnProperty(Symbol.iterator));
  assertSame([][Symbol.iterator], o[Symbol.iterator]);
  // This should throw, but currently it doesn't, because
  // ExecutableAccessorInfo callbacks don't see the current strict mode.
  // See note in accessors.cc:SetPropertyOnInstanceIfInherited.
  o[Symbol.iterator] = 10;
  assertFalse(o.hasOwnProperty(Symbol.iterator));
  assertEquals([][Symbol.iterator], o[Symbol.iterator]);
  assertSame([][Symbol.iterator], arguments[Symbol.iterator]);
}
TestArgumentsAsProto();
