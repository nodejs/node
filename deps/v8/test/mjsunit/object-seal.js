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

// Tests the Object.seal and Object.isSealed methods - ES 19.1.2.17 and
// ES 19.1.2.13

// Flags: --allow-natives-syntax --turbofan --noalways-turbofan

// Test that we return obj if non-object is passed as argument
var non_objects = new Array(undefined, null, 1, -1, 0, 42.43, Symbol("test"));
for (var key in non_objects) {
  assertSame(non_objects[key], Object.seal(non_objects[key]));
}

// Test that isSealed always returns true for non-objects
for (var key in non_objects) {
  assertTrue(Object.isSealed(non_objects[key]));
}

// Test normal data properties.
var obj = { x: 42, z: 'foobar' };
var desc = Object.getOwnPropertyDescriptor(obj, 'x');
assertTrue(desc.writable);
assertTrue(desc.configurable);
assertEquals(42, desc.value);

desc = Object.getOwnPropertyDescriptor(obj, 'z');
assertTrue(desc.writable);
assertTrue(desc.configurable);
assertEquals('foobar', desc.value);

assertTrue(Object.isExtensible(obj));
assertFalse(Object.isSealed(obj));

Object.seal(obj);

// Make sure we are no longer extensible.
assertFalse(Object.isExtensible(obj));
assertTrue(Object.isSealed(obj));

// We should not be frozen, since we are still able to
// update values.
assertFalse(Object.isFrozen(obj));

// We should not allow new properties to be added.
obj.foo = 42;
assertEquals(obj.foo, undefined);

desc = Object.getOwnPropertyDescriptor(obj, 'x');
assertTrue(desc.writable);
assertFalse(desc.configurable);
assertEquals(42, desc.value);

desc = Object.getOwnPropertyDescriptor(obj, 'z');
assertTrue(desc.writable);
assertFalse(desc.configurable);
assertEquals("foobar", desc.value);

// Since writable is not affected by seal we should still be able to
// update the values.
obj.x = "43";
assertEquals("43", obj.x);

// Test on accessors.
var obj2 = {};
function get() { return 43; };
function set() {};
Object.defineProperty(obj2, 'x', { get: get, set: set, configurable: true });

desc = Object.getOwnPropertyDescriptor(obj2, 'x');
assertTrue(desc.configurable);
assertEquals(undefined, desc.value);
assertEquals(set, desc.set);
assertEquals(get, desc.get);

assertTrue(Object.isExtensible(obj2));
assertFalse(Object.isSealed(obj2));
Object.seal(obj2);

// Since this is an accessor property the object is now effectively both
// sealed and frozen (accessors has no writable attribute).
assertTrue(Object.isFrozen(obj2));
assertFalse(Object.isExtensible(obj2));
assertTrue(Object.isSealed(obj2));

desc = Object.getOwnPropertyDescriptor(obj2, 'x');
assertFalse(desc.configurable);
assertEquals(undefined, desc.value);
assertEquals(set, desc.set);
assertEquals(get, desc.get);

obj2.foo = 42;
assertEquals(obj2.foo, undefined);

// Test seal on arrays.
var arr = new Array(42,43);

desc = Object.getOwnPropertyDescriptor(arr, '0');
assertTrue(desc.configurable);
assertTrue(desc.writable);
assertEquals(42, desc.value);

desc = Object.getOwnPropertyDescriptor(arr, '1');
assertTrue(desc.configurable);
assertTrue(desc.writable);
assertEquals(43, desc.value);

assertTrue(Object.isExtensible(arr));
assertFalse(Object.isSealed(arr));
Object.seal(arr);
assertTrue(Object.isSealed(arr));
assertFalse(Object.isExtensible(arr));
// Since the values in the array is still writable this object
// is not frozen.
assertFalse(Object.isFrozen(arr));

desc = Object.getOwnPropertyDescriptor(arr, '0');
assertFalse(desc.configurable);
assertTrue(desc.writable);
assertEquals(42, desc.value);

desc = Object.getOwnPropertyDescriptor(arr, '1');
assertFalse(desc.configurable);
assertTrue(desc.writable);
assertEquals(43, desc.value);

arr[0] = 'foo';

// We should be able to overwrite the existing value.
assertEquals('foo', arr[0]);

// Test that isSealed returns the correct value even if configurable
// has been set to false on all properties manually and the extensible
// flag has also been set to false manually.
var obj3 = { x: 42, y: 'foo' };

assertFalse(Object.isFrozen(obj3));

Object.defineProperty(obj3, 'x', {configurable: false, writable: true});
Object.defineProperty(obj3, 'y', {configurable: false, writable: false});
Object.preventExtensions(obj3);

assertTrue(Object.isSealed(obj3));


// Make sure that an object that has a configurable property
// is not classified as sealed.
var obj4 = {};
Object.defineProperty(obj4, 'x', {configurable: true, writable: false});
Object.defineProperty(obj4, 'y', {configurable: false, writable: false});
Object.preventExtensions(obj4);

assertFalse(Object.isSealed(obj4));

// Make sure that Object.seal returns the sealed object.
var obj4 = {};
assertTrue(obj4 === Object.seal(obj4));

//
// Test that built-in array functions can't modify a sealed array.
//
obj = [1, 2, 3];
var objControl = [4, 5, 6];

// Allow these functions to set up monomorphic calls, using custom built-ins.
var push_call = function(a) { a.push(10); return a; }
var pop_call = function(a) { return a.pop(); }
for (var i = 0; i < 3; i++) {
  push_call(obj);
  pop_call(obj);
}

Object.seal(obj);
assertThrows(function() { push_call(obj); }, TypeError);
assertThrows(function() { pop_call(obj); }, TypeError);

// But the control object is fine at these sites.
assertDoesNotThrow(function() { push_call(objControl); });
assertDoesNotThrow(function() { pop_call(objControl); });

assertDoesNotThrow(function() { obj.push(); });
assertThrows(function() { obj.push(3); }, TypeError);
assertThrows(function() { obj.pop(); }, TypeError);
assertThrows(function() { obj.shift(3); }, TypeError);
assertDoesNotThrow(function() { obj.unshift(); });
assertThrows(function() { obj.unshift(1); }, TypeError);
assertThrows(function() { obj.splice(0, 0, 100, 101, 102); }, TypeError);
assertDoesNotThrow(function() { obj.splice(0,0); });

assertDoesNotThrow(function() { objControl.push(3); });
assertDoesNotThrow(function() { objControl.pop(); });
assertDoesNotThrow(function() { objControl.shift(3); });
assertDoesNotThrow(function() { objControl.unshift(); });
assertDoesNotThrow(function() { objControl.splice(0, 0, 100, 101, 102); });

// Verify that crankshaft still does the right thing.
obj = [1, 2, 3];

push_call = function(a) { a.push(1000); return a; };
%PrepareFunctionForOptimization(push_call);
// Include a call site that doesn't have a custom built-in.
var shift_call = function(a) { a.shift(1000); return a; };
%PrepareFunctionForOptimization(shift_call);
for (var i = 0; i < 3; i++) {
  push_call(obj);
  shift_call(obj);
}

%OptimizeFunctionOnNextCall(push_call);
%OptimizeFunctionOnNextCall(shift_call);
push_call(obj);
shift_call(obj);
assertOptimized(push_call);
assertOptimized(shift_call);
Object.seal(obj);
assertThrows(function() { push_call(obj); }, TypeError);
assertThrows(function() { shift_call(obj); }, TypeError);
assertUnoptimized(push_call);
assertUnoptimized(shift_call);
assertDoesNotThrow(function() { push_call(objControl); });
assertDoesNotThrow(function() { shift_call(objControl); });

// Verify special behavior of splice on sealed objects.
obj = [1,2,3];
Object.seal(obj);
assertDoesNotThrow(function() { obj.splice(0,1,100); });
assertEquals(100, obj[0]);
assertDoesNotThrow(function() { obj.splice(0,2,1,2); });
assertDoesNotThrow(function() { obj.splice(1,2,1,2); });
// Count of items to delete is clamped by length.
assertDoesNotThrow(function() { obj.splice(1,2000,1,2); });
assertThrows(function() { obj.splice(0,0,1); }, TypeError);
assertThrows(function() { obj.splice(1,2000,1,2,3); }, TypeError);

// Test that the enumerable attribute is unperturbed by sealing.
obj = { x: 42, y: 'foo' };
Object.defineProperty(obj, 'y', {enumerable: false});
Object.seal(obj);
assertTrue(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
desc = Object.getOwnPropertyDescriptor(obj, 'x');
assertTrue(desc.enumerable);
desc = Object.getOwnPropertyDescriptor(obj, 'y');
assertFalse(desc.enumerable);

// Fast properties should remain fast
obj = { x: 42, y: 'foo' };
assertTrue(%HasFastProperties(obj));
Object.seal(obj);
assertTrue(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
assertTrue(%HasFastProperties(obj));

// Sealed objects should share maps where possible
obj = { prop1: 1, prop2: 2 };
obj2 = { prop1: 3, prop2: 4 };
assertTrue(%HaveSameMap(obj, obj2));
Object.seal(obj);
Object.seal(obj2);
assertTrue(Object.isSealed(obj));
assertTrue(Object.isSealed(obj2));
assertFalse(Object.isFrozen(obj));
assertFalse(Object.isFrozen(obj2));
assertTrue(%HaveSameMap(obj, obj2));

// Sealed objects should share maps even when they have elements
obj = { prop1: 1, prop2: 2, 75: 'foo' };
obj2 = { prop1: 3, prop2: 4, 150: 'bar' };
assertTrue(%HaveSameMap(obj, obj2));
Object.seal(obj);
Object.seal(obj2);
assertTrue(Object.isSealed(obj));
assertTrue(Object.isSealed(obj2));
assertFalse(Object.isFrozen(obj));
assertFalse(Object.isFrozen(obj));
assertTrue(%HaveSameMap(obj, obj2));

// Setting elements after sealing should not be allowed
obj = { prop: 'thing' };
Object.seal(obj);
assertTrue(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
obj[0] = 'hello';
assertFalse(obj.hasOwnProperty(0));

// Sealing an object in dictionary mode should work
// Also testing that getter/setter properties work after sealing
obj = { };
for (var i = 0; i < 100; ++i) {
  obj['x' + i] = i;
}
var accessorDidRun = false;
Object.defineProperty(obj, 'accessor', {
  get: function() { return 42 },
  set: function() { accessorDidRun = true },
  configurable: true,
  enumerable: true
});

assertFalse(%HasFastProperties(obj));
Object.seal(obj);
assertFalse(%HasFastProperties(obj));
assertTrue(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
assertFalse(Object.isExtensible(obj));
for (var i = 0; i < 100; ++i) {
  desc = Object.getOwnPropertyDescriptor(obj, 'x' + i);
  assertFalse(desc.configurable);
}
assertEquals(42, obj.accessor);
assertFalse(accessorDidRun);
obj.accessor = 'ignored value';
assertTrue(accessorDidRun);

// Sealing arguments should work
var func = function(arg) {
  Object.seal(arguments);
  assertTrue(Object.isSealed(arguments));
};
func('hello', 'world');
func('goodbye', 'world');

// Sealing sparse arrays
var sparseArr = [0, 1];
sparseArr[10000] = 10000;
Object.seal(sparseArr);
assertTrue(Object.isSealed(sparseArr));

// Accessors on fast object should behavior properly after sealing
obj = {};
Object.defineProperty(obj, 'accessor', {
  get: function() { return 42 },
  set: function() { accessorDidRun = true },
  configurable: true,
  enumerable: true
});
assertTrue(%HasFastProperties(obj));
Object.seal(obj);
assertTrue(Object.isSealed(obj));
assertTrue(%HasFastProperties(obj));
assertEquals(42, obj.accessor);
accessorDidRun = false;
obj.accessor = 'ignored value';
assertTrue(accessorDidRun);

// Test for regression in mixed accessor/data property objects.
// The strict function is one such object.
assertTrue(Object.isSealed(Object.seal(function(){"use strict";})));

// Also test a simpler case
obj = {};
Object.defineProperty(obj, 'accessor2', {
  get: function() { return 42 },
  set: function() { accessorDidRun = true },
  configurable: true,
  enumerable: true
});
obj.data = 'foo';
assertTrue(%HasFastProperties(obj));
Object.seal(obj);
assertTrue(%HasFastProperties(obj));
assertTrue(Object.isSealed(obj));

function Sealed() {}
Object.seal(Sealed);
assertDoesNotThrow(function() { return new Sealed(); });
Sealed.prototype.prototypeExists = true;
assertTrue((new Sealed()).prototypeExists);

obj = new Int32Array(10);
Object.seal(obj);
assertTrue(Object.isSealed(obj));

// Test packed element array built-in functions with seal.
function testPackedSealedArray1(obj) {
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));

  // Verify that the length can't be written by builtins.
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(1); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(1); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertDoesNotThrow(function() { obj.splice(0, 0); });

  // Verify search, filter, iterator
  obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
  assertTrue(%HasPackedElements(obj));
  Object.seal(obj);
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));
  assertEquals(obj.lastIndexOf(1), 2);
  assertEquals(obj.indexOf('a'), 4);
  assertEquals(obj.indexOf(undefined), 0);
  assertFalse(obj.includes(Symbol("test")));
  assertTrue(obj.includes(undefined));
  assertFalse(obj.includes(NaN));
  assertTrue(obj.includes());
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
};
obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.seal(obj);
testPackedSealedArray1(obj);

// Verify after transition from preventExtensions
obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.preventExtensions(obj);
Object.seal(obj);
testPackedSealedArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for sealed packed array
function testPackedSealedArray2(arr) {
  assertTrue(Object.isSealed(arr));
  assertFalse(Object.isFrozen(arr));
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
}

var arr1 = new Array('a', 'b', 'c');
assertTrue(%HasPackedElements(arr1));
Object.seal(arr1);
testPackedSealedArray2(arr1);

var arr2 = new Array('a', 'b', 'c');
assertTrue(%HasPackedElements(arr2));
Object.preventExtensions(arr2);
Object.seal(arr2);
testPackedSealedArray2(arr2);

// Test regression with Object.defineProperty
var obj = [];
obj.propertyA = 42;
obj[0] = true;
Object.seal(obj);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
});
assertEquals(obj, obj.propertyA);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
});
obj.propertyA = 42;
assertEquals(obj, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple array
var arr = ['a'];
Object.seal(arr);
arr[0] = 'b';
assertEquals(arr[0], 'b');

// Test regression Array.concat with double
var arr = ['a'];
Object.seal(arr);
arr = arr.concat(0.5);
assertEquals(arr, ['a', 0.5]);
Object.seal(arr);
arr = arr.concat([1.5, 'b']);
assertEquals(arr, ['a', 0.5, 1.5, 'b']);

// Regression test with change length
var arr = ['a', 'b'];
Object.seal(arr);
assertEquals(arr.length, 2);
arr.length = 3;
assertEquals(arr.length, 3);
arr[2] = 'c';
assertEquals(arr[2], undefined);
arr.length = 1;
assertEquals(arr.length, 2);

// Start testing for holey element array
// Test holey element array built-in functions with seal.
function testHoleySealedArray1(obj) {
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));

  // Verify that the length can't be written by builtins.
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(1); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(1); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertDoesNotThrow(function() { obj.splice(0, 0); });

  // Verify search, filter, iterator
  obj = [undefined, null, 1, , -1, 'a', Symbol("test")];
  assertTrue(%HasHoleyElements(obj));
  Object.seal(obj);
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));
  assertEquals(obj.lastIndexOf(1), 2);
  assertEquals(obj.indexOf('a'), 5);
  assertEquals(obj.indexOf(undefined), 0);
  assertFalse(obj.includes(Symbol("test")));
  assertTrue(obj.includes(undefined));
  assertFalse(obj.includes(NaN));
  assertTrue(obj.includes());
  assertEquals(obj.find(x => x==0), undefined);
  assertEquals(obj.findIndex(x => x=='a'), 5);
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
  for (var i = 0; i < length; i++) {
    if (i==3) continue;
    obj[i] = 'new';
    assertEquals(obj[i], 'new');
  }
};
obj = [undefined, null, 1, , -1, 'a', Symbol("test")];
assertTrue(%HasHoleyElements(obj));
Object.seal(obj);
testHoleySealedArray1(obj);

// Verify after transition from preventExtensions
obj = [undefined, null, 1, , -1, 'a', Symbol("test")];
assertTrue(%HasHoleyElements(obj));
Object.preventExtensions(obj);
Object.seal(obj);
testHoleySealedArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for sealed holey array
function testHoleySealedArray2(arr) {
  assertTrue(Object.isSealed(arr));
  assertFalse(Object.isFrozen(arr));
  assertEquals(arr.map(x => [x]), [, ['a'], ['b'], ['c']]);
  assertEquals(arr.flatMap(x => [x]), ["a", "b", "c"]);
  assertEquals(arr.flat(), ["a", "b", "c"]);
  assertEquals(arr.join('-'), "-a-b-c");
  const reducer = (accumulator, currentValue) => accumulator + currentValue;
  assertEquals(arr.reduce(reducer), "abc");
  assertEquals(arr.reduceRight(reducer), "cba");
  assertEquals(arr.slice(0, 1), [,]);
  assertEquals(arr.slice(1, 2), ["a"]);
  // Verify change content of sealed holey array
  assertThrows(function(){arr.sort();}, TypeError);
  assertEquals(arr.join(''), "abc");
  assertThrows(function(){arr.reverse();}, TypeError);
  assertEquals(arr.join(''), "abc");
  assertThrows(function(){arr.copyWithin(0, 1, 2);}, TypeError);
  assertEquals(arr.join(''),"abc");
  arr.copyWithin(1, 2, 3);
  assertEquals(arr.join(''),"bbc");
  assertThrows(function(){arr.fill('d');}, TypeError);
  assertEquals(arr.join(''), "bbc");
}

var arr1 = [, 'a', 'b', 'c'];
assertTrue(%HasHoleyElements(arr1));
Object.seal(arr1);
testHoleySealedArray2(arr1);

var arr2 = [, 'a', 'b', 'c'];
assertTrue(%HasHoleyElements(arr2));
Object.preventExtensions(arr2);
Object.seal(arr2);
testHoleySealedArray2(arr2);

// Test regression with Object.defineProperty
var obj = ['a', , 'b'];
obj.propertyA = 42;
obj[0] = true;
Object.seal(obj);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
});
assertEquals(obj, obj.propertyA);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
});
obj.propertyA = 42;
assertEquals(obj, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple holey array
var arr = [, 'a'];
Object.seal(arr);
arr[1] = 'b';
assertEquals(arr[1], 'b');
arr[0] = 1;
assertEquals(arr[0], undefined);

// Test regression Array.concat with double
var arr = ['a', , 'b'];
Object.seal(arr);
arr = arr.concat(0.5);
assertEquals(arr, ['a', ,'b', 0.5]);
Object.seal(arr);
arr = arr.concat([1.5, 'c']);
assertEquals(arr, ['a', ,'b', 0.5, 1.5, 'c']);

// Regression test with change length
var arr = ['a', ,'b'];
Object.seal(arr);
assertEquals(arr.length, 3);
arr.length = 4;
assertEquals(arr.length, 4);
arr[3] = 'c';
assertEquals(arr[3], undefined);
arr.length = 2;
assertEquals(arr.length, 3);

// Change length with holey entries at the end
var arr = ['a', ,];
Object.seal(arr);
assertEquals(arr.length, 2);
arr.length = 0;
assertEquals(arr.length, 1);
arr.length = 3;
assertEquals(arr.length, 3);
arr.length = 0;
assertEquals(arr.length, 1);

// Spread with array
var arr = ['a', 'b', 'c'];
Object.seal(arr);
var arrSpread = [...arr];
assertEquals(arrSpread.length, arr.length);
assertEquals(arrSpread[0], 'a');
assertEquals(arrSpread[1], 'b');
assertEquals(arrSpread[2], 'c');

// Spread with array-like
function returnArgs() {
  return Object.seal(arguments);
}
var arrLike = returnArgs('a', 'b', 'c');
assertTrue(Object.isSealed(arrLike));
var arrSpread = [...arrLike];
assertEquals(arrSpread.length, arrLike.length);
assertEquals(arrSpread[0], 'a');
assertEquals(arrSpread[1], 'b');
assertEquals(arrSpread[2], 'c');

// Spread with holey
function countArgs() {
  return arguments.length;
}
var arr = [, 'b','c'];
Object.seal(arr);
assertEquals(countArgs(...arr), 3);
assertEquals(countArgs(...[...arr]), 3);
assertEquals(countArgs.apply(this, [...arr]), 3);
function checkUndefined() {
  return arguments[0] === undefined;
}
assertTrue(checkUndefined(...arr));
assertTrue(checkUndefined(...[...arr]));
assertTrue(checkUndefined.apply(this, [...arr]));

//
// Array.prototype.map
//
(function() {
  var a = Object.seal(['0','1','2','3','4']);

  // Simple use.
  var result = [1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n) { return Number(n) + 1; }));

  // Use specified object as this object when calling the function.
  var o = { delta: 42 }
  result = [42,43,44,45,46];
  assertArrayEquals(result, a.map(function(n) { return this.delta + Number(n); }, o));

  // Modify original array.
  b = Object.seal(['0','1','2','3','4']);
  result = [1,2,3,4,5];
  assertArrayEquals(result,
      b.map(function(n, index, array) {
        array[index] = Number(n) + 1; return Number(n) + 1;
      }));
  assertArrayEquals(b, result);

  // Only loop through initial part of array and elements are not
  // added.
  a = Object.seal(['0','1','2','3','4']);
  result = [1,2,3,4,5];
  assertArrayEquals(result,
      a.map(function(n, index, array) { assertThrows(() => { array.push(n) }); return Number(n) + 1; }));
  assertArrayEquals(['0','1','2','3','4'], a);

  // Respect holes.
  a = new Array(20);
  a[1] = '2';
  Object.seal(a);
  a = Object.seal(a).map(function(n) { return 2*Number(n); });

  for (var i in a) {
    assertEquals(4, a[i]);
    assertEquals('1', i);
  }

  // Skip over missing properties.
  a = {
    "0": 1,
    "2": 2,
    length: 3
  };
  var received = [];
  assertArrayEquals([2, , 4],
      Array.prototype.map.call(Object.seal(a), function(n) {
        received.push(n);
        return n * 2;
      }));
  assertArrayEquals([1, 2], received);

  // Modify array prototype
  a = ['1', , 2];
  received = [];
  assertThrows(() => {
    Array.prototype.map.call(Object.seal(a), function(n) {
      a.__proto__ = null;
      received.push(n);
      return n * 2;
    });
  }, TypeError);
  assertArrayEquals([], received);

  // Create a new object in each function call when receiver is a
  // primitive value. See ECMA-262, Annex C.
  a = [];
  Object.seal(['1', '2']).map(function() { a.push(this) }, "");
  assertTrue(a[0] !== a[1]);

  // Do not create a new object otherwise.
  a = [];
  Object.seal(['1', '2']).map(function() { a.push(this) }, {});
  assertSame(a[0], a[1]);

  // In strict mode primitive values should not be coerced to an object.
  a = [];
  Object.seal(['1', '2']).map(function() { 'use strict'; a.push(this); }, "");
  assertEquals("", a[0]);
  assertEquals(a[0], a[1]);

})();


// Test with double elements
// Test packed element array built-in functions with seal.
function testDoubleSealedArray1(obj) {
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));

  // Verify that the length can't be written by builtins.
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(1); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(1); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertDoesNotThrow(function() { obj.splice(0, 0); });

  // Verify search, filter, iterator
  assertEquals(obj.lastIndexOf(1), 1);
  assertEquals(obj.indexOf(undefined), -1);
  assertFalse(obj.includes(Symbol("test")));
  assertTrue(obj.includes(1));
  assertTrue(obj.includes(-1.1));
  assertFalse(obj.includes());
  assertEquals(obj.find(x => x==0), undefined);
  assertEquals(obj.findIndex(x => x==2), 3);
  assertFalse(obj.some(x => typeof x == 'symbol'));
  assertFalse(obj.every(x => x == -1));
  var filteredArray = obj.filter(e => typeof e == "symbol");
  assertEquals(filteredArray.length, 0);
  assertEquals(obj.map(x => x), obj);
  var countPositiveNumber = 0;
  obj.forEach(function(item, index) {
    if (item === 1) {
      countPositiveNumber++;
      assertEquals(index, 1);
    }
  });
  assertEquals(countPositiveNumber, 1);
  assertEquals(obj.length, obj.concat([]).length);
  var iterator = obj.values();
  assertEquals(iterator.next().value, -1.1);
  assertEquals(iterator.next().value, 1);
  var iterator = obj.keys();
  assertEquals(iterator.next().value, 0);
  assertEquals(iterator.next().value, 1);
  var iterator = obj.entries();
  assertEquals(iterator.next().value, [0, -1.1]);
  assertEquals(iterator.next().value, [1, 1]);

  // Verify that the value can't be written
  var length = obj.length;
  for (var i = 0; i < length; i++) {
    obj[i] = 'new';
    assertEquals('new', obj[i]);
  }
}

obj = new Array(1.1, -1.1, 1, -1, 2);
assertTrue(%HasDoubleElements(obj));
Object.seal(obj);
testDoubleSealedArray1(obj);

// Verify change from non-extensible to sealed
obj = new Array(1.1, -1.1, 1, -1, 2);
assertTrue(%HasDoubleElements(obj));
Object.preventExtensions(obj);
Object.seal(obj);
assertTrue(Object.isSealed(obj));
testDoubleSealedArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for sealed packed array
function testDoubleSealedArray2(arr) {
  assertTrue(Object.isSealed(arr));
  assertTrue(Array.isArray(arr));
  assertEquals(arr.map(x => [x]), [[1], [1.1], [0]]);
  assertEquals(arr.flatMap(x => [x]), arr);
  assertEquals(arr.flat(), arr);
  assertEquals(arr.join('-'), "1-1.1-0");
  const reducer = (accumulator, currentValue) => accumulator + currentValue;
  assertEquals(arr.reduce(reducer), 2.1);
  assertEquals(arr.reduceRight(reducer), 2.1);
  assertEquals(arr.slice(0, 1), [1]);
}
var arr1 = new Array(1, 1.1, 0);
assertTrue(%HasDoubleElements(arr1));
Object.seal(arr1);
testDoubleSealedArray2(arr1);

// Verify change from non-extensible to sealed
var arr1 = new Array(1, 1.1, 0);
assertTrue(%HasDoubleElements(arr1));
Object.preventExtensions(arr1);
Object.seal(arr1);
testDoubleSealedArray2(arr1);

// Test regression with Object.defineProperty
var obj = [];
obj.propertyA = 42;
obj[0] = 1.1;
Object.seal(obj);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
});
assertEquals(obj, obj.propertyA);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
});
obj.propertyA = 42;
assertEquals(obj, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple array
var arr = [1.1];
Object.seal(arr);
arr[0] = 1;
assertEquals(arr[0], 1);

// Test regression Array.concat with double
var arr = [1.1];
Object.seal(arr);
arr = arr.concat(0.5);
assertEquals(arr, [1.1, 0.5]);
Object.seal(arr);
arr = arr.concat([1.5, 'b']);
assertEquals(arr, [1.1, 0.5, 1.5, 'b']);

// Regression test with change length
var arr = [1.1, 0];
Object.seal(arr);
assertEquals(arr.length, 2);
arr.length = 3;
assertEquals(arr.length, 3);
arr[2] = 'c';
assertEquals(arr[2], undefined);
arr.length = 1;
assertEquals(arr.length, 2);

// Start testing for holey double element array
// Test holey double element array built-in functions with seal.
function testHoleyDoubleSealedArray1() {
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));

  // Verify that the length can't be written by builtins.
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(1); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(1); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertDoesNotThrow(function() { obj.splice(0, 0); });

  // Verify search, filter, iterator
  obj = [-1.1, 0, 1, , -1, 1.1];
  assertTrue(%HasHoleyElements(obj));
  Object.seal(obj);
  assertTrue(Object.isSealed(obj));
  assertFalse(Object.isFrozen(obj));
  assertTrue(Array.isArray(obj));
  assertEquals(obj.lastIndexOf(1), 2);
  assertEquals(obj.indexOf(1.1), 5);
  assertEquals(obj.indexOf(undefined), -1);
  assertFalse(obj.includes(Symbol("test")));
  assertTrue(obj.includes(undefined));
  assertFalse(obj.includes(NaN));
  assertTrue(obj.includes());
  assertEquals(obj.find(x => x==0), 0);
  assertEquals(obj.findIndex(x => x==1.1), 5);
  assertFalse(obj.some(x => typeof x == 'symbol'));
  assertFalse(obj.every(x => x == -1));
  var filteredArray = obj.filter(e => typeof e == "symbol");
  assertEquals(filteredArray.length, 0);
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
  assertEquals(iterator.next().value, -1.1);
  assertEquals(iterator.next().value, 0);
  var iterator = obj.keys();
  assertEquals(iterator.next().value, 0);
  assertEquals(iterator.next().value, 1);
  var iterator = obj.entries();
  assertEquals(iterator.next().value, [0, -1.1]);
  assertEquals(iterator.next().value, [1, 0]);

  // Verify that the value can be written
  var length = obj.length;
  for (var i = 0; i < length; i++) {
    if (i==3) continue;
    obj[i] = 'new';
    assertEquals(obj[i], 'new');
  }
};

obj = [-1.1, 0, 1, , -1, 1.1];
assertTrue(%HasHoleyElements(obj));
Object.seal(obj);
testHoleyDoubleSealedArray1(obj);

// Verify change from non-extensible to sealed
obj = [-1.1, 0, 1, , -1, 1.1];
assertTrue(%HasHoleyElements(obj));
Object.preventExtensions(obj);
Object.seal(obj);
assertTrue(Object.isSealed(obj));
testHoleyDoubleSealedArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for sealed packed array
function testHoleyDoubleSealedArray2(arr) {
  assertTrue(Object.isSealed(arr));
  assertTrue(Array.isArray(arr));
  assertEquals(arr.map(x => [x]), [, [1.1], [1], [0]]);
  assertEquals(arr.flatMap(x => [x]), [1.1, 1, 0]);
  assertEquals(arr.flat(), [1.1, 1, 0]);
  assertEquals(arr.join('-'), "-1.1-1-0");
  const reducer = (accumulator, currentValue) => accumulator + currentValue;
  assertEquals(arr.reduce(reducer), 2.1);
  assertEquals(arr.reduceRight(reducer), 2.1);
  assertEquals(arr.slice(0, 1), [,]);
  assertEquals(arr.slice(1, 2), [1.1]);
}
var arr1 = [, 1.1, 1, 0];
assertTrue(%HasHoleyElements(arr1));
Object.seal(arr1);
testHoleyDoubleSealedArray2(arr1);

// Verify change from non-extensible to sealed
var arr1 = [, 1.1, 1, 0];
assertTrue(%HasHoleyElements(arr1));
Object.preventExtensions(arr1);
Object.seal(arr1);
testHoleyDoubleSealedArray2(arr1);

// Test regression with Object.defineProperty
var obj = [1.1, , 0];
obj.propertyA = 42;
obj[0] = 1.2;
Object.seal(obj);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
});
assertEquals(obj, obj.propertyA);
assertDoesNotThrow(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
});
obj.propertyA = 42;
assertEquals(obj, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple holey array
var arr = [, 1.1];
Object.seal(arr);
arr[1] = 'b';
assertEquals(arr[1], 'b');
arr[0] = 1;
assertEquals(arr[0], undefined);

// Test regression Array.concat with double
var arr = [1.1, , 0];
Object.seal(arr);
arr = arr.concat(0.5);
assertEquals(arr, [1.1, , 0, 0.5]);
Object.seal(arr);
arr = arr.concat([1.5, 'c']);
assertEquals(arr, [1.1, ,0, 0.5, 1.5, 'c']);

// Regression test with change length
var arr = [1.1, ,0];
Object.seal(arr);
assertEquals(arr.length, 3);
arr.length = 4;
assertEquals(arr.length, 4);
arr[3] = 'c';
assertEquals(arr[2], 0);
assertEquals(arr[3], undefined);
arr.length = 2;
assertEquals(arr.length, 3);

// Change length with holey entries at the end
var arr = [1.1, ,];
Object.seal(arr);
assertEquals(arr.length, 2);
arr.length = 0;
assertEquals(arr.length, 1);
arr.length = 3;
assertEquals(arr.length, 3);
arr.length = 0;
assertEquals(arr.length, 1);

// Spread with array
var arr = [1.1, 0, -1];
Object.seal(arr);
var arrSpread = [...arr];
assertEquals(arrSpread.length, arr.length);
assertEquals(arrSpread[0], 1.1);
assertEquals(arrSpread[1], 0);
assertEquals(arrSpread[2], -1);

// Spread with array-like
function returnArgs() {
  return Object.seal(arguments);
}
var arrLike = returnArgs(1.1, 0, -1);
assertTrue(Object.isSealed(arrLike));
var arrSpread = [...arrLike];
assertEquals(arrSpread.length, arrLike.length);
assertEquals(arrSpread[0], 1.1);
assertEquals(arrSpread[1], 0);
assertEquals(arrSpread[2], -1);

// Spread with holey
function countArgs() {
  return arguments.length;
}
var arr = [, 1.1, 0];
Object.seal(arr);
assertEquals(countArgs(...arr), 3);
assertEquals(countArgs(...[...arr]), 3);
assertEquals(countArgs.apply(this, [...arr]), 3);
function checkUndefined() {
  return arguments[0] === undefined;
}
assertTrue(checkUndefined(...arr));
assertTrue(checkUndefined(...[...arr]));
assertTrue(checkUndefined.apply(this, [...arr]));

//
// Array.prototype.map
//
(function() {
  var a = Object.seal([0.1,1,2,3,4]);

  // Simple use.
  var result = [1.1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n) { return Number(n) + 1; }));

  // Use specified object as this object when calling the function.
  var o = { delta: 42 }
  result = [42.1,43,44,45,46];
  assertArrayEquals(result, a.map(function(n) { return this.delta + Number(n); }, o));

  // Modify original array.
  b = Object.seal([0.1,1,2,3,4]);
  result = [1.1,2,3,4,5];
  assertArrayEquals(result,
      b.map(function(n, index, array) {
        array[index] = Number(n) + 1; return Number(n) + 1;
      }));
  assertArrayEquals(b, result);

  // Only loop through initial part of array and elements are not
  // added.
  a = Object.seal([0.1,1,2,3,4]);
  result = [1.1,2,3,4,5];
  assertArrayEquals(result,
      a.map(function(n, index, array) { assertThrows(() => { array.push(n) }); return Number(n) + 1; }));
  assertArrayEquals([0.1,1,2,3,4], a);

  // Respect holes.
  a = new Array(20);
  a[1] = 1.1;
  Object.seal(a);
  a = Object.seal(a).map(function(n) { return 2*Number(n); });

  for (var i in a) {
    assertEquals(2.2, a[i]);
    assertEquals('1', i);
  }

  // Skip over missing properties.
  a = {
    "0": 1.1,
    "2": 2,
    length: 3
  };
  var received = [];
  assertArrayEquals([2.2, , 4],
      Array.prototype.map.call(Object.seal(a), function(n) {
        received.push(n);
        return n * 2;
      }));
  assertArrayEquals([1.1, 2], received);

  // Modify array prototype
  a = [1.1, , 2];
  received = [];
  assertThrows(() => {
    Array.prototype.map.call(Object.seal(a), function(n) {
      a.__proto__ = null;
      received.push(n);
      return n * 2;
    });
  }, TypeError);
  assertArrayEquals([], received);

  // Create a new object in each function call when receiver is a
  // primitive value. See ECMA-262, Annex C.
  a = [];
  Object.seal([1.1, 2]).map(function() { a.push(this) }, "");
  assertTrue(a[0] !== a[1]);

  // Do not create a new object otherwise.
  a = [];
  Object.seal([1.1, 2]).map(function() { a.push(this) }, {});
  assertSame(a[0], a[1]);

  // In strict mode primitive values should not be coerced to an object.
  a = [];
  Object.seal([1.1, 1.2]).map(function() { 'use strict'; a.push(this); }, "");
  assertEquals("", a[0]);
  assertEquals(a[0], a[1]);

})();
