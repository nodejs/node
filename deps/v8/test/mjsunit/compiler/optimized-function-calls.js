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

// Flags: --allow-natives-syntax --expose-gc

function f() {
  gc();
  return 87;
}


var x = 42, y = 99;
function g() {
  return x | f() | (y | (x | (f() | x)));
}
f();  // Give us a chance to optimize f.
assertEquals(42 | 87 | 99, g());


// Regression test for issue where we would try do an illegal
// compile-time lookup on a null prototype.
var object = { f: function() { return 42; }, x: 42 };
delete object.x;
function call_f(o) {
  return o.f();
}
%PrepareFunctionForOptimization(call_f);
for (var i = 0; i < 5; i++) call_f(object);
%OptimizeFunctionOnNextCall(call_f);
call_f(object);


// Check that nested global function calls work.
function f0() {
  return 42;
}

function f1(a) {
  return a;
}

function f2(a, b) {
  return a * b;
}

function f3(a, b, c) {
  return a + b - c;
}

function f4(a, b, c, d) {
  return a * b + c - d;
}

function nested() {
  return f4(f3(f2(f1(f0()),f0()),f1(f0()),f0()),f2(f1(f0()),f0()),f1(f0()),f0())
    + f4(f0(),f1(f0()),f2(f1(f0()),f0()),f3(f2(f1(f0()),f0()),f1(f0()),f0()));
}
assertEquals(3113460, nested());
