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

// Check that %NewObjectFromBound works correctly when called from optimized
// frame.
function foo1(x, y, z) {
  assertEquals(1, x);
  assertEquals(2, y);
  assertEquals(3, z);
}

function foo2(x, y, z) {
  assertEquals(1, x);
  assertEquals(2, y);
  assertEquals(undefined, z);
}

function foo3(x, y, z) {
  assertEquals(1, x);
  assertEquals(2, y);
  assertEquals(3, z);
}


var foob1 = foo1.bind({}, 1);
var foob2 = foo2.bind({}, 1);
var foob3 = foo3.bind({}, 1);


function f1(y, z) {
  return %NewObjectFromBound(foob1);
}

function f2(y, z) {
  return %NewObjectFromBound(foob2);
}

function f3(y, z) {
  return %NewObjectFromBound(foob3);
}

// Check that %NewObjectFromBound looks at correct frame for inlined function.
function g1(z, y) {
  return f1(y, z); /* f should be inlined into g, note rotated arguments */
}

function g2(z, y, x) {
  return f2(y); /* f should be inlined into g, note argument count mismatch */
}

function g3(z, y, x) {
  return f3(x, y, z); /* f should be inlined into g, note argument count mismatch */
}

// Check that %NewObjectFromBound looks at correct frame for inlined function.
function ff(x) { }
function h1(z2, y2) {
  var local_z = z2 >> 1;
  ff(local_z);
  var local_y = y2 >> 1;
  ff(local_y);
  return f1(local_y, local_z); /* f should be inlined into h */
}

function h2(z2, y2, x2) {
  var local_z = z2 >> 1;
  ff(local_z);
  var local_y = y2 >> 1;
  ff(local_y);
  return f2(local_y); /* f should be inlined into h */
}

function h3(z2, y2, x2) {
  var local_z = z2 >> 1;
  ff(local_z);
  var local_y = y2 >> 1;
  ff(local_y);
  var local_x = x2 >> 1;
  ff(local_x);
  return f3(local_x, local_y, local_z); /* f should be inlined into h */
}


function invoke(f, args) {
  for (var i = 0; i < 5; i++) f.apply(this, args);
  %OptimizeFunctionOnNextCall(f);
  f.apply(this, args);
}

invoke(f1, [2, 3]);
invoke(f2, [2]);
invoke(f3, [2, 3, 4]);
invoke(g1, [3, 2]);
invoke(g2, [3, 2, 4]);
invoke(g3, [4, 3, 2]);
invoke(h1, [6, 4]);
invoke(h2, [6, 4, 8]);
invoke(h3, [8, 6, 4]);

// Check that %_IsConstructCall returns correct value when inlined
var NON_CONSTRUCT_MARKER = {};
var CONSTRUCT_MARKER = {};
function baz(x) {
  return (!%_IsConstructCall()) ? NON_CONSTRUCT_MARKER : CONSTRUCT_MARKER;
}

function bar(x, y, z) {
  var non_construct = baz(0); /* baz should be inlined */
  assertSame(non_construct, NON_CONSTRUCT_MARKER);
  var non_construct = baz(); /* baz should be inlined */
  assertSame(non_construct, NON_CONSTRUCT_MARKER);
  var non_construct = baz(0, 0); /* baz should be inlined */
  assertSame(non_construct, NON_CONSTRUCT_MARKER);
  var construct = new baz(0);
  assertSame(construct, CONSTRUCT_MARKER);
  var construct = new baz(0, 0);
  assertSame(construct, CONSTRUCT_MARKER);
}

invoke(bar, [1, 2, 3]);
