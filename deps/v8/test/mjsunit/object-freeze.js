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

// Tests the Object.freeze and Object.isFrozen methods - ES 19.1.2.5 and
// ES 19.1.2.12

// Flags: --allow-natives-syntax

// Test that we return obj if non-object is passed as argument
var non_objects = new Array(undefined, null, 1, -1, 0, 42.43, Symbol("test"));
for (var key in non_objects) {
  assertSame(non_objects[key], Object.freeze(non_objects[key]));
}

// Test that isFrozen always returns true for non-objects
for (var key in non_objects) {
  assertTrue(Object.isFrozen(non_objects[key]));
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
assertFalse(Object.isFrozen(obj));

Object.freeze(obj);

// Make sure we are no longer extensible.
assertFalse(Object.isExtensible(obj));
assertTrue(Object.isFrozen(obj));

obj.foo = 42;
assertEquals(obj.foo, undefined);

desc = Object.getOwnPropertyDescriptor(obj, 'x');
assertFalse(desc.writable);
assertFalse(desc.configurable);
assertEquals(42, desc.value);

desc = Object.getOwnPropertyDescriptor(obj, 'z');
assertFalse(desc.writable);
assertFalse(desc.configurable);
assertEquals("foobar", desc.value);

// Make sure that even if we try overwrite a value that is not writable, it is
// not changed.
obj.x = "tete";
assertEquals(42, obj.x);
obj.x = { get: function() {return 43}, set: function() {} };
assertEquals(42, obj.x);

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
assertFalse(Object.isFrozen(obj2));
Object.freeze(obj2);
assertTrue(Object.isFrozen(obj2));
assertFalse(Object.isExtensible(obj2));

desc = Object.getOwnPropertyDescriptor(obj2, 'x');
assertFalse(desc.configurable);
assertEquals(undefined, desc.value);
assertEquals(set, desc.set);
assertEquals(get, desc.get);

obj2.foo = 42;
assertEquals(obj2.foo, undefined);


// Test freeze on arrays.
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
assertFalse(Object.isFrozen(arr));
Object.freeze(arr);
assertTrue(Object.isFrozen(arr));
assertFalse(Object.isExtensible(arr));

desc = Object.getOwnPropertyDescriptor(arr, '0');
assertFalse(desc.configurable);
assertFalse(desc.writable);
assertEquals(42, desc.value);

desc = Object.getOwnPropertyDescriptor(arr, '1');
assertFalse(desc.configurable);
assertFalse(desc.writable);
assertEquals(43, desc.value);

arr[0] = 'foo';

assertEquals(arr[0], 42);


// Test that isFrozen return the correct value even if configurable has been set
// to false on all properties manually and the extensible flag has also been set
// to false manually.
var obj3 = { x: 42, y: 'foo' };

assertFalse(Object.isFrozen(obj3));

Object.defineProperty(obj3, 'x', {configurable: false, writable: false});
Object.defineProperty(obj3, 'y', {configurable: false, writable: false});
Object.preventExtensions(obj3);

assertTrue(Object.isFrozen(obj3));


// Make sure that an object that has only non-configurable, but one
// writable property, is not classified as frozen.
var obj4 = {};
Object.defineProperty(obj4, 'x', {configurable: false, writable: true});
Object.defineProperty(obj4, 'y', {configurable: false, writable: false});
Object.preventExtensions(obj4);

assertFalse(Object.isFrozen(obj4));

// Make sure that an object that has only non-writable, but one
// configurable property, is not classified as frozen.
var obj5 = {};
Object.defineProperty(obj5, 'x', {configurable: true, writable: false});
Object.defineProperty(obj5, 'y', {configurable: false, writable: false});
Object.preventExtensions(obj5);

assertFalse(Object.isFrozen(obj5));

// Make sure that Object.freeze returns the frozen object.
var obj6 = {}
assertTrue(obj6 === Object.freeze(obj6))

// Test that the enumerable attribute is unperturbed by freezing.
obj = { x: 42, y: 'foo' };
Object.defineProperty(obj, 'y', {enumerable: false});
Object.freeze(obj);
assertTrue(Object.isFrozen(obj));
desc = Object.getOwnPropertyDescriptor(obj, 'x');
assertTrue(desc.enumerable);
desc = Object.getOwnPropertyDescriptor(obj, 'y');
assertFalse(desc.enumerable);

// Fast properties should remain fast
obj = { x: 42, y: 'foo' };
assertTrue(%HasFastProperties(obj));
Object.freeze(obj);
assertTrue(Object.isFrozen(obj));
assertTrue(%HasFastProperties(obj));

// Frozen objects should share maps where possible
obj = { prop1: 1, prop2: 2 };
obj2 = { prop1: 3, prop2: 4 };
assertTrue(%HaveSameMap(obj, obj2));
Object.freeze(obj);
Object.freeze(obj2);
assertTrue(Object.isFrozen(obj));
assertTrue(Object.isFrozen(obj2));
assertTrue(%HaveSameMap(obj, obj2));

// Frozen objects should share maps even when they have elements
obj = { prop1: 1, prop2: 2, 75: 'foo' };
obj2 = { prop1: 3, prop2: 4, 150: 'bar' };
assertTrue(%HaveSameMap(obj, obj2));
Object.freeze(obj);
Object.freeze(obj2);
assertTrue(Object.isFrozen(obj));
assertTrue(Object.isFrozen(obj2));
assertTrue(%HaveSameMap(obj, obj2));

// Setting elements after freezing should not be allowed
obj = { prop: 'thing' };
Object.freeze(obj);
assertTrue(Object.isFrozen(obj));
obj[0] = 'hello';
assertFalse(obj.hasOwnProperty(0));

// Freezing an object in dictionary mode should work
// Also testing that getter/setter properties work after freezing
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
Object.freeze(obj);
assertFalse(%HasFastProperties(obj));
assertTrue(Object.isFrozen(obj));
assertFalse(Object.isExtensible(obj));
for (var i = 0; i < 100; ++i) {
  desc = Object.getOwnPropertyDescriptor(obj, 'x' + i);
  assertFalse(desc.writable);
  assertFalse(desc.configurable);
}
assertEquals(42, obj.accessor);
assertFalse(accessorDidRun);
obj.accessor = 'ignored value';
assertTrue(accessorDidRun);

// Freezing arguments should work
var func = function(arg) {
  Object.freeze(arguments);
  assertTrue(Object.isFrozen(arguments));
};
func('hello', 'world');
func('goodbye', 'world');

// Freezing sparse arrays
var sparseArr = [0, 1];
sparseArr[10000] = 10000;
Object.freeze(sparseArr);
assertTrue(Object.isFrozen(sparseArr));

// Accessors on fast object should behavior properly after freezing
obj = {};
Object.defineProperty(obj, 'accessor', {
  get: function() { return 42 },
  set: function() { accessorDidRun = true },
  configurable: true,
  enumerable: true
});
assertTrue(%HasFastProperties(obj));
Object.freeze(obj);
assertTrue(Object.isFrozen(obj));
assertTrue(%HasFastProperties(obj));
assertEquals(42, obj.accessor);
accessorDidRun = false;
obj.accessor = 'ignored value';
assertTrue(accessorDidRun);

// Test for regression in mixed accessor/data property objects.
// The strict function is one such object.
assertTrue(Object.isFrozen(Object.freeze(function(){"use strict";})));

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
Object.freeze(obj);
assertTrue(%HasFastProperties(obj));
assertTrue(Object.isFrozen(obj));

// Test array built-in functions with freeze.
obj = [1,2,3];
Object.freeze(obj);
// if frozen implies sealed, then the tests in object-seal.js are mostly
// sufficient.
assertTrue(Object.isSealed(obj));

// Verify that the length can't be written by builtins.
assertThrows(function() { obj.push(); }, TypeError);
assertThrows(function() { obj.unshift(); }, TypeError);
assertThrows(function() { obj.splice(0,0); }, TypeError);
assertTrue(Object.isFrozen(obj));

// Verify that an item can't be changed with splice.
assertThrows(function() { obj.splice(0,1,1); }, TypeError);
assertTrue(Object.isFrozen(obj));

// Verify that unshift() with no arguments will fail if it reifies from
// the prototype into the object.
obj = [1,,3];
obj.__proto__[1] = 1;
assertEquals(1, obj[1]);
Object.freeze(obj);
assertThrows(function() { obj.unshift(); }, TypeError);

// Sealing and then Freezing should do the right thing.
var obj = { foo: 'bar', 0: 'element' };
Object.seal(obj);
assertTrue(Object.isSealed(obj));
assertFalse(Object.isFrozen(obj));
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
assertTrue(Object.isFrozen(obj));


(function propertiesOfFrozenObjectNotFrozen() {
  function Frozen() {}
  Object.freeze(Frozen);
  assertDoesNotThrow(function() { return new Frozen(); });
  Frozen.prototype.prototypeExists = true;
  assertTrue((new Frozen()).prototypeExists);
})();


(function frozenPrototypePreventsPUT() {
  // A read-only property on the prototype should prevent a [[Put]] .
  function Constructor() {}
  Constructor.prototype.foo = 1;
  Object.freeze(Constructor.prototype);
  var obj = new Constructor();
  obj.foo = 2;
  assertSame(1, obj.foo);
})();


(function frozenFunctionSloppy() {
  // Check that freezing a function works correctly.
  var func = Object.freeze(function foo(){});
  assertTrue(Object.isFrozen(func));
  func.prototype = 42;
  assertFalse(func.prototype === 42);
  assertFalse(Object.getOwnPropertyDescriptor(func, "prototype").writable);
})();


(function frozenFunctionStrict() {
  // Check that freezing a strict function works correctly.
  var func = Object.freeze(function foo(){ "use strict"; });
  assertTrue(Object.isFrozen(func));
  func.prototype = 42;
  assertFalse(func.prototype === 42);
  assertFalse(Object.getOwnPropertyDescriptor(func, "prototype").writable);
})();


(function frozenArrayObject() {
  // Check that freezing array objects works correctly.
  var array = Object.freeze([0,1,2]);
  assertTrue(Object.isFrozen(array));
  array[0] = 3;
  assertEquals(0, array[0]);
  assertFalse(Object.getOwnPropertyDescriptor(array, "length").writable);
})();


(function frozenArgumentsObject() {
  // Check that freezing arguments objects works correctly.
  var args = Object.freeze((function(){ return arguments; })(0,1,2));
  assertTrue(Object.isFrozen(args));
  args[0] = 3;
  assertEquals(0, args[0]);
  assertFalse(Object.getOwnPropertyDescriptor(args, "length").writable);
  assertFalse(Object.getOwnPropertyDescriptor(args, "callee").writable);
})();

// Test packed element array built-in functions with freeze.
function testPackedFrozenArray1(obj) {
  assertTrue(Object.isSealed(obj));
  // Verify that the value can't be written
  obj1 = new Array(...obj);
  var length = obj.length;
  for (var i = 0; i < length-1; i++) {
    obj[i] = 'new';
    assertEquals(obj1[i], obj[i]);
  }
  // for symbol we cannot compare directly
  assertTrue(typeof obj[length-1] == 'symbol');

  // Verify that the length can't be written by builtins.
  assertTrue(Array.isArray(obj));
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(); }, TypeError);
  assertThrows(function() { obj.copyWithin(0,0); }, TypeError);
  assertThrows(function() { obj.fill(0); }, TypeError);
  assertThrows(function() { obj.reverse(); }, TypeError);
  assertThrows(function() { obj.sort(); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertThrows(function() { obj.splice(0, 0); }, TypeError);
  assertTrue(Object.isFrozen(obj));

  // Verify search, filter, iterator
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
}

obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.freeze(obj);
testPackedFrozenArray1(obj);

// Verify change from sealed to frozen
obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.seal(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testPackedFrozenArray1(obj);

// Verify change from non-extensible to frozen
obj = new Array(undefined, null, 1, -1, 'a', Symbol("test"));
assertTrue(%HasPackedElements(obj));
Object.preventExtensions(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testPackedFrozenArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for frozen packed array
function testPackedFrozenArray2(arr) {
  assertTrue(Object.isFrozen(arr));
  assertTrue(Array.isArray(arr));
  assertEquals(arr.map(x => [x]), [['a'], ['b'], ['c']]);
  assertEquals(arr.flatMap(x => [x]), arr);
  assertEquals(arr.flat(), arr);
  assertEquals(arr.join('-'), "a-b-c");
  const reducer = (accumulator, currentValue) => accumulator + currentValue;
  assertEquals(arr.reduce(reducer), "abc");
  assertEquals(arr.reduceRight(reducer), "cba");
  assertEquals(arr.slice(0, 1), ['a']);
}
var arr1 = new Array('a', 'b', 'c');
assertTrue(%HasPackedElements(arr1));
Object.freeze(arr1);
testPackedFrozenArray2(arr1);

// Verify change from sealed to frozen
var arr2 = new Array('a', 'b', 'c');
assertTrue(%HasPackedElements(arr2));
Object.seal(arr2);
Object.freeze(arr2);
testPackedFrozenArray2(arr2);

// Verify change from non-extensible to frozen
var arr2 = new Array('a', 'b', 'c');
assertTrue(%HasPackedElements(arr2));
Object.preventExtensions(arr2);
Object.freeze(arr2);
testPackedFrozenArray2(arr2);

// Verify that repeatedly attemping to freeze a typed array fails
var typedArray = new Uint8Array(10);
assertThrows(() => { Object.freeze(typedArray); }, TypeError);
assertFalse(Object.isFrozen(typedArray));
assertThrows(() => { Object.freeze(typedArray); }, TypeError);
assertFalse(Object.isFrozen(typedArray));

// Verify that freezing an empty typed array works
var typedArray = new Uint8Array(0);
Object.freeze(typedArray);
assertTrue(Object.isFrozen(typedArray));

// Test regression with Object.defineProperty
var obj = [];
obj.propertyA = 42;
obj[0] = true;
Object.freeze(obj);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
}, TypeError);
assertEquals(42, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
}, TypeError);
assertDoesNotThrow(function() {obj.propertyA = 2;});
assertEquals(obj.propertyA, 42);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple array
var arr = ['a'];
Object.freeze(arr);
arr[0] = 'b';
assertEquals(arr[0], 'a');

// Test regression Array.concat with double
var arr = ['a'];
Object.freeze(arr);
arr = arr.concat(0.5);
assertEquals(arr, ['a', 0.5]);
Object.freeze(arr);
arr = arr.concat([1.5, 'b']);
assertEquals(arr, ['a', 0.5, 1.5, 'b']);

// Regression test with change length
var arr = ['a', 'b'];
Object.freeze(arr);
assertEquals(arr.length, 2);
arr.length = 3;
assertEquals(arr.length, 2);
arr[2] = 'c';
assertEquals(arr[2], undefined);
arr.length = 1;
assertEquals(arr.length, 2);

// Start testing with holey array
// Test holey element array built-in functions with freeze.
function testHoleyFrozenArray1(obj) {
  assertTrue(Object.isSealed(obj));
  // Verify that the value can't be written
  obj1 = new Array(...obj);
  var length = obj.length;
  for (var i = 0; i < length-1; i++) {
    obj[i] = 'new';
    assertEquals(obj1[i], obj[i]);
  }
  // for symbol we cannot compare directly
  assertTrue(typeof obj[length-1] == 'symbol');

  // Verify that the length can't be written by builtins.
  assertTrue(Array.isArray(obj));
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(); }, TypeError);
  assertThrows(function() { obj.copyWithin(0,0); }, TypeError);
  assertThrows(function() { obj.fill(0); }, TypeError);
  assertThrows(function() { obj.reverse(); }, TypeError);
  assertThrows(function() { obj.sort(); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertThrows(function() { obj.splice(0, 0); }, TypeError);
  assertTrue(Object.isFrozen(obj));

  // Verify search, filter, iterator
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
}

obj = [undefined, null, 1, , -1, 'a', Symbol("test")];
assertTrue(%HasHoleyElements(obj));
Object.freeze(obj);
testHoleyFrozenArray1(obj);

// Verify change from sealed to frozen
obj = [undefined, null, 1, , -1, 'a', Symbol("test")];
assertTrue(%HasHoleyElements(obj));
Object.seal(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testHoleyFrozenArray1(obj);

// Verify change from non-extensible to frozen
obj = [undefined, null, 1, ,-1, 'a', Symbol("test")];
assertTrue(%HasHoleyElements(obj));
Object.preventExtensions(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testHoleyFrozenArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for frozen packed array
function testHoleyFrozenArray2(arr) {
  assertTrue(Object.isFrozen(arr));
  assertTrue(Array.isArray(arr));
  assertEquals(arr.map(x => [x]), [, ['a'], ['b'], ['c']]);
  assertEquals(arr.flatMap(x => [x]), ["a", "b", "c"]);
  assertEquals(arr.flat(), ["a", "b", "c"]);
  assertEquals(arr.join('-'), "-a-b-c");
  const reducer = (accumulator, currentValue) => accumulator + currentValue;
  assertEquals(arr.reduce(reducer), "abc");
  assertEquals(arr.reduceRight(reducer), "cba");
  assertEquals(arr.slice(0, 1), [,]);
  assertEquals(arr.slice(1, 2), ["a"]);
}
var arr1 = [, 'a', 'b', 'c'];
assertTrue(%HasHoleyElements(arr1));
Object.freeze(arr1);
testHoleyFrozenArray2(arr1);

// Verify change from sealed to frozen
var arr2 = [, 'a', 'b', 'c'];
assertTrue(%HasHoleyElements(arr2));
Object.seal(arr2);
Object.freeze(arr2);
testHoleyFrozenArray2(arr2);

// Verify change from non-extensible to frozen
var arr2 = [, 'a', 'b', 'c'];
assertTrue(%HasHoleyElements(arr2));
Object.preventExtensions(arr2);
Object.freeze(arr2);
testHoleyFrozenArray2(arr2);

// Test regression with Object.defineProperty
var obj = ['a', , 'b'];
obj.propertyA = 42;
obj[0] = true;
Object.freeze(obj);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
}, TypeError);
assertEquals(42, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
}, TypeError);
assertDoesNotThrow(function() {obj.propertyA = 2;});
assertEquals(obj.propertyA, 42);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple holey array
var arr = [, 'a'];
Object.freeze(arr);
arr[1] = 'b';
assertEquals(arr[1], 'a');
arr[0] = 1;
assertEquals(arr[0], undefined);

// Test regression Array.concat with double
var arr = ['a', , 'b'];
Object.freeze(arr);
arr = arr.concat(0.5);
assertEquals(arr, ['a', ,'b', 0.5]);
Object.freeze(arr);
arr = arr.concat([1.5, 'c']);
assertEquals(arr, ['a', ,'b', 0.5, 1.5, 'c']);

// Regression test with change length
var arr = ['a', ,'b'];
Object.freeze(arr);
assertEquals(arr.length, 3);
arr.length = 4;
assertEquals(arr.length, 3);
arr[3] = 'c';
assertEquals(arr[2], 'b');
assertEquals(arr[3], undefined);
arr.length = 2;
assertEquals(arr.length, 3);

// Change length with holey entries at the end
var arr = ['a', ,];
Object.freeze(arr);
assertEquals(arr.length, 2);
arr.length = 0;
assertEquals(arr.length, 2);
arr.length = 3;
assertEquals(arr.length, 2);
arr.length = 0;
assertEquals(arr.length, 2);

// Spread with array
var arr = ['a', 'b', 'c'];
Object.freeze(arr);
var arrSpread = [...arr];
assertEquals(arrSpread.length, arr.length);
assertEquals(arrSpread[0], 'a');
assertEquals(arrSpread[1], 'b');
assertEquals(arrSpread[2], 'c');

// Spread with array-like
function returnArgs() {
  return Object.freeze(arguments);
}
var arrLike = returnArgs('a', 'b', 'c');
assertTrue(Object.isFrozen(arrLike));
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
Object.freeze(arr);
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
  var a = Object.freeze(['0','1','2','3','4']);

  // Simple use.
  var result = [1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n) { return Number(n) + 1; }));

  // Use specified object as this object when calling the function.
  var o = { delta: 42 }
  result = [42,43,44,45,46];
  assertArrayEquals(result, a.map(function(n) { return this.delta + Number(n); }, o));

  // Modify original array.
  b = Object.freeze(['0','1','2','3','4']);
  result = [1,2,3,4,5];
  assertArrayEquals(result,
      b.map(function(n, index, array) {
        array[index] = Number(n) + 1; return Number(n) + 1;
      }));
  assertArrayEquals(b, a);

  // Only loop through initial part of array and elements are not
  // added.
  a = Object.freeze(['0','1','2','3','4']);
  result = [1,2,3,4,5];
  assertArrayEquals(result,
      a.map(function(n, index, array) { assertThrows(() => { array.push(n) }); return Number(n) + 1; }));
  assertArrayEquals(['0','1','2','3','4'], a);

  // Respect holes.
  a = new Array(20);
  a[1] = '2';
  Object.freeze(a);
  a = Object.freeze(a).map(function(n) { return 2*Number(n); });

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
      Array.prototype.map.call(Object.freeze(a), function(n) {
        received.push(n);
        return n * 2;
      }));
  assertArrayEquals([1, 2], received);

  // Modify array prototype
  a = ['1', , 2];
  received = [];
  assertThrows(() => {
    Array.prototype.map.call(Object.freeze(a), function(n) {
      a.__proto__ = null;
      received.push(n);
      return n * 2;
    });
  }, TypeError);
  assertArrayEquals([], received);

  // Create a new object in each function call when receiver is a
  // primitive value. See ECMA-262, Annex C.
  a = [];
  Object.freeze(['1', '2']).map(function() { a.push(this) }, "");
  assertTrue(a[0] !== a[1]);

  // Do not create a new object otherwise.
  a = [];
  Object.freeze(['1', '2']).map(function() { a.push(this) }, {});
  assertSame(a[0], a[1]);

  // In strict mode primitive values should not be coerced to an object.
  a = [];
  Object.freeze(['1', '2']).map(function() { 'use strict'; a.push(this); }, "");
  assertEquals("", a[0]);
  assertEquals(a[0], a[1]);

})();

// Test with double elements
// Test packed element array built-in functions with freeze.
function testDoubleFrozenArray1(obj) {
  assertTrue(Object.isSealed(obj));
  // Verify that the value can't be written
  obj1 = new Array(...obj);
  var length = obj.length;
  for (var i = 0; i < length-1; i++) {
    obj[i] = 'new';
    assertEquals(obj1[i], obj[i]);
  }
  // Verify that the length can't be written by builtins.
  assertTrue(Array.isArray(obj));
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(); }, TypeError);
  assertThrows(function() { obj.copyWithin(0,0); }, TypeError);
  assertThrows(function() { obj.fill(0); }, TypeError);
  assertThrows(function() { obj.reverse(); }, TypeError);
  assertThrows(function() { obj.sort(); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertThrows(function() { obj.splice(0, 0); }, TypeError);
  assertTrue(Object.isFrozen(obj));

  // Verify search, filter, iterator
  assertEquals(obj.lastIndexOf(1), 2);
  assertEquals(obj.indexOf(undefined), -1);
  assertFalse(obj.includes(Symbol("test")));
  assertTrue(obj.includes(1));
  assertTrue(obj.includes(-1.1));
  assertFalse(obj.includes());
  assertEquals(obj.find(x => x==0), undefined);
  assertEquals(obj.findIndex(x => x==2), 4);
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
  assertEquals(iterator.next().value, 1.1);
  assertEquals(iterator.next().value, -1.1);
  var iterator = obj.keys();
  assertEquals(iterator.next().value, 0);
  assertEquals(iterator.next().value, 1);
  var iterator = obj.entries();
  assertEquals(iterator.next().value, [0, 1.1]);
  assertEquals(iterator.next().value, [1, -1.1]);
}

obj = new Array(1.1, -1.1, 1, -1, 2);
assertTrue(%HasDoubleElements(obj));
Object.freeze(obj);
testDoubleFrozenArray1(obj);

// Verify change from sealed to frozen
obj = new Array(1.1, -1.1, 1, -1, 2);
assertTrue(%HasDoubleElements(obj));
Object.seal(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testDoubleFrozenArray1(obj);

// Verify change from non-extensible to frozen
obj = new Array(1.1, -1.1, 1, -1, 2);
assertTrue(%HasDoubleElements(obj));
Object.preventExtensions(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testDoubleFrozenArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for frozen packed array
function testDoubleFrozenArray2(arr) {
  assertTrue(Object.isFrozen(arr));
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
Object.freeze(arr1);
testDoubleFrozenArray2(arr1);

// Verify change from sealed to frozen
var arr1 = new Array(1, 1.1, 0);
assertTrue(%HasDoubleElements(arr1));
Object.seal(arr1);
Object.freeze(arr1);
testDoubleFrozenArray2(arr1);


// Verify change from non-extensible to frozen
var arr1 = new Array(1, 1.1, 0);
assertTrue(%HasDoubleElements(arr1));
Object.preventExtensions(arr1);
Object.freeze(arr1);
testDoubleFrozenArray2(arr1);

// Test regression with Object.defineProperty
var obj = [];
obj.propertyA = 42;
obj[0] = 1.1;
Object.freeze(obj);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
}, TypeError);
assertEquals(42, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
}, TypeError);
assertDoesNotThrow(function() {obj.propertyA = 2;});
assertEquals(obj.propertyA, 42);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple array
var arr = [1.1];
Object.freeze(arr);
arr[0] = 1;
assertEquals(arr[0], 1.1);

// Test regression Array.concat with double
var arr = [1.1];
Object.freeze(arr);
arr = arr.concat(0.5);
assertEquals(arr, [1.1, 0.5]);
Object.freeze(arr);
arr = arr.concat([1.5, 'b']);
assertEquals(arr, [1.1, 0.5, 1.5, 'b']);

// Regression test with change length
var arr = [1.1, 0];
Object.freeze(arr);
assertEquals(arr.length, 2);
arr.length = 3;
assertEquals(arr.length, 2);
arr[2] = 'c';
assertEquals(arr[2], undefined);
arr.length = 1;
assertEquals(arr.length, 2);

// Start testing with holey array
// Test holey element array built-in functions with freeze.
function testHoleyDoubleFrozenArray1(obj) {
  assertTrue(Object.isSealed(obj));
  // Verify that the value can't be written
  obj1 = new Array(...obj);
  var length = obj.length;
  for (var i = 0; i < length-1; i++) {
    obj[i] = 'new';
    assertEquals(obj1[i], obj[i]);
  }

  // Verify that the length can't be written by builtins.
  assertTrue(Array.isArray(obj));
  assertThrows(function() { obj.pop(); }, TypeError);
  assertThrows(function() { obj.push(); }, TypeError);
  assertThrows(function() { obj.shift(); }, TypeError);
  assertThrows(function() { obj.unshift(); }, TypeError);
  assertThrows(function() { obj.copyWithin(0,0); }, TypeError);
  assertThrows(function() { obj.fill(0); }, TypeError);
  assertThrows(function() { obj.reverse(); }, TypeError);
  assertThrows(function() { obj.sort(); }, TypeError);
  assertThrows(function() { obj.splice(0); }, TypeError);
  assertThrows(function() { obj.splice(0, 0); }, TypeError);
  assertTrue(Object.isFrozen(obj));

  // Verify search, filter, iterator
  assertEquals(obj.lastIndexOf(1), 2);
  assertEquals(obj.indexOf(1.1), 5);
  assertEquals(obj.indexOf(undefined), -1);
  assertFalse(obj.includes(Symbol("test")));
  assertTrue(obj.includes(undefined));
  assertFalse(obj.includes(NaN));
  assertTrue(obj.includes());
  assertEquals(obj.find(x => x==2), undefined);
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
}

obj = [-1.1, 0, 1, , -1, 1.1];
assertTrue(%HasHoleyElements(obj));
Object.freeze(obj);
testHoleyDoubleFrozenArray1(obj);

// Verify change from sealed to frozen
obj = [-1.1, 0, 1, , -1, 1.1];
assertTrue(%HasHoleyElements(obj));
Object.seal(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testHoleyDoubleFrozenArray1(obj);

// Verify change from non-extensible to frozen
obj = [-1.1, 0, 1, , -1, 1.1];
assertTrue(%HasHoleyElements(obj));
Object.preventExtensions(obj);
Object.freeze(obj);
assertTrue(Object.isSealed(obj));
testHoleyDoubleFrozenArray1(obj);

// Verify flat, map, slice, flatMap, join, reduce, reduceRight for frozen packed array
function testHoleyDoubleFrozenArray2(arr) {
  assertTrue(Object.isFrozen(arr));
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
Object.preventExtensions(arr1);
Object.freeze(arr1);
testHoleyDoubleFrozenArray2(arr1);

// Verify change from sealed to frozen
var arr1 = [, 1.1, 1, 0];
assertTrue(%HasHoleyElements(arr1));
Object.seal(arr1);
Object.freeze(arr1);
testHoleyDoubleFrozenArray2(arr1);

// Verify change from non-extensible to frozen
var arr1 = [, 1.1, 1, 0];
assertTrue(%HasHoleyElements(arr1));
Object.preventExtensions(arr1);
Object.freeze(arr1);
testHoleyDoubleFrozenArray2(arr1);

// Test regression with Object.defineProperty
var obj = [1.1, , 0];
obj.propertyA = 42;
obj[0] = true;
Object.freeze(obj);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
  });
}, TypeError);
assertEquals(42, obj.propertyA);
assertThrows(function() {
  Object.defineProperty(obj, 'propertyA', {
    value: obj,
    writable: false,
  });
}, TypeError);
assertDoesNotThrow(function() {obj.propertyA = 2;});
assertEquals(obj.propertyA, 42);
assertThrows(function() {
  Object.defineProperty(obj, 'abc', {
    value: obj,
  });
}, TypeError);

// Regression test with simple holey array
var arr = [, 1.1];
Object.freeze(arr);
arr[1] = 'b';
assertEquals(arr[1], 1.1);
arr[0] = 1;
assertEquals(arr[0], undefined);

// Test regression Array.concat with double
var arr = [1.1, , 0];
Object.freeze(arr);
arr = arr.concat(0.5);
assertEquals(arr, [1.1, , 0, 0.5]);
Object.freeze(arr);
arr = arr.concat([1.5, 'c']);
assertEquals(arr, [1.1, ,0, 0.5, 1.5, 'c']);

// Regression test with change length
var arr = [1.1, ,0];
Object.freeze(arr);
assertEquals(arr.length, 3);
arr.length = 4;
assertEquals(arr.length, 3);
arr[3] = 'c';
assertEquals(arr[2], 0);
assertEquals(arr[3], undefined);
arr.length = 2;
assertEquals(arr.length, 3);

// Change length with holey entries at the end
var arr = [1.1, ,];
Object.freeze(arr);
assertEquals(arr.length, 2);
arr.length = 0;
assertEquals(arr.length, 2);
arr.length = 3;
assertEquals(arr.length, 2);
arr.length = 0;
assertEquals(arr.length, 2);

// Spread with array
var arr = [1.1, 0, -1];
Object.freeze(arr);
var arrSpread = [...arr];
assertEquals(arrSpread.length, arr.length);
assertEquals(arrSpread[0], 1.1);
assertEquals(arrSpread[1], 0);
assertEquals(arrSpread[2], -1);

// Spread with array-like
function returnArgs() {
  return Object.freeze(arguments);
}
var arrLike = returnArgs(1.1, 0, -1);
assertTrue(Object.isFrozen(arrLike));
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
Object.freeze(arr);
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
  var a = Object.freeze([0.1,1,2,3,4]);

  // Simple use.
  var result = [1.1,2,3,4,5];
  assertArrayEquals(result, a.map(function(n) { return Number(n) + 1; }));

  // Use specified object as this object when calling the function.
  var o = { delta: 42 }
  result = [42.1,43,44,45,46];
  assertArrayEquals(result, a.map(function(n) { return this.delta + Number(n); }, o));

  // Modify original array.
  b = Object.freeze([0.1,1,2,3,4]);
  result = [1.1,2,3,4,5];
  assertArrayEquals(result,
      b.map(function(n, index, array) {
        array[index] = Number(n) + 1; return Number(n) + 1;
      }));
  assertArrayEquals(b, a);

  // Only loop through initial part of array and elements are not
  // added.
  a = Object.freeze([0.1,1,2,3,4]);
  result = [1.1,2,3,4,5];
  assertArrayEquals(result,
      a.map(function(n, index, array) { assertThrows(() => { array.push(n) }); return Number(n) + 1; }));
  assertArrayEquals([0.1,1,2,3,4], a);

  // Respect holes.
  a = new Array(20);
  a[1] = 1.1;
  Object.freeze(a);
  a = Object.freeze(a).map(function(n) { return 2*Number(n); });

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
      Array.prototype.map.call(Object.freeze(a), function(n) {
        received.push(n);
        return n * 2;
      }));
  assertArrayEquals([1.1, 2], received);

  // Modify array prototype
  a = [1.1, , 2];
  received = [];
  assertThrows(() => {
    Array.prototype.map.call(Object.freeze(a), function(n) {
      a.__proto__ = null;
      received.push(n);
      return n * 2;
    });
  }, TypeError);
  assertArrayEquals([], received);

  // Create a new object in each function call when receiver is a
  // primitive value. See ECMA-262, Annex C.
  a = [];
  Object.freeze([1.1, 2]).map(function() { a.push(this) }, "");
  assertTrue(a[0] !== a[1]);

  // Do not create a new object otherwise.
  a = [];
  Object.freeze([1.1, 2]).map(function() { a.push(this) }, {});
  assertSame(a[0], a[1]);

  // In strict mode primitive values should not be coerced to an object.
  a = [];
  Object.freeze([1.1, 1.2]).map(function() { 'use strict'; a.push(this); }, "");
  assertEquals("", a[0]);
  assertEquals(a[0], a[1]);

})();
