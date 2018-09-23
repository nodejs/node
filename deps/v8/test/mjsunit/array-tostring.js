// Copyright 2011 the V8 project authors. All rights reserved.
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

// Array's toString should call the object's own join method, if one exists and
// is callable. Otherwise, just use the original Object.toString function.

var success = "[test success]";
var expectedThis;
function testJoin() {
  assertEquals(0, arguments.length);
  assertSame(expectedThis, this);
  return success;
}


// On an Array object.

// Default case.
var a1 = [1, 2, 3];
assertEquals(a1.join(), a1.toString());

// Non-standard "join" function is called correctly.
var a2 = [1, 2, 3];
a2.join = testJoin;
expectedThis = a2;
assertEquals(success, a2.toString());

// Non-callable join function is ignored and Object.prototype.toString is
// used instead.
var a3 = [1, 2, 3];
a3.join = "not callable";
assertEquals("[object Array]", a3.toString());

// Non-existing join function is treated same as non-callable.
var a4 = [1, 2, 3];
a4.__proto__ = { toString: Array.prototype.toString };
// No join on Array.
assertEquals("[object Array]", a4.toString());


// On a non-Array object.

// Default looks-like-an-array case.
var o1 = {length: 3, 0: 1, 1: 2, 2: 3,
          toString: Array.prototype.toString,
          join: Array.prototype.join};
assertEquals(o1.join(), o1.toString());


// Non-standard join is called correctly.
// Check that we don't read, e.g., length before calling join.
var o2 = {toString : Array.prototype.toString,
          join: testJoin,
          get length() { assertUnreachable(); },
          get 0() { assertUnreachable(); }};
expectedThis = o2;
assertEquals(success, o2.toString());

// Non-standard join is called even if it looks like an array.
var o3 = {length: 3, 0: 1, 1: 2, 2: 3,
          toString: Array.prototype.toString,
          join: testJoin};
expectedThis = o3;
assertEquals(success, o3.toString());

// Non-callable join works same as for Array.
var o4 = {length: 3, 0: 1, 1: 2, 2: 3,
          toString: Array.prototype.toString,
          join: "not callable"};
assertEquals("[object Object]", o4.toString());


// Non-existing join works same as for Array.
var o5 = {length: 3, 0: 1, 1: 2, 2: 3,
          toString: Array.prototype.toString
          /* no join */};
assertEquals("[object Object]", o5.toString());


// Test that ToObject is called before getting "join", so the instance
// that "join" is read from is the same one passed as receiver later.
var called_before = false;
expectedThis = null;
Object.defineProperty(Number.prototype, "join", {get: function() {
            assertFalse(called_before);
            called_before = true;
            expectedThis = this;
            return testJoin;
        }});
Number.prototype.arrayToString = Array.prototype.toString;
assertEquals(success, (42).arrayToString());

// ----------------------------------------------------------
// Testing Array.prototype.toLocaleString

// Ensure that it never uses Array.prototype.toString for anything.
Array.prototype.toString = function() { assertUnreachable(); };

// Default case.
var la1 = [1, [2, 3], 4];
assertEquals("1,2,3,4", la1.toLocaleString());

// Used on a string (which looks like an array of characters).
String.prototype.toLocaleString = Array.prototype.toLocaleString;
assertEquals("1,2,3,4", "1234".toLocaleString());

// If toLocaleString of element is not callable, throw a TypeError.
var la2 = [1, {toLocaleString: "not callable"}, 3];
assertThrows(function() { la2.toLocaleString(); }, TypeError);

// If toLocaleString of element is callable, call it.
var la3 = [1, {toLocaleString: function() { return "XX";}}, 3];
assertEquals("1,XX,3", la3.toLocaleString());

// Omitted elements, as well as undefined and null, become empty string.
var la4 = [1, null, 3, undefined, 5,, 7];
assertEquals("1,,3,,5,,7", la4.toLocaleString());


// ToObject is called first and the same object is being used for the
// rest of the operations.
Object.defineProperty(Number.prototype, "length", {
    get: function() {
      exptectedThis = this;
      return 3;
    }});
for (var i = 0; i < 3; i++) {
  Object.defineProperty(Number.prototype, i, {
      get: function() {
        assertEquals(expectedThis, this);
        return +this;
      }});
}
Number.prototype.arrayToLocaleString = Array.prototype.toLocaleString;
assertEquals("42,42,42", (42).arrayToLocaleString());
