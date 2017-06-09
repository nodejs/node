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
