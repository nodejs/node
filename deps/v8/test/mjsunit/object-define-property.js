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

// Tests the object.defineProperty method - ES 15.2.3.6

// Flags: --allow-natives-syntax

// Check that an exception is thrown when null is passed as object.
try {
  Object.defineProperty(null, null, null);
  assertTrue(false);
} catch (e) {
  assertTrue(/called on non-object/.test(e));
}

// Check that an exception is thrown when undefined is passed as object.
try {
  Object.defineProperty(undefined, undefined, undefined);
  assertTrue(false);
} catch (e) {
  assertTrue(/called on non-object/.test(e));
}

// Check that an exception is thrown when non-object is passed as object.
try {
  Object.defineProperty(0, "foo", undefined);
  assertTrue(false);
} catch (e) {
  assertTrue(/called on non-object/.test(e));
}

// Object
var obj1 = {};

// Values
var val1 = 0;
var val2 = 0;
var val3 = 0;

// Descriptors
var emptyDesc = {};

var accessorConfigurable = { 
    set: function() { val1++; },
    get: function() { return val1; },
    configurable: true
};

var accessorNoConfigurable = {
    set: function() { val2++; },
    get: function() { return val2; },
    configurable: false 
};

var accessorOnlySet = {
  set: function() { val3++; },
  configurable: true
};

var accessorOnlyGet = {
  get: function() { return val3; },
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

// Define an accessor that has only a setter
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

// Add a getter - should not touch the setter
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


// Redefine to writable descriptor - now writing to foobar should be allowed
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
assertFalse(desc.writable);
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
assertFalse(desc.writable);
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
// __defineSetter__
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

// Make sure an error is thrown when trying to access to redefined function
try {
  obj4.bar();
  assertTrue(false);
} catch (e) {
  assertTrue(/is not a function/.test(e));
}


// Test runtime calls to DefineOrRedefineDataProperty and
// DefineOrRedefineAccessorProperty - make sure we don't 
// crash
try {
  %DefineOrRedefineAccessorProperty(0, 0, 0, 0, 0);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}

try {
  %DefineOrRedefineDataProperty(0, 0, 0, 0);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}

try {
  %DefineOrRedefineDataProperty(null, null, null, null);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}

try {
  %DefineOrRedefineAccessorProperty(null, null, null, null, null);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}

try {
  %DefineOrRedefineDataProperty({}, null, null, null);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}

// Defining properties null should fail even when we have
// other allowed values
try {
  %DefineOrRedefineAccessorProperty(null, 'foo', 0, func, 0);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}

try {
  %DefineOrRedefineDataProperty(null, 'foo', 0, 0);
} catch (e) {
  assertTrue(/illegal access/.test(e));
}
