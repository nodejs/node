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

// Tests the Object.seal and Object.isSealed methods - ES 15.2.3.9 and
// ES 15.2.3.12

// Flags: --allow-natives-syntax --noalways-opt

// Test that we throw an error if an object is not passed as argument.
var non_objects = new Array(undefined, null, 1, -1, 0, 42.43);
for (var key in non_objects) {
  var exception = false;
  try {
    Object.seal(non_objects[key]);
  } catch(e) {
    exception = true;
    assertTrue(/Object.seal called on non-object/.test(e));
  }
  assertTrue(exception);
}

for (var key in non_objects) {
  exception = false;
  try {
    Object.isSealed(non_objects[key]);
  } catch(e) {
    exception = true;
    assertTrue(/Object.isSealed called on non-object/.test(e));
  }
  assertTrue(exception);
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

push_call = function(a) { a.push(1000); return a; }
// Include a call site that doesn't have a custom built-in.
var shift_call = function(a) { a.shift(1000); return a; }
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
