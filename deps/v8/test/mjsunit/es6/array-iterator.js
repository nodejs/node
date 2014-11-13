// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --harmony-tostring


var NONE = 0;
var READ_ONLY = 1;
var DONT_ENUM = 2;
var DONT_DELETE = 4;


function assertHasOwnProperty(object, name, attrs) {
  assertTrue(object.hasOwnProperty(name));
  var desc = Object.getOwnPropertyDescriptor(object, name);
  assertEquals(desc.writable, !(attrs & READ_ONLY));
  assertEquals(desc.enumerable, !(attrs & DONT_ENUM));
  assertEquals(desc.configurable, !(attrs & DONT_DELETE));
}


function TestArrayPrototype() {
  assertHasOwnProperty(Array.prototype, 'entries', DONT_ENUM);
  assertHasOwnProperty(Array.prototype, 'keys', DONT_ENUM);
  assertHasOwnProperty(Array.prototype, Symbol.iterator, DONT_ENUM);
}
TestArrayPrototype();


function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}


function TestValues() {
  var array = ['a', 'b', 'c'];
  var iterator = array[Symbol.iterator]();
  assertIteratorResult('a', false, iterator.next());
  assertIteratorResult('b', false, iterator.next());
  assertIteratorResult('c', false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());

  array.push('d');
  assertIteratorResult(void 0, true, iterator.next());
}
TestValues();


function TestValuesMutate() {
  var array = ['a', 'b', 'c'];
  var iterator = array[Symbol.iterator]();
  assertIteratorResult('a', false, iterator.next());
  assertIteratorResult('b', false, iterator.next());
  assertIteratorResult('c', false, iterator.next());
  array.push('d');
  assertIteratorResult('d', false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
}
TestValuesMutate();


function TestKeys() {
  var array = ['a', 'b', 'c'];
  var iterator = array.keys();
  assertIteratorResult(0, false, iterator.next());
  assertIteratorResult(1, false, iterator.next());
  assertIteratorResult(2, false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());

  array.push('d');
  assertIteratorResult(void 0, true, iterator.next());
}
TestKeys();


function TestKeysMutate() {
  var array = ['a', 'b', 'c'];
  var iterator = array.keys();
  assertIteratorResult(0, false, iterator.next());
  assertIteratorResult(1, false, iterator.next());
  assertIteratorResult(2, false, iterator.next());
  array.push('d');
  assertIteratorResult(3, false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
}
TestKeysMutate();


function TestEntries() {
  var array = ['a', 'b', 'c'];
  var iterator = array.entries();
  assertIteratorResult([0, 'a'], false, iterator.next());
  assertIteratorResult([1, 'b'], false, iterator.next());
  assertIteratorResult([2, 'c'], false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());

  array.push('d');
  assertIteratorResult(void 0, true, iterator.next());
}
TestEntries();


function TestEntriesMutate() {
  var array = ['a', 'b', 'c'];
  var iterator = array.entries();
  assertIteratorResult([0, 'a'], false, iterator.next());
  assertIteratorResult([1, 'b'], false, iterator.next());
  assertIteratorResult([2, 'c'], false, iterator.next());
  array.push('d');
  assertIteratorResult([3, 'd'], false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
}
TestEntriesMutate();


function TestArrayIteratorPrototype() {
  var array = [];
  var iterator = array.keys();

  var ArrayIteratorPrototype = iterator.__proto__;

  assertEquals(ArrayIteratorPrototype, array[Symbol.iterator]().__proto__);
  assertEquals(ArrayIteratorPrototype, array.keys().__proto__);
  assertEquals(ArrayIteratorPrototype, array.entries().__proto__);

  assertEquals(Object.prototype, ArrayIteratorPrototype.__proto__);

  assertEquals('Array Iterator', %_ClassOf(array[Symbol.iterator]()));
  assertEquals('Array Iterator', %_ClassOf(array.keys()));
  assertEquals('Array Iterator', %_ClassOf(array.entries()));

  assertFalse(ArrayIteratorPrototype.hasOwnProperty('constructor'));
  assertArrayEquals(['next'],
      Object.getOwnPropertyNames(ArrayIteratorPrototype));
  assertHasOwnProperty(ArrayIteratorPrototype, 'next', DONT_ENUM);
  assertHasOwnProperty(ArrayIteratorPrototype, Symbol.iterator, DONT_ENUM);

  assertEquals("[object Array Iterator]",
      Object.prototype.toString.call(iterator));
  assertEquals("Array Iterator", ArrayIteratorPrototype[Symbol.toStringTag]);
  var desc = Object.getOwnPropertyDescriptor(
      ArrayIteratorPrototype, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.writable);
  assertEquals("Array Iterator", desc.value);
}
TestArrayIteratorPrototype();


function TestForArrayValues() {
  var buffer = [];
  var array = [0, 'a', true, false, null, /* hole */, undefined, NaN];
  var i = 0;
  for (var value of array[Symbol.iterator]()) {
    buffer[i++] = value;
  }

  assertEquals(8, buffer.length);

  for (var i = 0; i < buffer.length; i++) {
    assertSame(array[i], buffer[i]);
  }
}
TestForArrayValues();


function TestForArrayKeys() {
  var buffer = [];
  var array = [0, 'a', true, false, null, /* hole */, undefined, NaN];
  var i = 0;
  for (var key of array.keys()) {
    buffer[i++] = key;
  }

  assertEquals(8, buffer.length);

  for (var i = 0; i < buffer.length; i++) {
    assertEquals(i, buffer[i]);
  }
}
TestForArrayKeys();


function TestForArrayEntries() {
  var buffer = [];
  var array = [0, 'a', true, false, null, /* hole */, undefined, NaN];
  var i = 0;
  for (var entry of array.entries()) {
    buffer[i++] = entry;
  }

  assertEquals(8, buffer.length);

  for (var i = 0; i < buffer.length; i++) {
    assertSame(array[i], buffer[i][1]);
  }

  for (var i = 0; i < buffer.length; i++) {
    assertEquals(i, buffer[i][0]);
  }
}
TestForArrayEntries();


function TestForArray() {
  var buffer = [];
  var array = [0, 'a', true, false, null, /* hole */, undefined, NaN];
  var i = 0;
  for (var value of array) {
    buffer[i++] = value;
  }

  assertEquals(8, buffer.length);

  for (var i = 0; i < buffer.length; i++) {
    assertSame(array[i], buffer[i]);
  }
}
TestForArrayValues();


function TestNonOwnSlots() {
  var array = [0];
  var iterator = array[Symbol.iterator]();
  var object = {__proto__: iterator};

  assertThrows(function() {
    object.next();
  }, TypeError);
}
TestNonOwnSlots();
