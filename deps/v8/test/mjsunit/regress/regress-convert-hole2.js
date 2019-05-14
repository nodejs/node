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

// Test adding undefined from hole in double-holey to string.
var a = [1.5, , 1.8];

function f(a, i, l) {
  var v = a[i];
  return l + v;
}

assertEquals("test1.5", f(a, 0, "test"));
assertEquals("test1.5", f(a, 0, "test"));
%OptimizeFunctionOnNextCall(f);
assertEquals("testundefined", f(a, 1, "test"));

// Test double-hole going through a phi to a string-add.
function f2(b, a1, a2) {
  var v;
  if (b) {
    v = a1[0];
  } else {
    v = a2[0];
  }
  x = v * 2;
  return "test" + v + x;
}

f2(true, [1.4,1.8,,1.9], [1.4,1.8,,1.9]);
f2(true, [1.4,1.8,,1.9], [1.4,1.8,,1.9]);
f2(false, [1.4,1.8,,1.9], [1.4,1.8,,1.9]);
f2(false, [1.4,1.8,,1.9], [1.4,1.8,,1.9]);
%OptimizeFunctionOnNextCall(f2);
assertEquals("testundefinedNaN", f2(false, [,1.8,,1.9], [,1.9,,1.9]));

// Test converting smi-hole to double-hole.
function t_smi(a) {
  a[0] = 1.5;
}

t_smi([1,,3]);
t_smi([1,,3]);
t_smi([1,,3]);
%OptimizeFunctionOnNextCall(t_smi);
var ta = [1,,3];
t_smi(ta);
ta.__proto__ = [6,6,6];
assertEquals([1.5,6,3], ta);

// Test converting double-hole to tagged-hole.
function t(b) {
  b[1] = {};
}

t([1.4, 1.6,,1.8, NaN]);
t([1.4, 1.6,,1.8, NaN]);
%OptimizeFunctionOnNextCall(t);
var a = [1.6, 1.8,,1.9, NaN];
t(a);
a.__proto__ = [6,6,6,6,6];
assertEquals([1.6, {}, 6, 1.9, NaN], a);
