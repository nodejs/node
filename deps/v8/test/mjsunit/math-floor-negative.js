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

// Flags: --noenable-sse4-1 --allow-natives-syntax

function test1() {
  // Trigger overflow when converting/truncating double to integer.
  // Divide by 10 to avoid overflow when smi-tagging at the end.
  return Math.floor(-100000000000.5) / 10;
}

function test2() {
  // Trigger no overflow.
  return Math.floor(-100.2);
}

function test3() {
  // Trigger overflow when compensating by subtracting after compare.
  // Divide by 10 to avoid overflow when smi-tagging at the end.
  return Math.floor(-2147483648.1) / 10;
}

test1();
test1();
%OptimizeFunctionOnNextCall(test1);
test2();
test2();
%OptimizeFunctionOnNextCall(test2);
test3();
test3();
%OptimizeFunctionOnNextCall(test3);

assertEquals(-10000000000.1, test1());
assertEquals(-101, test2());
assertEquals(-214748364.9, test3());
