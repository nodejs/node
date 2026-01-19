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

function smi_field() {
  // Assign twice to make the field non-constant.
  var o = {smi: 1};
  o.smi = 0;
  return o;
}

function check_smi_repr(o, d1, d2) {
  var s = o.smi;
  var d = d1 - d2;
  s = s + d;
  o.smi = s;
  return o;
};
%PrepareFunctionForOptimization(check_smi_repr);
var test = smi_field();
check_smi_repr(smi_field(), 5, 3);
check_smi_repr(smi_field(), 6, 2);
%OptimizeFunctionOnNextCall(check_smi_repr);
var val = check_smi_repr(smi_field(), 8, 1);
assertTrue(%HaveSameMap(val, test));

function tagged_smi_field() {
  var o = {'tag': false};
  o.tag = 10;
  return o;
}

function check_smi_repr_from_tagged(o, o2) {
  var t = o2.tag;
  o.smi = t;
  return o;
};
%PrepareFunctionForOptimization(check_smi_repr_from_tagged);
check_smi_repr_from_tagged(smi_field(), tagged_smi_field());
check_smi_repr_from_tagged(smi_field(), tagged_smi_field());
%OptimizeFunctionOnNextCall(check_smi_repr_from_tagged);
var val = check_smi_repr_from_tagged(smi_field(), tagged_smi_field());
assertTrue(%HaveSameMap(val, test));
var overflow = tagged_smi_field();
overflow.tag = 0x80000000;
var val = check_smi_repr_from_tagged(smi_field(), overflow);
