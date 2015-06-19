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

// Flags: --allow-natives-syntax --expose-gc

// Test uint32 handing in optimized frames.

var K1 = 0x7fffffff;
var K2 = 0xffffffff;

var uint32_array = new Uint32Array(2);
uint32_array[0] = K1;
uint32_array[1] = K2;

function ChangeI2T(arr, i) {
  return uint32_array[i];
}

assertEquals(K1, ChangeI2T(uint32_array, 0));
assertEquals(K2, ChangeI2T(uint32_array, 1));
%OptimizeFunctionOnNextCall(ChangeI2T);
assertEquals(K1, ChangeI2T(uint32_array, 0));
// Loop to force inline allocation failure and a call into runtime.
for (var i = 0; i < 80000; i++) {
  assertEquals(K2, ChangeI2T(uint32_array, 1));
}

function SideEffect() {
  with ({}) { }  // not inlinable
}

function Deopt(obj, arr, i) {
  var x = arr[i];
  SideEffect();  // x will be used by HSimulate.
  obj.x;
  return x;
}

assertEquals(K1, Deopt({x: 0}, uint32_array, 0));
assertEquals(K2, Deopt({x: 0}, uint32_array, 1));
%OptimizeFunctionOnNextCall(Deopt);
assertEquals(K2, Deopt({}, uint32_array, 1));

function ChangeI2D(arr) {
  // This addition will have a double type feedback so ChangeI2D will
  // be generated for its operands.
  return arr[0] + arr[1];
}

assertEquals(K1 + K2, ChangeI2D(uint32_array));
assertEquals(K1 + K2, ChangeI2D(uint32_array));
%OptimizeFunctionOnNextCall(ChangeI2D);
assertEquals(K1 + K2, ChangeI2D(uint32_array));

function ShrShr(val) {
  return (val >>> 0) >>> 1;
}

assertEquals(K1, ShrShr(K2 | 0));
assertEquals(K1, ShrShr(K2 | 0));
%OptimizeFunctionOnNextCall(ShrShr);
assertEquals(K1, ShrShr(K2 | 0));

function SarShr(val) {
  return val >> (-2 >>> 0);
}

var K3 = 0x80000000;
assertEquals(-2, SarShr(K3 | 0));
assertEquals(-2, SarShr(K3 | 0));
%OptimizeFunctionOnNextCall(SarShr);
assertEquals(-2, SarShr(K3 | 0));

function Uint32Phi(a, b, c) {
  var i = a ? (b >>> 0) : (c >>> 0);
  return (i | 0);
}

var K4 = 0x80000001;
assertEquals(K3 | 0, Uint32Phi(true, K3, K4));
assertEquals(K4 | 0, Uint32Phi(false, K3, K4));
assertEquals(K3 | 0, Uint32Phi(true, K3, K4));
assertEquals(K4 | 0, Uint32Phi(false, K3, K4));
%OptimizeFunctionOnNextCall(Uint32Phi);
assertEquals(K3 | 0, Uint32Phi(true, K3, K4));
assertEquals(K4 | 0, Uint32Phi(false, K3, K4));

function NonUint32Phi(a, b, c) {
  var i = a ? (b >>> 0) : c;
  return (i | 0);
}

assertEquals(K3 | 0, NonUint32Phi(true, K3, K4));
assertEquals(K4 | 0, NonUint32Phi(false, K3, K4));
assertEquals(K3 | 0, NonUint32Phi(true, K3, K4));
assertEquals(K4 | 0, NonUint32Phi(false, K3, K4));
%OptimizeFunctionOnNextCall(NonUint32Phi);
assertEquals(K3 | 0, NonUint32Phi(true, K3, K4));
assertEquals(K4 | 0, NonUint32Phi(false, K3, K4));

function PhiOfPhi(x) {
  var a = (x >>> 0);
  for (var i = 0; i < 2; i++) {
    for (var j = 0; j < 2; j++) {
      a = (a >>> 0);
    }
  }
  return (a | 0);
}

assertEquals(1, PhiOfPhi(1));
assertEquals(1, PhiOfPhi(1));
%OptimizeFunctionOnNextCall(PhiOfPhi);
assertEquals(K3 | 0, PhiOfPhi(K3));

function PhiOfPhiUnsafe(x) {
  var a = x >>> 0;
  for (var i = 0; i < 2; i++) {
    for (var j = 0; j < 2; j++) {
      a = (a >>> 0);
    }
  }
  return a + a;
}

assertEquals(2, PhiOfPhiUnsafe(1));
assertEquals(2, PhiOfPhiUnsafe(1));
%OptimizeFunctionOnNextCall(PhiOfPhiUnsafe);
assertEquals(2 * K3, PhiOfPhiUnsafe(K3));

var old_array = new Array(1000);

for (var i = 0; i < old_array.length; i++) old_array[i] = null;

// Force promotion.
gc();
gc();

function FillOldArrayWithHeapNumbers(N) {
  for (var i = 0; i < N; i++) {
    old_array[i] = uint32_array[1];
  }
}

FillOldArrayWithHeapNumbers(1);
FillOldArrayWithHeapNumbers(1);
%OptimizeFunctionOnNextCall(FillOldArrayWithHeapNumbers);
FillOldArrayWithHeapNumbers(old_array.length);
gc();

// Test that HArgumentsObject does not prevent uint32 optimization and
// that arguments object with uint32 values inside is correctly materialized.
function Pack(x, y) {
  try {  // Prevent inlining.
    return [x, y];
  } catch (e) {
  }
}

function InnerWithArguments(x, f) {
  "use strict";
  x >>>= 8;
  return f(arguments[0], x|0);
}

function Outer(v, f) {
  return InnerWithArguments(v >>> 0, f);
}

assertArrayEquals([0x0100, 0x01], Outer(0x0100, Pack));
assertArrayEquals([0x0100, 0x01], Outer(0x0100, Pack));
assertArrayEquals([0x0100, 0x01], Outer(0x0100, Pack));
%OptimizeFunctionOnNextCall(Outer);
assertArrayEquals([0x0100, 0x01], Outer(0x0100, Pack));
assertArrayEquals([0xFFFFFFFF, 0x00FFFFFF], Outer(-1, Pack));

// Cause deopt inside InnerWithArguments by passing different pack function.
assertArrayEquals([0xFFFFFFFF, 0x00FFFFFF], Outer(-1, function (x, y) {
  return [x, y];
}));
