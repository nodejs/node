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
// Test expressions that can be computed with a multiply-add instruction.

function f(a, b, c) {
  return a - b * c;
}

function g(a, b, c) {
  return a * b - c;
}

function h(a, b, c, d) {
  return a * b - c * d;
}

assertEquals(-5.41, f(1.1, 2.1, 3.1));
assertEquals(-5.41, f(1.1, 2.1, 3.1));
%OptimizeFunctionOnNextCall(f);
assertEquals(-5.41, f(1.1, 2.1, 3.1));

assertEquals(8.36, g(2.2, 3.3, -1.1));
assertEquals(8.36, g(2.2, 3.3, -1.1));
%OptimizeFunctionOnNextCall(g);
assertEquals(8.36, g(2.2, 3.3, -1.1));

assertEquals(-1.5, h(1.5, 3.0, 12, 0.5));
assertEquals(-1.5, h(1.5, 3.0, 12, 0.5));
%OptimizeFunctionOnNextCall(h);
assertEquals(-1.5, h(1.5, 3.0, 12, 0.5));
