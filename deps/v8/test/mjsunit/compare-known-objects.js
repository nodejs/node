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

// Flags: --allow-natives-syntax

// Test CompareIC stubs for normal and strict equality comparison of known
// objects in slow mode. These objects share the same map even though they
// might have completely different properties.

function eq(a, b) {
  return a == b;
}

function eq_strict(a, b) {
  return a === b;
}

function test(a, b) {
  // Check CompareIC for equality of known objects.
  assertTrue(eq(a, a));
  assertTrue(eq(b, b));
  assertFalse(eq(a, b));
  // Check CompareIC for strict equality of known objects.
  assertTrue(eq_strict(a, a));
  assertTrue(eq_strict(b, b));
  assertFalse(eq_strict(a, b));
}

function O(){};
O.prototype.t = function() {}

var obj1 = new O;
var obj2 = new O;

// Test original objects.
assertTrue(%HaveSameMap(obj1, obj2));
test(obj1, obj2);

// Test after adding property to first object.
obj1.x = 1;
test(obj1, obj2);
