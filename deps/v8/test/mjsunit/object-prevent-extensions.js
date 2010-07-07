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


var obj1 = {};
// Extensible defaults to true.
assertTrue(Object.isExtensible(obj1));
Object.preventExtensions(obj1);

// Make sure the is_extensible flag is set. 
assertFalse(Object.isExtensible(obj1));
// Try adding a new property.
try {
  obj1.x = 42;
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}
assertEquals(undefined, obj1.x);

// Try adding a new element.
try {
  obj1[1] = 42;
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}
assertEquals(undefined, obj1[1]);


// Try when the object has an existing property.
var obj2 = {};
assertTrue(Object.isExtensible(obj2));
obj2.x = 42;
assertEquals(42, obj2.x);
assertTrue(Object.isExtensible(obj2));

Object.preventExtensions(obj2);
assertEquals(42, obj2.x);

try {
  obj2.y = 42;
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}

// obj2.y should still be undefined.
assertEquals(undefined, obj2.y);
// Make sure we can still write values to obj.x.
obj2.x = 43;
assertEquals(43, obj2.x)

try {
  obj2.y = new function() { return 42; };
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}
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

try {
  obj2[1] = 42;
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}

assertEquals(undefined, obj2[1]);

var arr = new Array();
arr[1] = 10;

Object.preventExtensions(arr);

try {
  arr[2] = 42;
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}
assertEquals(10, arr[1]);

// We should still be able to change exiting elements.
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
try {
  parent.y = 29;
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}


// Test that attributes on functions are also handled correctly.
function foo() {
  return 42;
}

Object.preventExtensions(foo);

try {
  foo.x = 29;
  assertUnreachable();
} catch (e) {
  assertTrue(/object is not extensible/.test(e));
}
