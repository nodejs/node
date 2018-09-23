// Copyright 2008 the V8 project authors. All rights reserved.
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

// Test that array join calls toString on subarrays.
var a = [[1,2],3,4,[5,6]];
assertEquals('1,2345,6', a.join(''));
assertEquals('1,2*3*4*5,6', a.join('*'));
assertEquals('1,2**3**4**5,6', a.join('**'));
assertEquals('1,2****3****4****5,6', a.join('****'));
assertEquals('1,2********3********4********5,6', a.join('********'));
assertEquals('1,2**********3**********4**********5,6', a.join('**********'));

// Create a cycle.
a.push(a);
assertEquals('1,2345,6', a.join(''));
assertEquals('1,2*3*4*5,6*', a.join('*'));
assertEquals('1,2**3**4**5,6**', a.join('**'));
assertEquals('1,2****3****4****5,6****', a.join('****'));
assertEquals('1,2********3********4********5,6********', a.join('********'));
assertEquals('1,2**********3**********4**********5,6**********', a.join('**********'));

// Replace array.prototype.toString.
var oldToString = Array.prototype.toString;
Array.prototype.toString = function() { return "array"; };
assertEquals('array34arrayarray', a.join(''));
assertEquals('array*3*4*array*array', a.join('*'));
assertEquals('array**3**4**array**array', a.join('**'));
assertEquals('array****3****4****array****array', a.join('****'));
assertEquals('array********3********4********array********array', a.join('********'));
assertEquals('array**********3**********4**********array**********array', a.join('**********'));

Array.prototype.toString = function() { throw 42; };
assertThrows("a.join('')");
assertThrows("a.join('*')");
assertThrows("a.join('**')");
assertThrows("a.join('****')");
assertThrows("a.join('********')");
assertThrows("a.join('**********')");

Array.prototype.toString = function() { return "array"; };
assertEquals('array34arrayarray', a.join(''));
assertEquals('array*3*4*array*array', a.join('*'));
assertEquals('array**3**4**array**array', a.join('**'));
assertEquals('array****3****4****array****array', a.join('****'));
assertEquals('array********3********4********array********array', a.join('********'));
assertEquals('array**********3**********4**********array**********array', a.join('**********'));

// Restore original toString.
delete Array.prototype.toString;
if (Array.prototype.toString != oldToString) {
  Array.prototype.toString = oldToString;
}

var a = new Array(123123);
assertEquals(123122, String(a).length);
assertEquals(123122, a.join(",").length);
assertEquals(246244, a.join("oo").length);

a = new Array(Math.pow(2,32) - 1);  // Max length.
assertEquals("", a.join(""));
a[123123123] = "o";
a[1255215215] = "p";
assertEquals("op", a.join(""));

a = new Array(100001);
for (var i = 0; i < a.length; i++) a[i] = undefined;
a[5] = "ab";
a[90000] = "cd";
assertEquals("abcd", a.join(""));  // Must not throw.
