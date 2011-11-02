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

// Flags: --allow-natives-syntax

// Check that %NewObjectFromBound works correctly when called from optimized
// frame.
function foo(x, y, z) {
  assertEquals(1, x);
  assertEquals(2, y);
  assertEquals(3, z);
}

var bound_arg = [1];

function f(y, z) {
  return %NewObjectFromBound(foo, bound_arg);
}

// Check that %NewObjectFromBound looks at correct frame for inlined function.
function g(z, y) {
  return f(y, z); /* f should be inlined into g, note rotated arguments */
}

// Check that %NewObjectFromBound looks at correct frame for inlined function.
function ff(x) { }
function h(z2, y2) {
  var local_z = z2 >> 1;
  ff(local_z);
  var local_y = y2 >> 1;
  ff(local_y);
  return f(local_y, local_z); /* f should be inlined into h */
}

for (var i = 0; i < 5; i++) f(2, 3);
%OptimizeFunctionOnNextCall(f);
f(2, 3);

for (var i = 0; i < 5; i++) g(3, 2);
%OptimizeFunctionOnNextCall(g);
g(3, 2);

for (var i = 0; i < 5; i++) h(6, 4);
%OptimizeFunctionOnNextCall(h);
h(6, 4);

// Check that %_IsConstructCall returns correct value when inlined
var NON_CONSTRUCT_MARKER = {};
var CONSTRUCT_MARKER = {};
function baz() {
  return (!%_IsConstructCall()) ? NON_CONSTRUCT_MARKER : CONSTRUCT_MARKER;
}

function bar(x, y, z) {
  var non_construct = baz(); /* baz should be inlined */
  assertEquals(non_construct, NON_CONSTRUCT_MARKER);
  var construct = new baz();
  assertEquals(construct, CONSTRUCT_MARKER);
}

for (var i = 0; i < 5; i++) new bar(1, 2, 3);
%OptimizeFunctionOnNextCall(bar);
bar(1, 2, 3);
