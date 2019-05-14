// Copyright 2015 the V8 project authors. All rights reserved.
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

// This is adapted from mjsunit/object-get-own-property-names.js.


// Check simple cases.
var obj = { a: 1, b: 2};
var keys = Reflect.ownKeys(obj);
assertEquals(2, keys.length);
assertEquals("a", keys[0]);
assertEquals("b", keys[1]);

var obj = { a: function(){}, b: function(){} };
var keys = Reflect.ownKeys(obj);
assertEquals(2, keys.length);
assertEquals("a", keys[0]);
assertEquals("b", keys[1]);

// Check slow case
var obj = { a: 1, b: 2, c: 3 };
delete obj.b;
var keys = Reflect.ownKeys(obj)
assertEquals(2, keys.length);
assertEquals("a", keys[0]);
assertEquals("c", keys[1]);

// Check that non-enumerable properties are being returned.
var keys = Reflect.ownKeys([1, 2]);
assertEquals(3, keys.length);
assertEquals("0", keys[0]);
assertEquals("1", keys[1]);
assertEquals("string", typeof keys[0]);
assertEquals("string", typeof keys[1]);
assertEquals("length", keys[2]);

// Check that no proto properties are returned.
var obj = { foo: "foo" };
obj.__proto__ = { bar: "bar" };
keys = Reflect.ownKeys(obj);
assertEquals(1, keys.length);
assertEquals("foo", keys[0]);

// Check that getter properties are returned.
var obj = {};
obj.__defineGetter__("getter", function() {});
keys = Reflect.ownKeys(obj);
assertEquals(1, keys.length);
assertEquals("getter", keys[0]);

// Check that implementation does not access Array.prototype.
var savedConcat = Array.prototype.concat;
Array.prototype.concat = function() { return []; }
keys = Reflect.ownKeys({0: 'foo', bar: 'baz'});
assertEquals(2, keys.length);
assertEquals('0', keys[0]);
assertEquals('bar', keys[1]);
assertSame(Array.prototype, keys.__proto__);
Array.prototype.concat = savedConcat;

assertThrows(function() { Reflect.ownKeys(4) }, TypeError);
assertThrows(function() { Reflect.ownKeys("foo") }, TypeError);
assertThrows(function() { Reflect.ownKeys(true) }, TypeError);

assertEquals(Reflect.ownKeys(Object(4)), []);
assertEquals(Reflect.ownKeys(Object("foo")), ["0", "1", "2", "length"]);
assertEquals(Reflect.ownKeys(Object(true)), []);
