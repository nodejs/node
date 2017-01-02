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

// Tests the object.defineProperty method - ES 15.2.3.6

// Flags: --allow-natives-syntax

// Check that an exception is thrown when null is passed as object.
var exception = false;
try {
  Object.defineProperty(null, null, null);
} catch (e) {
  exception = true;
  assertTrue(/called on non-object/.test(e));
}
assertTrue(exception);

// Check that an exception is thrown when undefined is passed as object.
exception = false;
try {
  Object.defineProperty(undefined, undefined, undefined);
} catch (e) {
  exception = true;
  assertTrue(/called on non-object/.test(e));
}
assertTrue(exception);

// Check that an exception is thrown when non-object is passed as object.
exception = false;
try {
  Object.defineProperty(0, "foo", undefined);
} catch (e) {
  exception = true;
  assertTrue(/called on non-object/.test(e));
}
assertTrue(exception);

// Object.
var obj1 = {};

// Values.
var val1 = 0;
var val2 = 0;
var val3 = 0;

function setter1() {val1++; }
function getter1() {return val1; }

function setter2() {val2++; }
function getter2() {return val2; }

function setter3() {val3++; }
function getter3() {return val3; }


// Descriptors.
var emptyDesc = {};

var accessorConfigurable = {
    set: setter1,
    get: getter1,
    configurable: true
};

var accessorNoConfigurable = {
    set: setter2,
    get: getter2,
    configurable: false
};

var accessorOnlySet = {
  set: setter3,
  configurable: true
};

var accessorOnlyGet = {
  get: getter3,
  configurable: true
};

var accessorDefault = {set: function(){} };

var dataConfigurable = { value: 1000, configurable: true };

var dataNoConfigurable = { value: 2000, configurable: false };

var dataWritable = { value: 3000, writable: true};


// Check that we can't add property with undefined attributes.
try {
  Object.defineProperty(obj1, "foo", undefined);
  assertTrue(false);
} catch (e) {
  assertTrue(/must be an object/.test(e));
}

// Make sure that we can add a property with an empty descriptor and
// that it has the default descriptor values.
Object.defineProperty(obj1, "foo", emptyDesc);

// foo should be undefined as it has no get, set or value
assertEquals(undefined, obj1.foo);

// We should, however, be able to retrieve the propertydescriptor which should
// have all default values (according to 8.6.1).
var desc = Object.getOwnPropertyDescriptor(obj1, "foo");
assertFalse(desc.configurable);
assertFalse(desc.enumerable);
assertFalse(desc.writable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);
assertEquals(desc.value, undefined);

// Make sure that getOwnPropertyDescriptor does not return a descriptor
// with default values if called with non existing property (otherwise
// the test above is invalid).
desc = Object.getOwnPropertyDescriptor(obj1, "bar");
assertEquals(desc, undefined);

// Make sure that foo can't be reset (as configurable is false).
try {
  Object.defineProperty(obj1, "foo", accessorConfigurable);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}


// Accessor properties

Object.defineProperty(obj1, "bar", accessorConfigurable);
desc = Object.getOwnPropertyDescriptor(obj1, "bar");
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorConfigurable.get);
assertEquals(desc.set, accessorConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj1.bar = 1);
assertEquals(1, val1);
assertEquals(1, obj1.bar = 1);
assertEquals(2, val1);
assertEquals(2, obj1.bar);

// Redefine bar with non configurable test
Object.defineProperty(obj1, "bar", accessorNoConfigurable);
desc = Object.getOwnPropertyDescriptor(obj1, "bar");
assertFalse(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorNoConfigurable.get);
assertEquals(desc.set, accessorNoConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj1.bar = 1);
assertEquals(2, val1);
assertEquals(1, val2);
assertEquals(1, obj1.bar = 1)
assertEquals(2, val1);
assertEquals(2, val2);
assertEquals(2, obj1.bar);

// Try to redefine bar again - should fail as configurable is false.
try {
  Object.defineProperty(obj1, "bar", accessorConfigurable);
  assertTrue(false);
} catch(e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Try to redefine bar again using the data descriptor - should fail.
try {
  Object.defineProperty(obj1, "bar", dataConfigurable);
  assertTrue(false);
} catch(e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Redefine using same descriptor - should succeed.
Object.defineProperty(obj1, "bar", accessorNoConfigurable);
desc = Object.getOwnPropertyDescriptor(obj1, "bar");
assertFalse(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorNoConfigurable.get);
assertEquals(desc.set, accessorNoConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj1.bar = 1);
assertEquals(2, val1);
assertEquals(3, val2);
assertEquals(1, obj1.bar = 1)
assertEquals(2, val1);
assertEquals(4, val2);
assertEquals(4, obj1.bar);

// Define an accessor that has only a setter.
Object.defineProperty(obj1, "setOnly", accessorOnlySet);
desc = Object.getOwnPropertyDescriptor(obj1, "setOnly");
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.set, accessorOnlySet.set);
assertEquals(desc.writable, undefined);
assertEquals(desc.value, undefined);
assertEquals(desc.get, undefined);
assertEquals(1, obj1.setOnly = 1);
assertEquals(1, val3);

// Add a getter - should not touch the setter.
Object.defineProperty(obj1, "setOnly", accessorOnlyGet);
desc = Object.getOwnPropertyDescriptor(obj1, "setOnly");
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.get, accessorOnlyGet.get);
assertEquals(desc.set, accessorOnlySet.set);
assertEquals(desc.writable, undefined);
assertEquals(desc.value, undefined);
assertEquals(1, obj1.setOnly = 1);
assertEquals(2, val3);

// The above should also work if redefining just a getter or setter on
// an existing property with both a getter and a setter.
Object.defineProperty(obj1, "both", accessorConfigurable);

Object.defineProperty(obj1, "both", accessorOnlySet);
desc = Object.getOwnPropertyDescriptor(obj1, "both");
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.set, accessorOnlySet.set);
assertEquals(desc.get, accessorConfigurable.get);
assertEquals(desc.writable, undefined);
assertEquals(desc.value, undefined);
assertEquals(1, obj1.both = 1);
assertEquals(3, val3);


// Data properties

Object.defineProperty(obj1, "foobar", dataConfigurable);
desc = Object.getOwnPropertyDescriptor(obj1, "foobar");
assertEquals(obj1.foobar, 1000);
assertEquals(desc.value, 1000);
assertTrue(desc.configurable);
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);
//Try writing to non writable attribute - should remain 1000
obj1.foobar = 1001;
assertEquals(obj1.foobar, 1000);


// Redefine to writable descriptor - now writing to foobar should be allowed.
Object.defineProperty(obj1, "foobar", dataWritable);
desc = Object.getOwnPropertyDescriptor(obj1, "foobar");
assertEquals(obj1.foobar, 3000);
assertEquals(desc.value, 3000);
// Note that since dataWritable does not define configurable the configurable
// setting from the redefined property (in this case true) is used.
assertTrue(desc.configurable);
assertTrue(desc.writable);
assertFalse(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);
// Writing to the property should now be allowed
obj1.foobar = 1001;
assertEquals(obj1.foobar, 1001);


// Redefine with non configurable data property.
Object.defineProperty(obj1, "foobar", dataNoConfigurable);
desc = Object.getOwnPropertyDescriptor(obj1, "foobar");
assertEquals(obj1.foobar, 2000);
assertEquals(desc.value, 2000);
assertFalse(desc.configurable);
assertTrue(desc.writable);
assertFalse(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);

// Try redefine again - shold fail because configurable is now false.
try {
  Object.defineProperty(obj1, "foobar", dataConfigurable);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Try redefine again with accessor property - shold also fail.
try {
  Object.defineProperty(obj1, "foobar", dataConfigurable);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}


// Redifine with the same descriptor - should succeed (step 6).
Object.defineProperty(obj1, "foobar", dataNoConfigurable);
desc = Object.getOwnPropertyDescriptor(obj1, "foobar");
assertEquals(obj1.foobar, 2000);
assertEquals(desc.value, 2000);
assertFalse(desc.configurable);
assertTrue(desc.writable);
assertFalse(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);


// New object
var obj2 = {};

// Make accessor - redefine to data
Object.defineProperty(obj2, "foo", accessorConfigurable);

// Redefine to data property
Object.defineProperty(obj2, "foo", dataConfigurable);
desc = Object.getOwnPropertyDescriptor(obj2, "foo");
assertEquals(obj2.foo, 1000);
assertEquals(desc.value, 1000);
assertTrue(desc.configurable);
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);


// Redefine back to accessor
Object.defineProperty(obj2, "foo", accessorConfigurable);
desc = Object.getOwnPropertyDescriptor(obj2, "foo");
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorConfigurable.get);
assertEquals(desc.set, accessorConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj2.foo = 1);
assertEquals(3, val1);
assertEquals(4, val2);
assertEquals(3, obj2.foo);

// Make data - redefine to accessor
Object.defineProperty(obj2, "bar", dataConfigurable)

// Redefine to accessor property
Object.defineProperty(obj2, "bar", accessorConfigurable);
desc = Object.getOwnPropertyDescriptor(obj2, "bar");
assertTrue(desc.configurable);
assertFalse(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorConfigurable.get);
assertEquals(desc.set, accessorConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj2.bar = 1);
assertEquals(4, val1);
assertEquals(4, val2);
assertEquals(4, obj2.foo);

// Redefine back to data property
Object.defineProperty(obj2, "bar", dataConfigurable);
desc = Object.getOwnPropertyDescriptor(obj2, "bar");
assertEquals(obj2.bar, 1000);
assertEquals(desc.value, 1000);
assertTrue(desc.configurable);
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);


// Redefinition of an accessor defined using __defineGetter__ and
// __defineSetter__.
function get(){return this.x}
function set(x){this.x=x};

var obj3 = {x:1000};
obj3.__defineGetter__("foo", get);
obj3.__defineSetter__("foo", set);

desc = Object.getOwnPropertyDescriptor(obj3, "foo");
assertTrue(desc.configurable);
assertTrue(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, get);
assertEquals(desc.set, set);
assertEquals(desc.value, undefined);
assertEquals(1, obj3.foo = 1);
assertEquals(1, obj3.x);
assertEquals(1, obj3.foo);

// Redefine to accessor property (non configurable) - note that enumerable
// which we do not redefine should remain the same (true).
Object.defineProperty(obj3, "foo", accessorNoConfigurable);
desc = Object.getOwnPropertyDescriptor(obj3, "foo");
assertFalse(desc.configurable);
assertTrue(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorNoConfigurable.get);
assertEquals(desc.set, accessorNoConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj3.foo = 1);
assertEquals(5, val2);
assertEquals(5, obj3.foo);


obj3.__defineGetter__("bar", get);
obj3.__defineSetter__("bar", set);


// Redefine back to data property
Object.defineProperty(obj3, "bar", dataConfigurable);
desc = Object.getOwnPropertyDescriptor(obj3, "bar");
assertEquals(obj3.bar, 1000);
assertEquals(desc.value, 1000);
assertTrue(desc.configurable);
assertFalse(desc.writable);
assertTrue(desc.enumerable);
assertEquals(desc.get, undefined);
assertEquals(desc.set, undefined);


var obj4 = {};
var func = function (){return 42;};
obj4.bar = func;
assertEquals(42, obj4.bar());

Object.defineProperty(obj4, "bar", accessorConfigurable);
desc = Object.getOwnPropertyDescriptor(obj4, "bar");
assertTrue(desc.configurable);
assertTrue(desc.enumerable);
assertEquals(desc.writable, undefined);
assertEquals(desc.get, accessorConfigurable.get);
assertEquals(desc.set, accessorConfigurable.set);
assertEquals(desc.value, undefined);
assertEquals(1, obj4.bar = 1);
assertEquals(5, val1);
assertEquals(5, obj4.bar);

// Make sure an error is thrown when trying to access to redefined function.
try {
  obj4.bar();
  assertTrue(false);
} catch (e) {
  assertTrue(/is not a function/.test(e));
}


// Test that all possible differences in step 6 in DefineOwnProperty are
// exercised, i.e., any difference in the given property descriptor and the
// existing properties should not return true, but throw an error if the
// existing configurable property is false.

var obj5 = {};
// Enumerable will default to false.
Object.defineProperty(obj5, 'foo', accessorNoConfigurable);
desc = Object.getOwnPropertyDescriptor(obj5, 'foo');
// First, test that we are actually allowed to set the accessor if all
// values are of the descriptor are the same as the existing one.
Object.defineProperty(obj5, 'foo', accessorNoConfigurable);

// Different setter.
var descDifferent = {
  configurable:false,
  enumerable:false,
  set: setter1,
  get: getter2
};

try {
  Object.defineProperty(obj5, 'foo', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Different getter.
descDifferent = {
  configurable:false,
  enumerable:false,
  set: setter2,
  get: getter1
};

try {
  Object.defineProperty(obj5, 'foo', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Different enumerable.
descDifferent = {
  configurable:false,
  enumerable:true,
  set: setter2,
  get: getter2
};

try {
  Object.defineProperty(obj5, 'foo', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Different configurable.
descDifferent = {
  configurable:false,
  enumerable:true,
  set: setter2,
  get: getter2
};

try {
  Object.defineProperty(obj5, 'foo', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// No difference.
descDifferent = {
  configurable:false,
  enumerable:false,
  set: setter2,
  get: getter2
};
// Make sure we can still redefine if all properties are the same.
Object.defineProperty(obj5, 'foo', descDifferent);

// Make sure that obj5 still holds the original values.
desc = Object.getOwnPropertyDescriptor(obj5, 'foo');
assertEquals(desc.get, getter2);
assertEquals(desc.set, setter2);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);


// Also exercise step 6 on data property, writable and enumerable
// defaults to false.
Object.defineProperty(obj5, 'bar', dataNoConfigurable);

// Test that redefinition with the same property descriptor is possible
Object.defineProperty(obj5, 'bar', dataNoConfigurable);

// Different value.
descDifferent = {
  configurable:false,
  enumerable:false,
  writable: false,
  value: 1999
};

try {
  Object.defineProperty(obj5, 'bar', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// Different writable.
descDifferent = {
  configurable:false,
  enumerable:false,
  writable: true,
  value: 2000
};

try {
  Object.defineProperty(obj5, 'bar', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}


// Different enumerable.
descDifferent = {
  configurable:false,
  enumerable:true ,
  writable:false,
  value: 2000
};

try {
  Object.defineProperty(obj5, 'bar', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}


// Different configurable.
descDifferent = {
  configurable:true,
  enumerable:false,
  writable:false,
  value: 2000
};

try {
  Object.defineProperty(obj5, 'bar', descDifferent);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot redefine property/.test(e));
}

// No difference.
descDifferent = {
  configurable:false,
  enumerable:false,
  writable:false,
  value:2000
};
// Make sure we can still redefine if all properties are the same.
Object.defineProperty(obj5, 'bar', descDifferent);

// Make sure that obj5 still holds the original values.
desc = Object.getOwnPropertyDescriptor(obj5, 'bar');
assertEquals(desc.value, 2000);
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);


// Make sure that we can't overwrite +0 with -0 and vice versa.
var descMinusZero = {value: -0, configurable: false};
var descPlusZero = {value: +0, configurable: false};

Object.defineProperty(obj5, 'minuszero', descMinusZero);

// Make sure we can redefine with -0.
Object.defineProperty(obj5, 'minuszero', descMinusZero);

exception = false;
try {
  Object.defineProperty(obj5, 'minuszero', descPlusZero);
} catch (e) {
  exception = true;
  assertTrue(/Cannot redefine property/.test(e));
}
assertTrue(exception);


Object.defineProperty(obj5, 'pluszero', descPlusZero);

// Make sure we can redefine with +0.
Object.defineProperty(obj5, 'pluszero', descPlusZero);

exception = false;
try {
  Object.defineProperty(obj5, 'pluszero', descMinusZero);
} catch (e) {
  exception = true;
  assertTrue(/Cannot redefine property/.test(e));
}
assertTrue(exception);


var obj6 = {};
obj6[1] = 'foo';
obj6[2] = 'bar';
obj6[3] = '42';
obj6[4] = '43';
obj6[5] = '44';

var descElement = { value: 'foobar' };
var descElementNonConfigurable = { value: 'barfoo', configurable: false };
var descElementNonWritable = { value: 'foofoo', writable: false };
var descElementNonEnumerable = { value: 'barbar', enumerable: false };
var descElementAllFalse = { value: 'foofalse',
                            configurable: false,
                            writable: false,
                            enumerable: false };


// Redefine existing property.
Object.defineProperty(obj6, '1', descElement);
desc = Object.getOwnPropertyDescriptor(obj6, '1');
assertEquals(desc.value, 'foobar');
assertTrue(desc.writable);
assertTrue(desc.enumerable);
assertTrue(desc.configurable);

// Redefine existing property with configurable: false.
Object.defineProperty(obj6, '2', descElementNonConfigurable);
desc = Object.getOwnPropertyDescriptor(obj6, '2');
assertEquals(desc.value, 'barfoo');
assertTrue(desc.writable);
assertTrue(desc.enumerable);
assertFalse(desc.configurable);

// Can use defineProperty to change the value of a non
// configurable property.
try {
  Object.defineProperty(obj6, '2', descElement);
  desc = Object.getOwnPropertyDescriptor(obj6, '2');
  assertEquals(desc.value, 'foobar');
} catch (e) {
  assertUnreachable();
}

// Ensure that we can't change the descriptor of a
// non configurable property.
exception = false;
try {
  var descAccessor = { get: function() { return 0; } };
  Object.defineProperty(obj6, '2', descAccessor);
} catch (e) {
  exception = true;
  assertTrue(/Cannot redefine property/.test(e));
}
assertTrue(exception);

Object.defineProperty(obj6, '2', descElementNonWritable);
desc = Object.getOwnPropertyDescriptor(obj6, '2');
assertEquals(desc.value, 'foofoo');
assertFalse(desc.writable);
assertTrue(desc.enumerable);
assertFalse(desc.configurable);

Object.defineProperty(obj6, '3', descElementNonWritable);
desc = Object.getOwnPropertyDescriptor(obj6, '3');
assertEquals(desc.value, 'foofoo');
assertFalse(desc.writable);
assertTrue(desc.enumerable);
assertTrue(desc.configurable);

// Redefine existing property with configurable: false.
Object.defineProperty(obj6, '4', descElementNonEnumerable);
desc = Object.getOwnPropertyDescriptor(obj6, '4');
assertEquals(desc.value, 'barbar');
assertTrue(desc.writable);
assertFalse(desc.enumerable);
assertTrue(desc.configurable);

// Redefine existing property with configurable: false.
Object.defineProperty(obj6, '5', descElementAllFalse);
desc = Object.getOwnPropertyDescriptor(obj6, '5');
assertEquals(desc.value, 'foofalse');
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);

// Define non existing property - all attributes should default to false.
Object.defineProperty(obj6, '15', descElement);
desc = Object.getOwnPropertyDescriptor(obj6, '15');
assertEquals(desc.value, 'foobar');
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);

// Make sure that we can't redefine using direct access.
obj6[15] ='overwrite';
assertEquals(obj6[15],'foobar');


// Repeat the above tests on an array.
var arr = new Array();
arr[1] = 'foo';
arr[2] = 'bar';
arr[3] = '42';
arr[4] = '43';
arr[5] = '44';

var descElement = { value: 'foobar' };
var descElementNonConfigurable = { value: 'barfoo', configurable: false };
var descElementNonWritable = { value: 'foofoo', writable: false };
var descElementNonEnumerable = { value: 'barbar', enumerable: false };
var descElementAllFalse = { value: 'foofalse',
                            configurable: false,
                            writable: false,
                            enumerable: false };


// Redefine existing property.
Object.defineProperty(arr, '1', descElement);
desc = Object.getOwnPropertyDescriptor(arr, '1');
assertEquals(desc.value, 'foobar');
assertTrue(desc.writable);
assertTrue(desc.enumerable);
assertTrue(desc.configurable);

// Redefine existing property with configurable: false.
Object.defineProperty(arr, '2', descElementNonConfigurable);
desc = Object.getOwnPropertyDescriptor(arr, '2');
assertEquals(desc.value, 'barfoo');
assertTrue(desc.writable);
assertTrue(desc.enumerable);
assertFalse(desc.configurable);

// Can use defineProperty to change the value of a non
// configurable property of an array.
try {
  Object.defineProperty(arr, '2', descElement);
  desc = Object.getOwnPropertyDescriptor(arr, '2');
  assertEquals(desc.value, 'foobar');
} catch (e) {
  assertUnreachable();
}

// Ensure that we can't change the descriptor of a
// non configurable property.
exception = false;
try {
  var descAccessor = { get: function() { return 0; } };
  Object.defineProperty(arr, '2', descAccessor);
} catch (e) {
  exception = true;
  assertTrue(/Cannot redefine property/.test(e));
}
assertTrue(exception);

Object.defineProperty(arr, '2', descElementNonWritable);
desc = Object.getOwnPropertyDescriptor(arr, '2');
assertEquals(desc.value, 'foofoo');
assertFalse(desc.writable);
assertTrue(desc.enumerable);
assertFalse(desc.configurable);

Object.defineProperty(arr, '3', descElementNonWritable);
desc = Object.getOwnPropertyDescriptor(arr, '3');
assertEquals(desc.value, 'foofoo');
assertFalse(desc.writable);
assertTrue(desc.enumerable);
assertTrue(desc.configurable);

// Redefine existing property with configurable: false.
Object.defineProperty(arr, '4', descElementNonEnumerable);
desc = Object.getOwnPropertyDescriptor(arr, '4');
assertEquals(desc.value, 'barbar');
assertTrue(desc.writable);
assertFalse(desc.enumerable);
assertTrue(desc.configurable);

// Redefine existing property with configurable: false.
Object.defineProperty(arr, '5', descElementAllFalse);
desc = Object.getOwnPropertyDescriptor(arr, '5');
assertEquals(desc.value, 'foofalse');
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);

// Define non existing property - all attributes should default to false.
Object.defineProperty(arr, '15', descElement);
desc = Object.getOwnPropertyDescriptor(arr, '15');
assertEquals(desc.value, 'foobar');
assertFalse(desc.writable);
assertFalse(desc.enumerable);
assertFalse(desc.configurable);

// Define non-array property, check that .length is unaffected.
assertEquals(16, arr.length);
Object.defineProperty(arr, '0x20', descElement);
assertEquals(16, arr.length);

// See issue 968: http://code.google.com/p/v8/issues/detail?id=968
var o = { x : 42 };
Object.defineProperty(o, "x", { writable: false });
assertEquals(42, o.x);
o.x = 37;
assertEquals(42, o.x);

o = { x : 42 };
Object.defineProperty(o, "x", {});
assertEquals(42, o.x);
o.x = 37;
// Writability is preserved.
assertEquals(37, o.x);

var o = { };
Object.defineProperty(o, "x", { writable: false });
assertEquals(undefined, o.x);
o.x = 37;
assertEquals(undefined, o.x);

o = { get x() { return 87; } };
Object.defineProperty(o, "x", { writable: false });
assertEquals(undefined, o.x);
o.x = 37;
assertEquals(undefined, o.x);

// Ignore inherited properties.
o = { __proto__ : { x : 87 } };
Object.defineProperty(o, "x", { writable: false });
assertEquals(undefined, o.x);
o.x = 37;
assertEquals(undefined, o.x);

function testDefineProperty(obj, propertyName, desc, resultDesc) {
  Object.defineProperty(obj, propertyName, desc);
  var actualDesc = Object.getOwnPropertyDescriptor(obj, propertyName);
  assertEquals(resultDesc.enumerable, actualDesc.enumerable);
  assertEquals(resultDesc.configurable, actualDesc.configurable);
  if (resultDesc.hasOwnProperty('value')) {
    assertEquals(resultDesc.value, actualDesc.value);
    assertEquals(resultDesc.writable, actualDesc.writable);
    assertFalse(resultDesc.hasOwnProperty('get'));
    assertFalse(resultDesc.hasOwnProperty('set'));
  } else {
    assertEquals(resultDesc.get, actualDesc.get);
    assertEquals(resultDesc.set, actualDesc.set);
    assertFalse(resultDesc.hasOwnProperty('value'));
    assertFalse(resultDesc.hasOwnProperty('writable'));
  }
}

// tests redefining existing property with a generic descriptor
o = { p : 42 };
testDefineProperty(o, 'p',
  { },
  { value : 42, writable : true, enumerable : true, configurable : true });

o = { p : 42 };
testDefineProperty(o, 'p',
  { enumerable : true },
  { value : 42, writable : true, enumerable : true, configurable : true });

o = { p : 42 };
testDefineProperty(o, 'p',
  { configurable : true },
  { value : 42, writable : true, enumerable : true, configurable : true });

o = { p : 42 };
testDefineProperty(o, 'p',
  { enumerable : false },
  { value : 42, writable : true, enumerable : false, configurable : true });

o = { p : 42 };
testDefineProperty(o, 'p',
  { configurable : false },
  { value : 42, writable : true, enumerable : true, configurable : false });

o = { p : 42 };
testDefineProperty(o, 'p',
  { enumerable : true, configurable : true },
  { value : 42, writable : true, enumerable : true, configurable : true });

o = { p : 42 };
testDefineProperty(o, 'p',
  { enumerable : false, configurable : true },
  { value : 42, writable : true, enumerable : false, configurable : true });

o = { p : 42 };
testDefineProperty(o, 'p',
  { enumerable : true, configurable : false },
  { value : 42, writable : true, enumerable : true, configurable : false });

o = { p : 42 };
testDefineProperty(o, 'p',
  { enumerable : false, configurable : false },
  { value : 42, writable : true, enumerable : false, configurable : false });

// can make a writable, non-configurable field non-writable
o = { p : 42 };
Object.defineProperty(o, 'p', { configurable: false });
testDefineProperty(o, 'p',
  { writable: false },
  { value : 42, writable : false, enumerable : true, configurable : false });

// redefine of get only property with generic descriptor
o = {};
Object.defineProperty(o, 'p',
  { get : getter1, enumerable: true, configurable: true });
testDefineProperty(o, 'p',
  { enumerable : false, configurable : false },
  { get: getter1, set: undefined, enumerable : false, configurable : false });

// redefine of get/set only property with generic descriptor
o = {};
Object.defineProperty(o, 'p',
  { get: getter1, set: setter1, enumerable: true, configurable: true });
testDefineProperty(o, 'p',
  { enumerable : false, configurable : false },
  { get: getter1, set: setter1, enumerable : false, configurable : false });

// redefine of set only property with generic descriptor
o = {};
Object.defineProperty(o, 'p',
  { set : setter1, enumerable: true, configurable: true });
testDefineProperty(o, 'p',
  { enumerable : false, configurable : false },
  { get: undefined, set: setter1, enumerable : false, configurable : false });


// Regression test: Ensure that growing dictionaries are not ignored.
o = {};
for (var i = 0; i < 1000; i++) {
  // Non-enumerable property forces dictionary mode.
  Object.defineProperty(o, i, {value: i, enumerable: false});
}
assertEquals(999, o[999]);


// Regression test: Bizzare behavior on non-strict arguments object.
// TODO(yangguo): Tests disabled, needs investigation!
/*
(function test(arg0) {
  // Here arguments[0] is a fast alias on arg0.
  Object.defineProperty(arguments, "0", {
    value:1,
    enumerable:false
  });
  // Here arguments[0] is a slow alias on arg0.
  Object.defineProperty(arguments, "0", {
    value:2,
    writable:false
  });
  // Here arguments[0] is no alias at all.
  Object.defineProperty(arguments, "0", {
    value:3
  });
  assertEquals(2, arg0);
  assertEquals(3, arguments[0]);
})(0);
*/

// Regression test: We should never observe the hole value.
var objectWithGetter = {};
objectWithGetter.__defineGetter__('foo', function() {});
assertEquals(undefined, objectWithGetter.__lookupSetter__('foo'));

var objectWithSetter = {};
objectWithSetter.__defineSetter__('foo', function(x) {});
assertEquals(undefined, objectWithSetter.__lookupGetter__('foo'));

// An object with a getter on the prototype chain.
function getter() { return 111; }
function anotherGetter() { return 222; }

function testGetterOnProto(expected, o) {
  assertEquals(expected, o.quebec);
}

obj1 = {};
Object.defineProperty(obj1, "quebec", { get: getter, configurable: true });
obj2 = Object.create(obj1);
obj3 = Object.create(obj2);

testGetterOnProto(111, obj3);
testGetterOnProto(111, obj3);
%OptimizeFunctionOnNextCall(testGetterOnProto);
testGetterOnProto(111, obj3);
testGetterOnProto(111, obj3);

Object.defineProperty(obj1, "quebec", { get: anotherGetter });

testGetterOnProto(222, obj3);
testGetterOnProto(222, obj3);
%OptimizeFunctionOnNextCall(testGetterOnProto);
testGetterOnProto(222, obj3);
testGetterOnProto(222, obj3);

// An object with a setter on the prototype chain.
var modifyMe;
function setter(x) { modifyMe = x+1; }
function anotherSetter(x) { modifyMe = x+2; }

function testSetterOnProto(expected, o) {
  modifyMe = 333;
  o.romeo = 444;
  assertEquals(expected, modifyMe);
}

obj1 = {};
Object.defineProperty(obj1, "romeo", { set: setter, configurable: true });
obj2 = Object.create(obj1);
obj3 = Object.create(obj2);

testSetterOnProto(445, obj3);
testSetterOnProto(445, obj3);
%OptimizeFunctionOnNextCall(testSetterOnProto);
testSetterOnProto(445, obj3);
testSetterOnProto(445, obj3);

Object.defineProperty(obj1, "romeo", { set: anotherSetter });

testSetterOnProto(446, obj3);
testSetterOnProto(446, obj3);
%OptimizeFunctionOnNextCall(testSetterOnProto);
testSetterOnProto(446, obj3);
testSetterOnProto(446, obj3);

// Removing a setter on the prototype chain.
function testSetterOnProtoStrict(o) {
  "use strict";
  o.sierra = 12345;
}

obj1 = {};
Object.defineProperty(obj1, "sierra",
                      { get: getter, set: setter, configurable: true });
obj2 = Object.create(obj1);
obj3 = Object.create(obj2);

testSetterOnProtoStrict(obj3);
testSetterOnProtoStrict(obj3);
%OptimizeFunctionOnNextCall(testSetterOnProtoStrict);
testSetterOnProtoStrict(obj3);
testSetterOnProtoStrict(obj3);

Object.defineProperty(obj1, "sierra",
                      { get: getter, set: undefined, configurable: true });

exception = false;
try {
  testSetterOnProtoStrict(obj3);
} catch (e) {
  exception = true;
  assertTrue(/which has only a getter/.test(e));
}
assertTrue(exception);

// Test assignment to a getter-only property on the prototype chain. This makes
// sure that crankshaft re-checks its assumptions and doesn't rely only on type
// feedback (which would be monomorphic here).

function Assign(o) {
  o.blubb = 123;
}

function C() {}

Assign(new C);
Assign(new C);
%OptimizeFunctionOnNextCall(Assign);
Object.defineProperty(C.prototype, "blubb", {get: function() { return -42; }});
Assign(new C);

// Test that changes to the prototype of a simple constructor are not ignored,
// even after creating initial instances.
function C() {
  this.x = 23;
}
assertEquals(23, new C().x);
C.prototype.__defineSetter__('x', function(value) { this.y = 23; });
assertEquals(void 0, new C().x);
