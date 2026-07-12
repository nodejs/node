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

// Deoptimization after a logical not in an effect context should not see a
// value for the logical not expression.
function test0(n) {
  var a = new Array(n);
  for (var i = 0; i < n; ++i) {
    // ~ of a non-numeric value is used to trigger deoptimization.
    a[i] = void !delete 'object' % ~delete 4;
  }
}

// OSR (after deoptimization) is used to observe the stack height mismatch.
for (var i = 0; i < 5; ++i) {
  for (var j = 1; j < 12; ++j) {
    test0(j * 1000);
  }
}


// Similar test with a different subexpression of unary !.
function test1(n) {
  var a = new Array(n);
  for (var i = 0; i < n; ++i) {
    a[i] = void !-'object' % ~delete 4;
  }
}

for (i = 0; i < 5; ++i) {
  for (j = 1; j < 12; ++j) {
    test1(j * 1000);
  }
}


// A similar issue, different subexpression of unary ! (e0 !== e1 is
// translated into !(e0 == e1)) and different effect context.
function side_effect() {}
function observe(x, y) {
  return x;
}
function test2(x) {
  return observe(
      this, (side_effect.observe <= side_effect.side_effect !== false, x + 1));
};
%PrepareFunctionForOptimization(test2);
for (var i = 0; i < 5; ++i) test2(0);
%OptimizeFunctionOnNextCall(test2);
test2(0);
test2(test2);
