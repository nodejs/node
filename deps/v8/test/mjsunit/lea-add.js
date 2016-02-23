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

function a() {
  var sum = 0;
  for (var i = 0; i < 500; ++i) {
    sum = (i + sum) | 0;
  }
  return sum;
}

function b() {
  var sum = 0;
  for (var i = -500; i < 0; ++i) {
    sum = (i + sum) | 0;
  }
  return sum;
}

function c() {
  var sum = 0;
  for (var i = 0; i < 500; ++i) {
    sum += (i + -0x7fffffff) | 0;
  }
  return sum;
}

function d() {
  var sum = 0;
  for (var i = -501; i < 0; ++i) {
    sum += (i + 501) | 0;
  }
  return sum;
}

a();
a();
%OptimizeFunctionOnNextCall(a);
assertEquals(124750, a());
assertEquals(124750, a());

b();
b();
%OptimizeFunctionOnNextCall(b);
assertEquals(-125250, b());
assertEquals(-125250, b());

c();
c();
%OptimizeFunctionOnNextCall(c);
assertEquals(-1073741698750, c());
assertEquals(-1073741698750, c());

d();
d();
%OptimizeFunctionOnNextCall(d);
assertEquals(125250, d());
assertEquals(125250, d());
