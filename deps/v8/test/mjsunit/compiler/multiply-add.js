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

// Flags: --allow-natives-syntax
// Test expressions that can be computed with a multiply-add instruction.

function f(a, b, c) {
  return a * b + c;
}

function g(a, b, c) {
  return a + b * c;
}

function h(a, b, c, d) {
  return a * b + c * d;
}

assertEquals(5, f(1, 2, 3));
assertEquals(5, f(1, 2, 3));
%OptimizeFunctionOnNextCall(f);
assertEquals(5, f(1, 2, 3));
assertEquals("2foo", f(1, 2, "foo"));
assertEquals(5.41, f(1.1, 2.1, 3.1));
assertEquals(5.41, f(1.1, 2.1, 3.1));
%OptimizeFunctionOnNextCall(f);
assertEquals(5.41, f(1.1, 2.1, 3.1));

assertEquals(7, g(1, 2, 3));
assertEquals(7, g(1, 2, 3));
%OptimizeFunctionOnNextCall(g);
assertEquals(7, g(1, 2, 3));
assertEquals(8.36, g(1.1, 2.2, 3.3));
assertEquals(8.36, g(1.1, 2.2, 3.3));
%OptimizeFunctionOnNextCall(g);
assertEquals(8.36, g(1.1, 2.2, 3.3));

assertEquals(14, h(1, 2, 3, 4));
assertEquals(14, h(1, 2, 3, 4));
%OptimizeFunctionOnNextCall(h);
assertEquals(14, h(1, 2, 3, 4));
assertEquals(15.02, h(1.1, 2.1, 3.1, 4.1));
assertEquals(15.02, h(1.1, 2.1, 3.1, 4.1));
%OptimizeFunctionOnNextCall(h);
assertEquals(15.02, h(1.1, 2.1, 3.1, 4.1));
