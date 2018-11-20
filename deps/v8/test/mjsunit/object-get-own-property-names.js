// Copyright 2009 the V8 project authors. All rights reserved.
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

// Test ES5 section 15.2.3.4 Object.getOwnPropertyNames.

// Check simple cases.
var obj = { a: 1, b: 2};
var propertyNames = Object.getOwnPropertyNames(obj);
propertyNames.sort();
assertEquals(2, propertyNames.length);
assertEquals("a", propertyNames[0]);
assertEquals("b", propertyNames[1]);

var obj = { a: function(){}, b: function(){} };
var propertyNames = Object.getOwnPropertyNames(obj);
propertyNames.sort();
assertEquals(2, propertyNames.length);
assertEquals("a", propertyNames[0]);
assertEquals("b", propertyNames[1]);

// Check slow case
var obj = { a: 1, b: 2, c: 3 };
delete obj.b;
var propertyNames = Object.getOwnPropertyNames(obj)
propertyNames.sort();
assertEquals(2, propertyNames.length);
assertEquals("a", propertyNames[0]);
assertEquals("c", propertyNames[1]);

// Check that non-enumerable properties are being returned.
var propertyNames = Object.getOwnPropertyNames([1, 2]);
propertyNames.sort();
assertEquals(3, propertyNames.length);
assertEquals("0", propertyNames[0]);
assertEquals("1", propertyNames[1]);
assertEquals("string", typeof propertyNames[0]);
assertEquals("string", typeof propertyNames[1]);
assertEquals("length", propertyNames[2]);

// Check that no proto properties are returned.
var obj = { foo: "foo" };
obj.__proto__ = { bar: "bar" };
propertyNames = Object.getOwnPropertyNames(obj);
propertyNames.sort();
assertEquals(1, propertyNames.length);
assertEquals("foo", propertyNames[0]);

// Check that getter properties are returned.
var obj = {};
obj.__defineGetter__("getter", function() {});
propertyNames = Object.getOwnPropertyNames(obj);
propertyNames.sort();
assertEquals(1, propertyNames.length);
assertEquals("getter", propertyNames[0]);

// Check that implementation does not access Array.prototype.
var savedConcat = Array.prototype.concat;
Array.prototype.concat = function() { return []; }
propertyNames = Object.getOwnPropertyNames({0: 'foo', bar: 'baz'});
assertEquals(2, propertyNames.length);
assertEquals('0', propertyNames[0]);
assertEquals('bar', propertyNames[1]);
assertSame(Array.prototype, propertyNames.__proto__);
Array.prototype.concat = savedConcat;

assertEquals(Object.getOwnPropertyNames(4), []);
assertEquals(Object.getOwnPropertyNames("foo"), ["0", "1", "2", "length"]);
assertEquals(Object.getOwnPropertyNames(true), []);

try {
  Object.getOwnPropertyNames(undefined);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot convert undefined or null to object/.test(e));
}

try {
  Object.getOwnPropertyNames(null);
  assertTrue(false);
} catch (e) {
  assertTrue(/Cannot convert undefined or null to object/.test(e));
}
