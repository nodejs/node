// Copyright 2011 the V8 project authors. All rights reserved.
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

// Flags: --harmony-weakmaps --expose-gc


// Test valid getter and setter calls
var m = new WeakMap;
assertDoesNotThrow(function () { m.get(new Object) });
assertDoesNotThrow(function () { m.set(new Object) });
assertDoesNotThrow(function () { m.has(new Object) });
assertDoesNotThrow(function () { m.delete(new Object) });


// Test invalid getter and setter calls
var m = new WeakMap;
assertThrows(function () { m.get(undefined) }, TypeError);
assertThrows(function () { m.set(undefined, 0) }, TypeError);
assertThrows(function () { m.get(0) }, TypeError);
assertThrows(function () { m.set(0, 0) }, TypeError);
assertThrows(function () { m.get('a-key') }, TypeError);
assertThrows(function () { m.set('a-key', 0) }, TypeError);


// Test expected mapping behavior
var m = new WeakMap;
function TestMapping(map, key, value) {
  map.set(key, value);
  assertSame(value, map.get(key));
}
TestMapping(m, new Object, 23);
TestMapping(m, new Object, 'the-value');
TestMapping(m, new Object, new Object);


// Test expected querying behavior
var m = new WeakMap;
var key = new Object;
TestMapping(m, key, 'to-be-present');
assertTrue(m.has(key));
assertFalse(m.has(new Object));
TestMapping(m, key, undefined);
assertFalse(m.has(key));
assertFalse(m.has(new Object));


// Test expected deletion behavior
var m = new WeakMap;
var key = new Object;
TestMapping(m, key, 'to-be-deleted');
assertTrue(m.delete(key));
assertFalse(m.delete(key));
assertFalse(m.delete(new Object));
assertSame(m.get(key), undefined);


// Test GC of map with entry
var m = new WeakMap;
var key = new Object;
m.set(key, 'not-collected');
gc();
assertSame('not-collected', m.get(key));


// Test GC of map with chained entries
var m = new WeakMap;
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


// Test property attribute [[Enumerable]]
var m = new WeakMap;
function props(x) {
  var array = [];
  for (var p in x) array.push(p);
  return array.sort();
}
assertArrayEquals([], props(WeakMap));
assertArrayEquals([], props(WeakMap.prototype));
assertArrayEquals([], props(m));


// Test arbitrary properties on weak maps
var m = new WeakMap;
function TestProperty(map, property, value) {
  map[property] = value;
  assertEquals(value, map[property]);
}
for (i = 0; i < 20; i++) {
  TestProperty(m, i, 'val' + i);
  TestProperty(m, 'foo' + i, 'bar' + i);
}
TestMapping(m, new Object, 'foobar');


// Test direct constructor call
var m = WeakMap();
assertTrue(m instanceof WeakMap);


// Test some common JavaScript idioms
var m = new WeakMap;
assertTrue(m instanceof WeakMap);
assertTrue(WeakMap.prototype.set instanceof Function)
assertTrue(WeakMap.prototype.get instanceof Function)
assertTrue(WeakMap.prototype.has instanceof Function)
assertTrue(WeakMap.prototype.delete instanceof Function)


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


// Stress Test
// There is a proposed stress-test available at the es-discuss mailing list
// which cannot be reasonably automated.  Check it out by hand if you like:
// https://mail.mozilla.org/pipermail/es-discuss/2011-May/014096.html
