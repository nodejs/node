// Copyright 2013 the V8 project authors. All rights reserved.
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

// Flags: --expose-gc --allow-natives-syntax

function f() {
  return 23;
}

function call(o) {
  return o['']();
}

function test() {
  var o1 = %ToFastProperties(Object.create({ foo:1 }, { '': { value:f }}));
  var o2 = %ToFastProperties(Object.create({ bar:1 }, { '': { value:f }}));
  var o3 = %ToFastProperties(Object.create({ baz:1 }, { '': { value:f }}));
  var o4 = %ToFastProperties(Object.create({ qux:1 }, { '': { value:f }}));
  var o5 = %ToFastProperties(Object.create({ loo:1 }, { '': { value:f }}));
  // Called twice on o1 to turn monomorphic.
  assertEquals(23, call(o1));
  assertEquals(23, call(o1));
  // Called on four other objects to turn megamorphic.
  assertEquals(23, call(o2));
  assertEquals(23, call(o3));
  assertEquals(23, call(o4));
  assertEquals(23, call(o5));
  return o1;
}

// Fill stub cache with entries.
test();

// Clear stub cache during GC.
gc();

// Turn IC megamorphic again.
var oboom = test();

// Optimize with previously cleared stub cache.
%OptimizeFunctionOnNextCall(call);
assertEquals(23, call(oboom));
