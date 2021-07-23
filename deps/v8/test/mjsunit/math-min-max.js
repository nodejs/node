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

// Flags: --allow-natives-syntax

// Test Math.min().

assertEquals(Infinity, Math.min());
assertEquals(1, Math.min(1));
assertEquals(1, Math.min(1, 2));
assertEquals(1, Math.min(2, 1));
assertEquals(1, Math.min(1, 2, 3));
assertEquals(1, Math.min(3, 2, 1));
assertEquals(1, Math.min(2, 3, 1));
assertEquals(1.1, Math.min(1.1, 2.2, 3.3));
assertEquals(1.1, Math.min(3.3, 2.2, 1.1));
assertEquals(1.1, Math.min(2.2, 3.3, 1.1));

// Prepare a non-Smi zero value.
function returnsNonSmi(){ return 0.25; }
var ZERO = (function() {
  var z;
  // We have to have a loop here because the first time we get a Smi from the
  // runtime system.  After a while the binary op IC settles down and we get
  // a non-Smi from the generated code.
  for (var i = 0; i < 10; i++) {
    z = returnsNonSmi() - returnsNonSmi();
  }
  return z;
})();
assertEquals(0, ZERO);
assertEquals(Infinity, 1/ZERO);
assertEquals(-Infinity, 1/-ZERO);
// Here we would like to have assertFalse(%IsSmi(ZERO));  This is, however,
// unreliable, since a new space exhaustion at a critical moment could send
// us into the runtime system, which would quite legitimately put a Smi zero
// here.
assertFalse(%IsSmi(-ZERO));

var o = {};
o.valueOf = function() { return 1; };
assertEquals(1, Math.min(2, 3, '1'));
assertEquals(1, Math.min(3, o, 2));
assertEquals(1, Math.min(o, 2));
assertEquals(-Infinity, Infinity / Math.min(-0, +0));
assertEquals(-Infinity, Infinity / Math.min(+0, -0));
assertEquals(-Infinity, Infinity / Math.min(+0, -0, 1));
assertEquals(-Infinity, Infinity / Math.min(-0, ZERO));
assertEquals(-Infinity, Infinity / Math.min(ZERO, -0));
assertEquals(-Infinity, Infinity / Math.min(ZERO, -0, 1));
assertEquals(-1, Math.min(+0, -0, -1));
assertEquals(-1, Math.min(-1, +0, -0));
assertEquals(-1, Math.min(+0, -1, -0));
assertEquals(-1, Math.min(-0, -1, +0));
assertEquals(NaN, Math.min('oxen'));
assertEquals(NaN, Math.min('oxen', 1));
assertEquals(NaN, Math.min(1, 'oxen'));


// Test Math.max().

assertEquals(Number.NEGATIVE_INFINITY, Math.max());
assertEquals(1, Math.max(1));
assertEquals(2, Math.max(1, 2));
assertEquals(2, Math.max(2, 1));
assertEquals(3, Math.max(1, 2, 3));
assertEquals(3, Math.max(3, 2, 1));
assertEquals(3, Math.max(2, 3, 1));
assertEquals(3.3, Math.max(1.1, 2.2, 3.3));
assertEquals(3.3, Math.max(3.3, 2.2, 1.1));
assertEquals(3.3, Math.max(2.2, 3.3, 1.1));

var o = {};
o.valueOf = function() { return 3; };
assertEquals(3, Math.max(2, '3', 1));
assertEquals(3, Math.max(1, o, 2));
assertEquals(3, Math.max(o, 1));
assertEquals(Infinity, Infinity / Math.max(-0, +0));
assertEquals(Infinity, Infinity / Math.max(+0, -0));
assertEquals(Infinity, Infinity / Math.max(+0, -0, -1));
assertEquals(Infinity, Infinity / Math.max(-0, ZERO));
assertEquals(Infinity, Infinity / Math.max(ZERO, -0));
assertEquals(Infinity, Infinity / Math.max(ZERO, -0, -1));
assertEquals(1, Math.max(+0, -0, +1));
assertEquals(1, Math.max(+1, +0, -0));
assertEquals(1, Math.max(+0, +1, -0));
assertEquals(1, Math.max(-0, +1, +0));
assertEquals(NaN, Math.max('oxen'));
assertEquals(NaN, Math.max('oxen', 1));
assertEquals(NaN, Math.max(1, 'oxen'));

assertEquals(Infinity, 1/Math.max(ZERO, -0));
assertEquals(Infinity, 1/Math.max(-0, ZERO));

function run(crankshaft_test) {
  %PrepareFunctionForOptimization(crankshaft_test);
  crankshaft_test(1);
  crankshaft_test(1);
  %OptimizeFunctionOnNextCall(crankshaft_test);
  crankshaft_test(-0);
}

function crankshaft_test_1(arg) {
  var v1 = 1;
  var v2 = 5;
  var v3 = 1.5;
  var v4 = 5.5;
  var v5 = 2;
  var v6 = 6;
  var v7 = 0;
  var v8 = -0;

  var v9 = 9.9;
  var v0 = 10.1;
  // Integer32 representation.
  assertEquals(v2, Math.max(v1++, v2++));
  assertEquals(v1, Math.min(v1++, v2++));
  // Tagged representation.
  assertEquals(v4, Math.max(v3, v4));
  assertEquals(v3, Math.min(v3, v4));
  assertEquals(v6, Math.max(v5, v6));
  assertEquals(v5, Math.min(v5, v6));
  // Double representation.
  assertEquals(v0, Math.max(v0++, v9++));
  assertEquals(v9, Math.min(v0++, v9++));
  // Mixed representation.
  assertEquals(v1, Math.min(v1++, v9++));  // int32, double
  assertEquals(v0, Math.max(v0++, v2++));  // double, int32
  assertEquals(v1, Math.min(v1++, v6));    // int32, tagged
  assertEquals(v2, Math.max(v5, v2++));    // tagged, int32
  assertEquals(v6, Math.min(v6, v9++));    // tagged, double
  assertEquals(v0, Math.max(v0++, v5));    // double, tagged

  // Minus zero.
  assertEquals(Infinity, 1/Math.max(v7, v8));
  assertEquals(-Infinity, 1/Math.min(v7, v8));
  // NaN.
  assertEquals(NaN, Math.max(NaN, v8));
  assertEquals(NaN, Math.min(NaN, v9));
  assertEquals(NaN, Math.max(v8, NaN));
  assertEquals(NaN, Math.min(v9, NaN));
  // Minus zero as Integer32.
  assertEquals((arg === -0) ? -Infinity : 1, 1/Math.min(arg, v2));
}

run(crankshaft_test_1);

function crankshaft_test_2() {
  var v9 = {};
  v9.valueOf = function() { return 6; }
  // Deopt expected due to non-heapnumber objects.
  assertEquals(6, Math.min(v9, 12));
}

run(crankshaft_test_2);

var o = { a: 1, b: 2 };

// Test smi-based Math.min.
function f(o) {
  return Math.min(o.a, o.b);
}

%PrepareFunctionForOptimization(f);
assertEquals(1, f(o));
assertEquals(1, f(o));
%OptimizeFunctionOnNextCall(f);
assertEquals(1, f(o));
o.a = 5;
o.b = 4;
assertEquals(4, f(o));

// Test overriding Math.min and Math.max
Math.min = function(a, b) { return a + b; }
Math.max = function(a, b) { return a - b; }

function crankshaft_test_3() {
  assertEquals(8, Math.min(3, 5));
  assertEquals(3, Math.max(5, 2));
}

run(crankshaft_test_3);
