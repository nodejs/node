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

function foo_smi(a, b) {
  var result = a * b;
  result += a * 35;
  result += a * -1;
  result += a * 1;
  result += a * 0;
  return result * a;
}

function foo_int(a, b) {
  var result = a * b;
  result += a * 35;
  result += a * -1;
  result += a * 1;
  result += a * 0;
  return result * a;
}

%PrepareFunctionForOptimization(foo_smi);
foo_smi(10, 5);
var r1 = foo_smi(10, 5);
%OptimizeFunctionOnNextCall(foo_smi);
var r2 = foo_smi(10, 5);

assertEquals(r1, r2);

%PrepareFunctionForOptimization(foo_int);
foo_int(10, 21474800);
var r3 = foo_int(10, 21474800);
%OptimizeFunctionOnNextCall(foo_int);
var r4 = foo_int(10, 21474800);

assertEquals(r3, r4);

// Check overflow with -1 constant.
function foo2(value) {
  return value * -1;
}

%PrepareFunctionForOptimization(foo2);
foo2(-2147483600);
foo2(-2147483600);
%OptimizeFunctionOnNextCall(foo2);
assertEquals(2147483648, foo2(-2147483648));
