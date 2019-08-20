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

// Flags: --allow-natives-syntax --opt --noalways-opt

function mul(a, b) {
    return a * b;
}


%PrepareFunctionForOptimization(mul);
mul(-1, 2);
mul(-1, 2);
%OptimizeFunctionOnNextCall(mul);
assertEquals(-2, mul(-1, 2));
assertOptimized(mul);

// Deopt on minus zero.
assertEquals(-0, mul(-1, 0));
assertUnoptimized(mul);


function mul2(a, b) {
    return a * b;
}

%PrepareFunctionForOptimization(mul2);
mul2(-1, 2);
mul2(-1, 2);
%OptimizeFunctionOnNextCall(mul2);

// 2^30 is a smi boundary on arm and ia32.
var two_30 = 1 << 30;
// 2^31 is a smi boundary on x64.
var two_31 = 2 * two_30;

if (%IsValidSmi(two_31)) {
  // Deopt on two_31 on x64.
  assertEquals(two_31, mul2(-two_31, -1));
  assertUnoptimized(mul2);
} else {
  // Deopt on two_30 on ia32.
  assertEquals(two_30, mul2(-two_30, -1));
  assertUnoptimized(mul2);
}
