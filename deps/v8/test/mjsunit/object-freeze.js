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

// Tests the Object.freeze and Object.isFrozen methods - ES 15.2.3.9 and
// ES 15.2.3.12


// Test that we throw an error if an object is not passed as argument.
var non_objects = new Array(undefined, null, 1, -1, 0, 42.43);
for (var key in non_objects) {
  try {
    Object.freeze(non_objects[key]);
    assertUnreachable();
  } catch(e) {
    assertTrue(/Object.freeze called on non-object/.test(e));
  }
}

for (var key in non_objects) {
  try {
    Object.isFrozen(non_objects[key]);
    assertUnreachable();
  } catch(e) {
    assertTrue(/Object.isFrozen called on non-object/.test(e));
  }
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

try {
   obj.foo = 42;
   assertUnreachable();
} catch(e) {
  assertTrue(/object is not extensible/.test(e));
}

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

try {
  obj2.foo = 42;
  assertUnreachable();
} catch(e) {
  assertTrue(/object is not extensible/.test(e));
}


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
