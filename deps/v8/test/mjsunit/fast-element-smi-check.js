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

var a = new Array(10);

function test_load_set_smi(a) {
  return a[0] = a[0] = 1;
}

test_load_set_smi(a);
test_load_set_smi(a);
test_load_set_smi(123);

function test_load_set_smi_2(a) {
  return a[0] = a[0] = 1;
}

test_load_set_smi_2(a);
%OptimizeFunctionOnNextCall(test_load_set_smi_2);
test_load_set_smi_2(a);
test_load_set_smi_2(0);
%DeoptimizeFunction(test_load_set_smi_2);
%ClearFunctionTypeFeedback(test_load_set_smi_2);

var b = new Object();

function test_load_set_smi_3(b) {
  return b[0] = b[0] = 1;
}

test_load_set_smi_3(b);
test_load_set_smi_3(b);
test_load_set_smi_3(123);

function test_load_set_smi_4(b) {
  return b[0] = b[0] = 1;
}

test_load_set_smi_4(b);
%OptimizeFunctionOnNextCall(test_load_set_smi_4);
test_load_set_smi_4(b);
test_load_set_smi_4(0);
%DeoptimizeFunction(test_load_set_smi_4);
%ClearFunctionTypeFeedback(test_load_set_smi_4);
