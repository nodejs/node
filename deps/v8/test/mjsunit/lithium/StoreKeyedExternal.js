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

// Flags: --allow-natives-syntax --no-use-osr

function foo_pixel(a, i, v) {
  a[0] = v;
  a[i] = v;
}

function foo_uint16(a, i, v) {
  a[0] = v;
  a[i] = v;
}

function foo_uint32(a, i, v) {
  a[0] = v;
  a[i] = v;
}

function foo_float(a, i, v) {
  a[0] = v;
  a[i] = v;
}

function foo_double(a, i, v) {
  a[0] = v;
  a[i] = v;
}

var A1_pixel = new Uint8ClampedArray(2);
var A2_pixel = new Uint8ClampedArray(2);
var A3_pixel = new Uint8ClampedArray(2);

var A1_uint16 = new Uint16Array(2);
var A2_uint16 = new Uint16Array(2);
var A3_uint16 = new Uint16Array(2);

var A1_uint32 = new Uint32Array(2);
var A2_uint32 = new Uint32Array(2);
var A3_uint32 = new Uint32Array(2);

var A1_float = new Float32Array(2);
var A2_float = new Float32Array(2);
var A3_float = new Float32Array(2);

var A1_double = new Float64Array(2);
var A2_double = new Float64Array(2);
var A3_double = new Float64Array(2);

foo_pixel(A1_pixel, 1, 34);
foo_pixel(A2_pixel, 1, 34);
%OptimizeFunctionOnNextCall(foo_pixel);
foo_pixel(A3_pixel, 1, 34);

foo_uint16(A1_uint16, 1, 3.4);
foo_uint16(A2_uint16, 1, 3.4);
%OptimizeFunctionOnNextCall(foo_uint16);
foo_uint16(A3_uint16, 1, 3.4);

foo_uint32(A1_uint32, 1, 3.4);
foo_uint32(A2_uint32, 1, 3.4);
%OptimizeFunctionOnNextCall(foo_uint32);
foo_uint32(A3_uint32, 1, 3.4);

foo_float(A1_float, 1, 3.4);
foo_float(A2_float, 1, 3.4);
%OptimizeFunctionOnNextCall(foo_float);
foo_float(A3_float, 1, 3.4);

foo_double(A1_double, 1, 3.4);
foo_double(A2_double, 1, 3.4);
%OptimizeFunctionOnNextCall(foo_double);
foo_double(A3_double, 1, 3.4);

assertEquals(A1_pixel[0], A3_pixel[0]);
assertEquals(A1_pixel[1], A3_pixel[1]);
assertEquals(A1_uint16[0], A3_uint16[0]);
assertEquals(A1_uint16[1], A3_uint16[1]);
assertEquals(A1_uint32[0], A3_uint32[0]);
assertEquals(A1_uint32[1], A3_uint32[1]);
assertEquals(A1_float[0], A3_float[0]);
assertEquals(A1_float[1], A3_float[1]);
assertEquals(A1_double[0], A3_double[0]);
assertEquals(A1_double[1], A3_double[1]);
