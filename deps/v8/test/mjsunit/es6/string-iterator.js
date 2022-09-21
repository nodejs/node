// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function TestStringPrototypeIterator() {
  assertTrue(String.prototype.hasOwnProperty(Symbol.iterator));
  assertFalse("".hasOwnProperty(Symbol.iterator));
  assertFalse("".propertyIsEnumerable(Symbol.iterator));
}
TestStringPrototypeIterator();


function assertIteratorResult(value, done, result) {
  assertEquals({value: value, done: done}, result);
}


function TestManualIteration() {
  var string = "abc";
  var iterator = string[Symbol.iterator]();
  assertIteratorResult('a', false, iterator.next());
  assertIteratorResult('b', false, iterator.next());
  assertIteratorResult('c', false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
}
TestManualIteration();


function TestSurrogatePairs() {
  var lo = "\uD834";
  var hi = "\uDF06";
  var pair = lo + hi;
  var string = "abc" + pair + "def" + lo + pair + hi + lo;
  var iterator = string[Symbol.iterator]();
  assertIteratorResult('a', false, iterator.next());
  assertIteratorResult('b', false, iterator.next());
  assertIteratorResult('c', false, iterator.next());
  assertIteratorResult(pair, false, iterator.next());
  assertIteratorResult('d', false, iterator.next());
  assertIteratorResult('e', false, iterator.next());
  assertIteratorResult('f', false, iterator.next());
  assertIteratorResult(lo, false, iterator.next());
  assertIteratorResult(pair, false, iterator.next());
  assertIteratorResult(hi, false, iterator.next());
  assertIteratorResult(lo, false, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
  assertIteratorResult(void 0, true, iterator.next());
}
TestSurrogatePairs();


function TestStringIteratorPrototype() {
  var iterator = ""[Symbol.iterator]();
  var StringIteratorPrototype = iterator.__proto__;
  assertFalse(StringIteratorPrototype.hasOwnProperty('constructor'));
  assertSame(StringIteratorPrototype.__proto__.__proto__, Object.prototype);
  assertArrayEquals(['next'],
      Object.getOwnPropertyNames(StringIteratorPrototype));
  assertEquals('[object String Iterator]', "" + iterator);
  assertEquals("String Iterator", StringIteratorPrototype[Symbol.toStringTag]);
  var desc = Object.getOwnPropertyDescriptor(
      StringIteratorPrototype, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.writable);
  assertEquals("String Iterator", desc.value);
}
TestStringIteratorPrototype();


function TestForOf() {
  var lo = "\uD834";
  var hi = "\uDF06";
  var pair = lo + hi;
  var string = "abc" + pair + "def" + lo + pair + hi + lo;
  var expected = ['a', 'b', 'c', pair, 'd', 'e', 'f', lo, pair, hi, lo];

  var i = 0;
  for (var char of string) {
    assertEquals(expected[i++], char);
  }

  assertEquals(expected.length, i);
}
TestForOf();


function TestNonOwnSlots() {
  var iterator = ""[Symbol.iterator]();
  var object = {__proto__: iterator};

  assertThrows(function() { object.next(); }, TypeError);
}
TestNonOwnSlots();


function TestSlicedStringRegression() {
  var long_string = "abcdefhijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  var sliced_string = long_string.substring(1);
  var iterator = sliced_string[Symbol.iterator]();
}
TestSlicedStringRegression();


(function(){
  var str = "\uD83C\uDF1F\u5FCD\u8005\u306E\u653B\u6483\uD83C\uDF1F";
  var arr = [...str];
  assertEquals(["\uD83C\uDF1F", "\u5FCD", "\u8005", "\u306E", "\u653B",
                "\u6483", "\uD83C\uDF1F"], arr);
  assertEquals(7, arr.length);
})();
