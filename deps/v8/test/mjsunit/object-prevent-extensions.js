// Copyright 2010 the V8 project authors. All rights reserved.
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

// Tests the Object.preventExtensions method - ES 15.2.3.10

// Flags: --allow-natives-syntax


var obj1 = {};
// Extensible defaults to true.
assertTrue(Object.isExtensible(obj1));
Object.preventExtensions(obj1);

// Make sure the is_extensible flag is set.
assertFalse(Object.isExtensible(obj1));
obj1.x = 42;
assertEquals(undefined, obj1.x);

// Try adding a new element.
obj1[1] = 42;
assertEquals(undefined, obj1[1]);


// Try when the object has an existing property.
var obj2 = {};
assertTrue(Object.isExtensible(obj2));
obj2.x = 42;
assertEquals(42, obj2.x);
assertTrue(Object.isExtensible(obj2));

Object.preventExtensions(obj2);
assertEquals(42, obj2.x);

obj2.y = 42;
// obj2.y should still be undefined.
assertEquals(undefined, obj2.y);
// Make sure we can still write values to obj.x.
obj2.x = 43;
assertEquals(43, obj2.x)

obj2.y = new function() { return 42; };
// obj2.y should still be undefined.
assertEquals(undefined, obj2.y);
assertEquals(43, obj2.x)

try {
  Object.defineProperty(obj2, "y", {value: 42});
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}

// obj2.y should still be undefined.
assertEquals(undefined, obj2.y);
assertEquals(43, obj2.x);

obj2[1] = 42;
assertEquals(undefined, obj2[1]);

var arr = new Array();
arr[1] = 10;

Object.preventExtensions(arr);

arr[2] = 42;
assertEquals(10, arr[1]);

// We should still be able to change existing elements.
arr[1]= 42;
assertEquals(42, arr[1]);


// Test the the extensible flag is not inherited.
var parent = {};
parent.x = 42;
Object.preventExtensions(parent);

var child = Object.create(parent);

// We should be able to add new properties to the child object.
child.y = 42;

// This should have no influence on the parent class.
parent.y = 29;


// Test that attributes on functions are also handled correctly.
function foo() {
  return 42;
}

Object.preventExtensions(foo);

foo.x = 29;
assertEquals(undefined, foo.x);

// when Object.isExtensible(o) === false
// assignment should return right hand side value
var o = Object.preventExtensions({});
var v = o.v = 50;
assertEquals(undefined, o.v);
assertEquals(50, v);

// test same behavior as above, but for integer properties
var n = o[0] = 100;
assertEquals(undefined, o[0]);
assertEquals(100, n);

// Fast properties should remain fast
obj = { x: 42, y: 'foo' };
assertTrue(%HasFastProperties(obj));
Object.preventExtensions(obj);
assertFalse(Object.isExtensible(obj));
assertFalse(Object.isSealed(obj));
assertTrue(%HasFastProperties(obj));

// Non-extensible objects should share maps where possible
obj = { prop1: 1, prop2: 2 };
obj2 = { prop1: 3, prop2: 4 };
assertTrue(%HaveSameMap(obj, obj2));
Object.preventExtensions(obj);
Object.preventExtensions(obj2);
assertFalse(Object.isExtensible(obj));
assertFalse(Object.isExtensible(obj2));
assertFalse(Object.isSealed(obj));
assertFalse(Object.isSealed(obj2));
assertTrue(%HaveSameMap(obj, obj2));

// Non-extensible objects should share maps even when they have elements
obj = { prop1: 1, prop2: 2, 75: 'foo' };
obj2 = { prop1: 3, prop2: 4, 150: 'bar' };
assertTrue(%HaveSameMap(obj, obj2));
Object.preventExtensions(obj);
Object.preventExtensions(obj2);
assertFalse(Object.isExtensible(obj));
assertFalse(Object.isExtensible(obj2));
assertFalse(Object.isSealed(obj));
assertFalse(Object.isSealed(obj2));
assertTrue(%HaveSameMap(obj, obj2));

// Test packed element array built-in functions with preventExtensions.
obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.preventExtensions(obj);
assertFalse(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
assertFalse(Object.isExtensible(obj));
assertTrue(Array.isArray(obj));

// Verify that the length can't be written by builtins.
assertThrows(function() { obj.push(1); }, TypeError);
assertThrows(function() { obj.unshift(1); }, TypeError);
assertThrows(function() { obj.splice(0, 0, 1); }, TypeError);

// Verify search, filter, iterator
obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.preventExtensions(obj);
assertFalse(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
assertFalse(Object.isExtensible(obj));
assertTrue(Array.isArray(obj));
assertEquals(obj.lastIndexOf(1), 2);
assertEquals(obj.indexOf('a'), 4);
assertFalse(obj.includes(Symbol("test")));
assertEquals(obj.find(x => x==0), undefined);
assertEquals(obj.findIndex(x => x=='a'), 4);
assertTrue(obj.some(x => typeof x == 'symbol'));
assertFalse(obj.every(x => x == -1));
var filteredArray = obj.filter(e => typeof e == "symbol");
assertEquals(filteredArray.length, 1);
assertEquals(obj.map(x => x), obj);
var countPositiveNumber = 0;
obj.forEach(function(item, index) {
  if (item === 1) {
    countPositiveNumber++;
    assertEquals(index, 2);
  }
});
assertEquals(countPositiveNumber, 1);
assertEquals(obj.length, obj.concat([]).length);
var iterator = obj.values();
assertEquals(iterator.next().value, undefined);
assertEquals(iterator.next().value, null);
var iterator = obj.keys();
assertEquals(iterator.next().value, 0);
assertEquals(iterator.next().value, 1);
var iterator = obj.entries();
assertEquals(iterator.next().value, [0, undefined]);
assertEquals(iterator.next().value, [1, null]);

// Verify that the value can be written
var length = obj.length;
for (var i = 0; i < length-1; i++) {
  obj[i] = 'new';
  assertEquals(obj[i], 'new');
}

// Verify flat, map, flatMap, join, reduce, reduceRight for sealed packed array
var arr = ['a', 'b', 'c'];
assertTrue(%HasPackedElements(arr));
Object.preventExtensions(arr);
assertFalse(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
assertFalse(Object.isExtensible(obj));
assertTrue(Array.isArray(obj));
assertEquals(arr.map(x => [x]), [['a'], ['b'], ['c']]);
assertEquals(arr.flatMap(x => [x]), arr);
assertEquals(arr.flat(), arr);
assertEquals(arr.join('-'), "a-b-c");
const reducer = (accumulator, currentValue) => accumulator + currentValue;
assertEquals(arr.reduce(reducer), "abc");
assertEquals(arr.reduceRight(reducer), "cba");
assertEquals(arr.slice(0, 1), ['a']);

// Verify change content of sealed packed array
arr.sort();
assertEquals(arr.join(''), "abc");
arr.reverse();
assertEquals(arr.join(''), "cba");
arr.copyWithin(0, 1, 2);
assertEquals(arr.join(''),"bba");
arr.fill('d');
assertEquals(arr.join(''), "ddd");
arr.pop();
assertEquals(arr.join(''), "dd");
