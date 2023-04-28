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

function f_store(test, test2, a, i) {
  var o = [0.5,1,,3];
  var d;
  if (test) {
    d = 1.5;
  } else {
    d = o[i];
  }
  if (test2) {
    d += 1;
  }
  a[i] = d;
  return d;
}

var a1 = [0, 0, 0, {}];
%PrepareFunctionForOptimization(f_store);
f_store(true, false, a1, 0);
f_store(true, true, a1, 0);
f_store(false, false, a1, 1);
f_store(false, true, a1, 1);
%OptimizeFunctionOnNextCall(f_store);
f_store(false, false, a1, 2);
assertEquals(undefined, a1[2]);

function test_arg(expected) {
  return function(v) {
    assertEquals(expected, v);
  }
}

function f_call(f, test, test2, i) {
  var o = [0.5,1,,3];
  var d;
  if (test) {
    d = 1.5;
  } else {
    d = o[i];
  }
  if (test2) {
    d += 1;
  }
  f(d);
  return d;
}

%PrepareFunctionForOptimization(f_call);
f_call(test_arg(1.5), true, false, 0);
f_call(test_arg(2.5), true, true, 0);
f_call(test_arg(1), false, false, 1);
f_call(test_arg(2), false, true, 1);
%OptimizeFunctionOnNextCall(f_call);
f_call(test_arg(undefined), false, false, 2);


function f_external(test, test2, test3, a, i) {
  var o = [0.5,1,,3];
  var d;
  if (test) {
    d = 1.5;
  } else {
    d = o[i];
  }
  if (test2) {
    d += 1;
  }
  if (test3) {
    d = d|0;
  }
  a[d] = 1;
  assertEquals(1, a[d]);
  return d;
}

var a2 = new Int32Array(10);
%PrepareFunctionForOptimization(f_external);
f_external(true, false, true, a2, 0);
f_external(true, true, true, a2, 0);
f_external(false, false, true, a2, 1);
f_external(false, true, true, a2, 1);
%OptimizeFunctionOnNextCall(f_external);
f_external(false, false, false, a2, 2);
assertEquals(1, a2[undefined]);
