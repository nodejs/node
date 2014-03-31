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

// Flags: --expose-gc --allow-natives-syntax


// Note: this test is superseded by harmony/collections.js.
// IF YOU CHANGE THIS FILE, apply the same changes to harmony/collections.js!
// TODO(rossberg): Remove once non-weak collections have caught up.

// Test valid getter and setter calls on WeakSets.
function TestValidSetCalls(m) {
  assertDoesNotThrow(function () { m.add(new Object) });
  assertDoesNotThrow(function () { m.has(new Object) });
  assertDoesNotThrow(function () { m.delete(new Object) });
}
TestValidSetCalls(new WeakSet);


// Test valid getter and setter calls on WeakMaps
function TestValidMapCalls(m) {
  assertDoesNotThrow(function () { m.get(new Object) });
  assertDoesNotThrow(function () { m.set(new Object) });
  assertDoesNotThrow(function () { m.has(new Object) });
  assertDoesNotThrow(function () { m.delete(new Object) });
}
TestValidMapCalls(new WeakMap);


// Test invalid getter and setter calls for WeakMap
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


// Test expected behavior for WeakSets
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
TestSet(new WeakSet, new Object);


// Test expected mapping behavior for WeakMaps
function TestMapping(map, key, value) {
  assertSame(undefined, map.set(key, value));
  assertSame(value, map.get(key));
}
function TestMapBehavior1(m) {
  TestMapping(m, new Object, 23);
  TestMapping(m, new Object, 'the-value');
  TestMapping(m, new Object, new Object);
}
TestMapBehavior1(new WeakMap);


// Test expected querying behavior of WeakMaps
function TestQuery(m) {
  var key = new Object;
  var values = [ 'x', 0, +Infinity, -Infinity, true, false, null, undefined ];
  for (var i = 0; i < values.length; i++) {
    TestMapping(m, key, values[i]);
    assertTrue(m.has(key));
    assertFalse(m.has(new Object));
  }
}
TestQuery(new WeakMap);


// Test expected deletion behavior of WeakMaps
function TestDelete(m) {
  var key = new Object;
  TestMapping(m, key, 'to-be-deleted');
  assertTrue(m.delete(key));
  assertFalse(m.delete(key));
  assertFalse(m.delete(new Object));
  assertSame(m.get(key), undefined);
}
TestDelete(new WeakMap);


// Test GC of WeakMaps with entry
function TestGC1(m) {
  var key = new Object;
  m.set(key, 'not-collected');
  gc();
  assertSame('not-collected', m.get(key));
}
TestGC1(new WeakMap);


// Test GC of WeakMaps with chained entries
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
TestEnumerable(WeakMap);
TestEnumerable(WeakSet);


// Test arbitrary properties on WeakMaps
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
TestArbitrary(new WeakMap);


// Test direct constructor call
assertThrows(function() { WeakMap(); }, TypeError);
assertThrows(function() { WeakSet(); }, TypeError);


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
assertEquals("WeakMap", %_ClassOf(new WeakMap))
assertEquals("Object", %_ClassOf(WeakMap.prototype))
assertEquals("WeakSet", %_ClassOf(new WeakSet))
assertEquals("Object", %_ClassOf(WeakMap.prototype))


// Test name of constructor.
assertEquals("WeakMap", WeakMap.name);
assertEquals("WeakSet", WeakSet.name);


// Test prototype property of WeakMap and WeakSet.
function TestPrototype(C) {
  assertTrue(C.prototype instanceof Object);
  assertEquals({
    value: {},
    writable: false,
    enumerable: false,
    configurable: false
  }, Object.getOwnPropertyDescriptor(C, "prototype"));
}
TestPrototype(WeakMap);
TestPrototype(WeakSet);


// Test constructor property of the WeakMap and WeakSet prototype.
function TestConstructor(C) {
  assertFalse(C === Object.prototype.constructor);
  assertSame(C, C.prototype.constructor);
  assertSame(C, (new C).__proto__.constructor);
}
TestConstructor(WeakMap);
TestConstructor(WeakSet);


// Test the WeakMap and WeakSet global properties themselves.
function TestDescriptor(global, C) {
  assertEquals({
    value: C,
    writable: true,
    enumerable: false,
    configurable: true
  }, Object.getOwnPropertyDescriptor(global, C.name));
}
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
  { proto: WeakMap.prototype,
    funcs: [ 'get', 'set', 'has', 'delete' ],
    receivers: alwaysBogus.concat([ new WeakSet ]),
  },
  { proto: WeakSet.prototype,
    funcs: [ 'add', 'has', 'delete' ],
    receivers: alwaysBogus.concat([ new WeakMap ]),
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
