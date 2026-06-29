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

function foo(a, i, v) {
  a[0] = v;
  a[i] = v;
}

function foo_int(a, i, v) {
  a[0] = v;
  a[i] = v;
}

var A1 = [1.2, 2.3];
var A2 = [1.2, 2.3];
var A3 = [1.2, 2.3];

var A1_int = [12, 23];
var A2_int = [12, 23];
var A3_int = [12, 23];

%PrepareFunctionForOptimization(foo);
foo(A1, 1, 3.4);
foo(A2, 1, 3.4);
%OptimizeFunctionOnNextCall(foo);
foo(A3, 1, 3.4);

%PrepareFunctionForOptimization(foo_int);
foo_int(A1_int, 1, 34);
foo_int(A2_int, 1, 34);
%OptimizeFunctionOnNextCall(foo_int);
foo_int(A3_int, 1, 34);

assertEquals(A1[0], A3[0]);
assertEquals(A1[1], A3[1]);
assertEquals(A1_int[0], A3_int[0]);
assertEquals(A1_int[1], A3_int[1]);
