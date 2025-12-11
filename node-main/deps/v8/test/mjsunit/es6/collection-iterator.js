// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(f) {
  %PrepareFunctionForOptimization(f);
  f();
  f();
  %OptimizeFunctionOnNextCall(f);
  f();
}

test(function TestSetIterator() {
  var s = new Set;
  var iter = s.values();

  var SetIteratorPrototype = iter.__proto__;
  assertFalse(SetIteratorPrototype.hasOwnProperty('constructor'));
  assertSame(SetIteratorPrototype.__proto__.__proto__, Object.prototype);

  var propertyNames = Object.getOwnPropertyNames(SetIteratorPrototype);
  assertArrayEquals(['next'], propertyNames);

  assertSame(new Set().values().__proto__, SetIteratorPrototype);
  assertSame(new Set().entries().__proto__, SetIteratorPrototype);

  assertEquals("[object Set Iterator]",
      Object.prototype.toString.call(iter));
  assertEquals("Set Iterator", SetIteratorPrototype[Symbol.toStringTag]);
  var desc = Object.getOwnPropertyDescriptor(
      SetIteratorPrototype, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.writable);
  assertEquals("Set Iterator", desc.value);
});


test(function TestSetIteratorValues() {
  var s = new Set;
  s.add(1);
  s.add(2);
  s.add(3);
  var iter = s.values();

  assertEquals({value: 1, done: false}, iter.next());
  assertEquals({value: 2, done: false}, iter.next());
  assertEquals({value: 3, done: false}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
});


test(function TestSetIteratorKeys() {
  assertEquals(Set.prototype.keys, Set.prototype.values);
});


test(function TestSetIteratorEntries() {
  var s = new Set;
  s.add(1);
  s.add(2);
  s.add(3);
  var iter = s.entries();

  assertEquals({value: [1, 1], done: false}, iter.next());
  assertEquals({value: [2, 2], done: false}, iter.next());
  assertEquals({value: [3, 3], done: false}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
});


test(function TestSetIteratorMutations() {
  var s = new Set;
  s.add(1);
  var iter = s.values();
  assertEquals({value: 1, done: false}, iter.next());
  s.add(2);
  s.add(3);
  s.add(4);
  s.add(5);
  assertEquals({value: 2, done: false}, iter.next());
  s.delete(3);
  assertEquals({value: 4, done: false}, iter.next());
  s.delete(5);
  assertEquals({value: undefined, done: true}, iter.next());
  s.add(4);
  assertEquals({value: undefined, done: true}, iter.next());
});


test(function TestSetIteratorMutations2() {
  var s = new Set;
  s.add(1);
  s.add(2);
  var i = s.values();
  assertEquals({value: 1, done: false}, i.next());
  s.delete(2);
  s.delete(1);
  s.add(2);
  assertEquals({value: 2, done: false}, i.next());
  assertEquals({value: undefined, done: true}, i.next());
});


test(function TestSetIteratorMutations3() {
  var s = new Set;
  s.add(1);
  s.add(2);
  var i = s.values();
  assertEquals({value: 1, done: false}, i.next());
  s.delete(2);
  s.delete(1);
  for (var x = 2; x < 500; ++x) s.add(x);
  for (var x = 2; x < 500; ++x) s.delete(x);
  for (var x = 2; x < 1000; ++x) s.add(x);
  assertEquals({value: 2, done: false}, i.next());
  for (var x = 1001; x < 2000; ++x) s.add(x);
  s.delete(3);
  for (var x = 6; x < 2000; ++x) s.delete(x);
  assertEquals({value: 4, done: false}, i.next());
  s.delete(5);
  assertEquals({value: undefined, done: true}, i.next());
  s.add(4);
  assertEquals({value: undefined, done: true}, i.next());
});


test(function TestSetInvalidReceiver() {
  assertThrows(function() {
    Set.prototype.values.call({});
  }, TypeError);
  assertThrows(function() {
    Set.prototype.entries.call({});
  }, TypeError);
});


test(function TestSetIteratorInvalidReceiver() {
  var iter = new Set().values();
  assertThrows(function() {
    iter.next.call({});
  });
});


test(function TestSetIteratorSymbol() {
  assertEquals(Set.prototype[Symbol.iterator], Set.prototype.values);
  assertTrue(Set.prototype.hasOwnProperty(Symbol.iterator));
  assertFalse(Set.prototype.propertyIsEnumerable(Symbol.iterator));

  var iter = new Set().values();
  assertEquals(iter, iter[Symbol.iterator]());
  assertEquals(iter[Symbol.iterator].name, '[Symbol.iterator]');
});


test(function TestMapIterator() {
  var m = new Map;
  var iter = m.values();

  var MapIteratorPrototype = iter.__proto__;
  assertFalse(MapIteratorPrototype.hasOwnProperty('constructor'));
  assertSame(MapIteratorPrototype.__proto__.__proto__, Object.prototype);

  var propertyNames = Object.getOwnPropertyNames(MapIteratorPrototype);
  assertArrayEquals(['next'], propertyNames);

  assertSame(new Map().values().__proto__, MapIteratorPrototype);
  assertSame(new Map().keys().__proto__, MapIteratorPrototype);
  assertSame(new Map().entries().__proto__, MapIteratorPrototype);

  assertEquals("[object Map Iterator]",
      Object.prototype.toString.call(iter));
  assertEquals("Map Iterator", MapIteratorPrototype[Symbol.toStringTag]);
  var desc = Object.getOwnPropertyDescriptor(
      MapIteratorPrototype, Symbol.toStringTag);
  assertTrue(desc.configurable);
  assertFalse(desc.writable);
  assertEquals("Map Iterator", desc.value);
});


test(function TestMapIteratorValues() {
  var m = new Map;
  m.set(1, 11);
  m.set(2, 22);
  m.set(3, 33);
  var iter = m.values();

  assertEquals({value: 11, done: false}, iter.next());
  assertEquals({value: 22, done: false}, iter.next());
  assertEquals({value: 33, done: false}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
});


test(function TestMapIteratorKeys() {
  var m = new Map;
  m.set(1, 11);
  m.set(2, 22);
  m.set(3, 33);
  var iter = m.keys();

  assertEquals({value: 1, done: false}, iter.next());
  assertEquals({value: 2, done: false}, iter.next());
  assertEquals({value: 3, done: false}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
});


test(function TestMapIteratorEntries() {
  var m = new Map;
  m.set(1, 11);
  m.set(2, 22);
  m.set(3, 33);
  var iter = m.entries();

  assertEquals({value: [1, 11], done: false}, iter.next());
  assertEquals({value: [2, 22], done: false}, iter.next());
  assertEquals({value: [3, 33], done: false}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
  assertEquals({value: undefined, done: true}, iter.next());
});


test(function TestMapInvalidReceiver() {
  assertThrows(function() {
    Map.prototype.values.call({});
  }, TypeError);
  assertThrows(function() {
    Map.prototype.keys.call({});
  }, TypeError);
  assertThrows(function() {
    Map.prototype.entries.call({});
  }, TypeError);
});


test(function TestMapIteratorInvalidReceiver() {
  var iter = new Map().values();
  assertThrows(function() {
    iter.next.call({});
  }, TypeError);
});


test(function TestMapIteratorSymbol() {
  assertEquals(Map.prototype[Symbol.iterator], Map.prototype.entries);
  assertTrue(Map.prototype.hasOwnProperty(Symbol.iterator));
  assertFalse(Map.prototype.propertyIsEnumerable(Symbol.iterator));

  var iter = new Map().values();
  assertEquals(iter, iter[Symbol.iterator]());
  assertEquals(iter[Symbol.iterator].name, '[Symbol.iterator]');
});
