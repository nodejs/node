// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --harmony-collections
// Flags: --expose-gc --allow-natives-syntax


// Test valid getter and setter calls on Sets and WeakSets
function TestValidSetCalls(m) {
  assertDoesNotThrow(function () { m.add(new Object) });
  assertDoesNotThrow(function () { m.has(new Object) });
  assertDoesNotThrow(function () { m.delete(new Object) });
}
TestValidSetCalls(new Set);
TestValidSetCalls(new WeakSet);


// Test valid getter and setter calls on Maps and WeakMaps
function TestValidMapCalls(m) {
  assertDoesNotThrow(function () { m.get(new Object) });
  assertDoesNotThrow(function () { m.set(new Object) });
  assertDoesNotThrow(function () { m.has(new Object) });
  assertDoesNotThrow(function () { m.delete(new Object) });
}
TestValidMapCalls(new Map);
TestValidMapCalls(new WeakMap);


// Test invalid getter and setter calls for WeakMap only
function TestInvalidCalls(m) {
  assertThrows(function () { m.get(undefined) }, TypeError);
  assertThrows(function () { m.set(undefined, 0) }, TypeError);
  assertThrows(function () { m.get(null) }, TypeError);
  assertThrows(function () { m.set(null, 0) }, TypeError);
  assertThrows(function () { m.get(0) }, TypeError);
  assertThrows(function () { m.set(0, 0) }, TypeError);
  assertThrows(function () { m.get('a-key') }, TypeError);
  assertThrows(function () { m.set('a-key', 0) }, TypeError);
}
TestInvalidCalls(new WeakMap);


// Test expected behavior for Sets and WeakSets
function TestSet(set, key) {
  assertFalse(set.has(key));
  assertSame(undefined, set.add(key));
  assertTrue(set.has(key));
  assertTrue(set.delete(key));
  assertFalse(set.has(key));
  assertFalse(set.delete(key));
  assertFalse(set.has(key));
}
function TestSetBehavior(set) {
  for (var i = 0; i < 20; i++) {
    TestSet(set, new Object);
    TestSet(set, i);
    TestSet(set, i / 100);
    TestSet(set, 'key-' + i);
  }
  var keys = [ +0, -0, +Infinity, -Infinity, true, false, null, undefined ];
  for (var i = 0; i < keys.length; i++) {
    TestSet(set, keys[i]);
  }
}
TestSetBehavior(new Set);
TestSet(new WeakSet, new Object);


// Test expected mapping behavior for Maps and WeakMaps
function TestMapping(map, key, value) {
  assertSame(undefined, map.set(key, value));
  assertSame(value, map.get(key));
}
function TestMapBehavior1(m) {
  TestMapping(m, new Object, 23);
  TestMapping(m, new Object, 'the-value');
  TestMapping(m, new Object, new Object);
}
TestMapBehavior1(new Map);
TestMapBehavior1(new WeakMap);


// Test expected mapping behavior for Maps only
function TestMapBehavior2(m) {
  for (var i = 0; i < 20; i++) {
    TestMapping(m, i, new Object);
    TestMapping(m, i / 10, new Object);
    TestMapping(m, 'key-' + i, new Object);
  }
  var keys = [ +0, -0, +Infinity, -Infinity, true, false, null, undefined ];
  for (var i = 0; i < keys.length; i++) {
    TestMapping(m, keys[i], new Object);
  }
}
TestMapBehavior2(new Map);


// Test expected querying behavior of Maps and WeakMaps
function TestQuery(m) {
  var key = new Object;
  var values = [ 'x', 0, +Infinity, -Infinity, true, false, null, undefined ];
  for (var i = 0; i < values.length; i++) {
    TestMapping(m, key, values[i]);
    assertTrue(m.has(key));
    assertFalse(m.has(new Object));
  }
}
TestQuery(new Map);
TestQuery(new WeakMap);


// Test expected deletion behavior of Maps and WeakMaps
function TestDelete(m) {
  var key = new Object;
  TestMapping(m, key, 'to-be-deleted');
  assertTrue(m.delete(key));
  assertFalse(m.delete(key));
  assertFalse(m.delete(new Object));
  assertSame(m.get(key), undefined);
}
TestDelete(new Map);
TestDelete(new WeakMap);


// Test GC of Maps and WeakMaps with entry
function TestGC1(m) {
  var key = new Object;
  m.set(key, 'not-collected');
  gc();
  assertSame('not-collected', m.get(key));
}
TestGC1(new Map);
TestGC1(new WeakMap);


// Test GC of Maps and WeakMaps with chained entries
function TestGC2(m) {
  var head = new Object;
  for (key = head, i = 0; i < 10; i++, key = m.get(key)) {
    m.set(key, new Object);
  }
  gc();
  var count = 0;
  for (key = head; key != undefined; key = m.get(key)) {
    count++;
  }
  assertEquals(11, count);
}
TestGC2(new Map);
TestGC2(new WeakMap);


// Test property attribute [[Enumerable]]
function TestEnumerable(func) {
  function props(x) {
    var array = [];
    for (var p in x) array.push(p);
    return array.sort();
  }
  assertArrayEquals([], props(func));
  assertArrayEquals([], props(func.prototype));
  assertArrayEquals([], props(new func()));
}
TestEnumerable(Set);
TestEnumerable(Map);
TestEnumerable(WeakMap);
TestEnumerable(WeakSet);


// Test arbitrary properties on Maps and WeakMaps
function TestArbitrary(m) {
  function TestProperty(map, property, value) {
    map[property] = value;
    assertEquals(value, map[property]);
  }
  for (var i = 0; i < 20; i++) {
    TestProperty(m, i, 'val' + i);
    TestProperty(m, 'foo' + i, 'bar' + i);
  }
  TestMapping(m, new Object, 'foobar');
}
TestArbitrary(new Map);
TestArbitrary(new WeakMap);


// Test direct constructor call
assertThrows(function() { Set(); }, TypeError);
assertThrows(function() { Map(); }, TypeError);
assertThrows(function() { WeakMap(); }, TypeError);
assertThrows(function() { WeakSet(); }, TypeError);


// Test whether NaN values as keys are treated correctly.
var s = new Set;
assertFalse(s.has(NaN));
assertFalse(s.has(NaN + 1));
assertFalse(s.has(23));
s.add(NaN);
assertTrue(s.has(NaN));
assertTrue(s.has(NaN + 1));
assertFalse(s.has(23));
var m = new Map;
assertFalse(m.has(NaN));
assertFalse(m.has(NaN + 1));
assertFalse(m.has(23));
m.set(NaN, 'a-value');
assertTrue(m.has(NaN));
assertTrue(m.has(NaN + 1));
assertFalse(m.has(23));


// Test some common JavaScript idioms for Sets
var s = new Set;
assertTrue(s instanceof Set);
assertTrue(Set.prototype.add instanceof Function)
assertTrue(Set.prototype.has instanceof Function)
assertTrue(Set.prototype.delete instanceof Function)
assertTrue(Set.prototype.clear instanceof Function)


// Test some common JavaScript idioms for Maps
var m = new Map;
assertTrue(m instanceof Map);
assertTrue(Map.prototype.set instanceof Function)
assertTrue(Map.prototype.get instanceof Function)
assertTrue(Map.prototype.has instanceof Function)
assertTrue(Map.prototype.delete instanceof Function)
assertTrue(Map.prototype.clear instanceof Function)


// Test some common JavaScript idioms for WeakMaps
var m = new WeakMap;
assertTrue(m instanceof WeakMap);
assertTrue(WeakMap.prototype.set instanceof Function)
assertTrue(WeakMap.prototype.get instanceof Function)
assertTrue(WeakMap.prototype.has instanceof Function)
assertTrue(WeakMap.prototype.delete instanceof Function)
assertTrue(WeakMap.prototype.clear instanceof Function)


// Test some common JavaScript idioms for WeakSets
var s = new WeakSet;
assertTrue(s instanceof WeakSet);
assertTrue(WeakSet.prototype.add instanceof Function)
assertTrue(WeakSet.prototype.has instanceof Function)
assertTrue(WeakSet.prototype.delete instanceof Function)
assertTrue(WeakSet.prototype.clear instanceof Function)


// Test class of instance and prototype.
assertEquals("Set", %_ClassOf(new Set))
assertEquals("Object", %_ClassOf(Set.prototype))
assertEquals("Map", %_ClassOf(new Map))
assertEquals("Object", %_ClassOf(Map.prototype))
assertEquals("WeakMap", %_ClassOf(new WeakMap))
assertEquals("Object", %_ClassOf(WeakMap.prototype))
assertEquals("WeakSet", %_ClassOf(new WeakSet))
assertEquals("Object", %_ClassOf(WeakMap.prototype))


// Test name of constructor.
assertEquals("Set", Set.name);
assertEquals("Map", Map.name);
assertEquals("WeakMap", WeakMap.name);
assertEquals("WeakSet", WeakSet.name);


// Test prototype property of Set, Map, WeakMap and WeakSet.
// TODO(2793): Should all be non-writable, and the extra flag removed.
function TestPrototype(C, writable) {
  assertTrue(C.prototype instanceof Object);
  assertEquals({
    value: {},
    writable: writable,
    enumerable: false,
    configurable: false
  }, Object.getOwnPropertyDescriptor(C, "prototype"));
}
TestPrototype(Set, true);
TestPrototype(Map, true);
TestPrototype(WeakMap, false);
TestPrototype(WeakSet, false);


// Test constructor property of the Set, Map, WeakMap and WeakSet prototype.
function TestConstructor(C) {
  assertFalse(C === Object.prototype.constructor);
  assertSame(C, C.prototype.constructor);
  assertSame(C, (new C).__proto__.constructor);
}
TestConstructor(Set);
TestConstructor(Map);
TestConstructor(WeakMap);
TestConstructor(WeakSet);


// Test the Set, Map, WeakMap and WeakSet global properties themselves.
function TestDescriptor(global, C) {
  assertEquals({
    value: C,
    writable: true,
    enumerable: false,
    configurable: true
  }, Object.getOwnPropertyDescriptor(global, C.name));
}
TestDescriptor(this, Set);
TestDescriptor(this, Map);
TestDescriptor(this, WeakMap);
TestDescriptor(this, WeakSet);


// Regression test for WeakMap prototype.
assertTrue(WeakMap.prototype.constructor === WeakMap)
assertTrue(Object.getPrototypeOf(WeakMap.prototype) === Object.prototype)


// Regression test for issue 1617: The prototype of the WeakMap constructor
// needs to be unique (i.e. different from the one of the Object constructor).
assertFalse(WeakMap.prototype === Object.prototype);
var o = Object.create({});
assertFalse("get" in o);
assertFalse("set" in o);
assertEquals(undefined, o.get);
assertEquals(undefined, o.set);
var o = Object.create({}, { myValue: {
  value: 10,
  enumerable: false,
  configurable: true,
  writable: true
}});
assertEquals(10, o.myValue);


// Regression test for issue 1884: Invoking any of the methods for Harmony
// maps, sets, or weak maps, with a wrong type of receiver should be throwing
// a proper TypeError.
var alwaysBogus = [ undefined, null, true, "x", 23, {} ];
var bogusReceiversTestSet = [
  { proto: Set.prototype,
    funcs: [ 'add', 'has', 'delete' ],
    receivers: alwaysBogus.concat([ new Map, new WeakMap, new WeakSet ]),
  },
  { proto: Map.prototype,
    funcs: [ 'get', 'set', 'has', 'delete' ],
    receivers: alwaysBogus.concat([ new Set, new WeakMap, new WeakSet ]),
  },
  { proto: WeakMap.prototype,
    funcs: [ 'get', 'set', 'has', 'delete' ],
    receivers: alwaysBogus.concat([ new Set, new Map, new WeakSet ]),
  },
  { proto: WeakSet.prototype,
    funcs: [ 'add', 'has', 'delete' ],
    receivers: alwaysBogus.concat([ new Set, new Map, new WeakMap ]),
  },
];
function TestBogusReceivers(testSet) {
  for (var i = 0; i < testSet.length; i++) {
    var proto = testSet[i].proto;
    var funcs = testSet[i].funcs;
    var receivers = testSet[i].receivers;
    for (var j = 0; j < funcs.length; j++) {
      var func = proto[funcs[j]];
      for (var k = 0; k < receivers.length; k++) {
        assertThrows(function () { func.call(receivers[k], {}) }, TypeError);
      }
    }
  }
}
TestBogusReceivers(bogusReceiversTestSet);


// Stress Test
// There is a proposed stress-test available at the es-discuss mailing list
// which cannot be reasonably automated.  Check it out by hand if you like:
// https://mail.mozilla.org/pipermail/es-discuss/2011-May/014096.html


// Set and Map size getters
var setSizeDescriptor = Object.getOwnPropertyDescriptor(Set.prototype, 'size');
assertEquals(undefined, setSizeDescriptor.value);
assertEquals(undefined, setSizeDescriptor.set);
assertTrue(setSizeDescriptor.get instanceof Function);
assertEquals(undefined, setSizeDescriptor.get.prototype);
assertFalse(setSizeDescriptor.enumerable);
assertTrue(setSizeDescriptor.configurable);

var s = new Set();
assertFalse(s.hasOwnProperty('size'));
for (var i = 0; i < 10; i++) {
  assertEquals(i, s.size);
  s.add(i);
}
for (var i = 9; i >= 0; i--) {
  s.delete(i);
  assertEquals(i, s.size);
}


var mapSizeDescriptor = Object.getOwnPropertyDescriptor(Map.prototype, 'size');
assertEquals(undefined, mapSizeDescriptor.value);
assertEquals(undefined, mapSizeDescriptor.set);
assertTrue(mapSizeDescriptor.get instanceof Function);
assertEquals(undefined, mapSizeDescriptor.get.prototype);
assertFalse(mapSizeDescriptor.enumerable);
assertTrue(mapSizeDescriptor.configurable);

var m = new Map();
assertFalse(m.hasOwnProperty('size'));
for (var i = 0; i < 10; i++) {
  assertEquals(i, m.size);
  m.set(i, i);
}
for (var i = 9; i >= 0; i--) {
  m.delete(i);
  assertEquals(i, m.size);
}


// Test Set clear
(function() {
  var s = new Set();
  s.add(42);
  assertTrue(s.has(42));
  assertEquals(1, s.size);
  s.clear();
  assertFalse(s.has(42));
  assertEquals(0, s.size);
})();


// Test Map clear
(function() {
  var m = new Map();
  m.set(42, true);
  assertTrue(m.has(42));
  assertEquals(1, m.size);
  m.clear();
  assertFalse(m.has(42));
  assertEquals(0, m.size);
})();


// Test WeakMap clear
(function() {
  var k = new Object();
  var w = new WeakMap();
  w.set(k, 23);
  assertTrue(w.has(k));
  assertEquals(23, w.get(k));
  w.clear();
  assertFalse(w.has(k));
  assertEquals(undefined, w.get(k));
})();


// Test WeakSet clear
(function() {
  var k = new Object();
  var w = new WeakSet();
  w.add(k);
  assertTrue(w.has(k));
  w.clear();
  assertFalse(w.has(k));
})();


(function TestMinusZeroSet() {
  var m = new Set();
  m.add(0);
  m.add(-0);
  assertEquals(1, m.size);
  assertTrue(m.has(0));
  assertTrue(m.has(-0));
})();


(function TestMinusZeroMap() {
  var m = new Map();
  m.set(0, 'plus');
  m.set(-0, 'minus');
  assertEquals(1, m.size);
  assertTrue(m.has(0));
  assertTrue(m.has(-0));
  assertEquals('minus', m.get(0));
  assertEquals('minus', m.get(-0));
})();
