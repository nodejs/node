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

// Flags: --allow-natives-syntax

var imul_func = Math.imul;
function imul_polyfill(a, b) {
  var ah = a >>> 16 & 0xffff;
  var al = a & 0xffff;
  var bh = b >>> 16 & 0xffff;
  var bl = b & 0xffff;
  return al * bl + (ah * bl + al * bh << 16 >>> 0) | 0;
}

function TestMathImul(expected, a, b) {
  function imul_meth_closure(a, b) {
    return Math.imul(a, b);
  };
  %PrepareFunctionForOptimization(imul_meth_closure);
  function imul_func_closure(a, b) {
    return imul_func(a, b);
  }

  // Test reference implementation.
  ;
  %PrepareFunctionForOptimization(imul_func_closure);
  assertEquals(expected, imul_polyfill(a, b));
  // Test direct method call.
  assertEquals(expected, Math.imul(a, b));

  // Test direct function call.
  assertEquals(expected, imul_func(a, b));

  // Test optimized method call inside closure.
  assertEquals(expected, imul_meth_closure(a, b));
  assertEquals(expected, imul_meth_closure(a, b));
  %OptimizeFunctionOnNextCall(imul_meth_closure);
  assertEquals(expected, imul_meth_closure(a, b));

  // Test optimized function call inside closure.
  assertEquals(expected, imul_func_closure(a, b));
  assertEquals(expected, imul_func_closure(a, b));
  %OptimizeFunctionOnNextCall(imul_func_closure);
  assertEquals(expected, imul_func_closure(a, b));

  // Deoptimize closures and forget type feedback.
  %DeoptimizeFunction(imul_meth_closure);
  %DeoptimizeFunction(imul_func_closure);
  %ClearFunctionFeedback(imul_meth_closure);
  %ClearFunctionFeedback(imul_func_closure);
}

TestMathImul(8, 2, 4);
TestMathImul(-8, -1, 8);
TestMathImul(4, -2, -2);
TestMathImul(-5, 0xffffffff, 5);
TestMathImul(-10, 0xfffffffe, 5);

TestMathImul(0, false, 7);
TestMathImul(0, 7, false);
TestMathImul(0, false, false);

TestMathImul(7, true, 7);
TestMathImul(7, 7, true);
TestMathImul(1, true, true);

TestMathImul(0, false, true);
TestMathImul(0, true, false);

TestMathImul(0, undefined, 7);
TestMathImul(0, 7, undefined);
TestMathImul(0, undefined, undefined);

TestMathImul(0, -0, 7);
TestMathImul(0, 7, -0);

TestMathImul(0, 0.1, 7);
TestMathImul(0, 7, 0.1);
TestMathImul(0, 0.9, 7);
TestMathImul(0, 7, 0.9);
TestMathImul(7, 1.1, 7);
TestMathImul(7, 7, 1.1);
TestMathImul(7, 1.9, 7);
TestMathImul(7, 7, 1.9);

TestMathImul(0, "str", 7);
TestMathImul(0, 7, "str");

TestMathImul(0, {}, 7);
TestMathImul(0, 7, {});

TestMathImul(0, [], 7);
TestMathImul(0, 7, []);

// 2^30 is a smi boundary on arm and ia32.
var two_30 = 1 << 30;

TestMathImul(-two_30, two_30, 7);
TestMathImul(-two_30, 7, two_30);
TestMathImul(0, two_30, two_30);

TestMathImul(two_30, -two_30, 7);
TestMathImul(two_30, 7, -two_30);
TestMathImul(0, -two_30, -two_30);

// 2^31 is a smi boundary on x64.
var two_31 = 2 * two_30;

TestMathImul(-two_31, two_31, 7);
TestMathImul(-two_31, 7, two_31);
TestMathImul(0, two_31, two_31);

TestMathImul(-two_31, -two_31, 7);
TestMathImul(-two_31, 7, -two_31);
TestMathImul(0, -two_31, -two_31);

// 2^31 - 1 is the largest int32 value.
var max_val = two_31 - 1;

TestMathImul(two_31 - 7, max_val, 7);
TestMathImul(two_31 - 7, 7, max_val);
TestMathImul(1, max_val, max_val);

// 2^16 is a boundary value that overflows when squared.
var two_16 = 1 << 16;

TestMathImul(0, two_16, two_16);
TestMathImul(-two_16, two_16 - 1, two_16);
TestMathImul(-two_16, two_16, two_16 - 1);
TestMathImul(-2 * two_16 + 1, two_16 - 1, two_16 - 1);
