// Copyright 2010 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax --check-elimination

function A(x, y) {
  this.x = x;
  this.y = y;
}

function B(x, y) {
  this.x = x;
  this.y = y;
}

function F1(a, b) {
  if (a == b) return a.x;
  else return b.x;
}

function F2(a, b) {
  if (a == b) return a.x;
  else return b.x;
}

function F3(a, b) {
  var f = a.y;
  if (a == b) return a.x;
  else return b.x;
}

function F4(a, b) {
  var f = b.y;
  if (a == b) return a.x;
  else return b.x;
}

%NeverOptimizeFunction(test);

function test(f, a, b) {
  %PrepareFunctionForOptimization(f);

  f(a, a);
  f(a, b);
  f(b, a);
  f(b, c);
  f(b, b);
  f(c, c);

  %OptimizeFunctionOnNextCall(f)

  assertEquals(a.x, f(a, a));
  assertEquals(b.x, f(b, b));
}

var a = new A(3, 5);
var b = new B(2, 6);
var c = new A(1, 7);

test(F1, a, c);
test(F2, a, b);
test(F3, a, b);
test(F4, a, b);
