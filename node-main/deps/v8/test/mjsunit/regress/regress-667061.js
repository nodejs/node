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

// Test non-ICC case.
var caught = false;
try {
  (('foo'))();
} catch (o) {
  assertTrue(o instanceof TypeError);
  caught = true;
}
assertTrue(caught);


// Test uninitialized case.
function h(o) {
  return o.x();
}

var caught = false;
try {
  h({ x: 1 });
} catch (o) {
  assertTrue(o instanceof TypeError);
  caught = true;
}
assertTrue(caught);


// Test monomorphic case.
function g(o) {
  return o.x();
}

function O(x) { this.x = x; };
var o = new O(function() { return 1; });
assertEquals(1, g(o));  // go monomorphic
assertEquals(1, g(o));  // stay monomorphic

var caught = false;
try {
  g(new O(3));
} catch (o) {
  assertTrue(o instanceof TypeError);
  caught = true;
}
assertTrue(caught);


// Test megamorphic case.
function f(o) {
  return o.x();
}

assertEquals(1, f({ x: function () { return 1; }}));  // go monomorphic
assertEquals(2, f({ x: function () { return 2; }}));  // go megamorphic
assertEquals(3, f({ x: function () { return 3; }}));  // stay megamorphic

var caught = false;
try {
  f({ x: 4 });
} catch (o) {
  assertTrue(o instanceof TypeError);
  caught = true;
}
assertTrue(caught);
