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

// Flags: --allow-natives-syntax --turbofan --no-always-turbofan

// This is a boiled-down example happening in the Epic Citadel demo:
// After deopting, the multiplication for unary minus stayed in Smi
// mode instead of going to double mode, leading to deopt loops.

function unaryMinusTest(x) {
  var g = (1 << x) | 0;
  // Optimized code will contain a LMulI with -1 as right operand.
  return (g & -g) - 1 | 0;
}

%PrepareFunctionForOptimization(unaryMinusTest);
unaryMinusTest(3);
unaryMinusTest(3);
%OptimizeFunctionOnNextCall(unaryMinusTest);
unaryMinusTest(3);
assertOptimized(unaryMinusTest);

// Deopt on kMinInt
unaryMinusTest(31);
// TODO(v8:13245): Investigate why this assertion fails and what intended
// behavior is.
// assertUnoptimized(unaryMinusTest);
%PrepareFunctionForOptimization(unaryMinusTest);

// We should have learned something from the deopt.
unaryMinusTest(31);
%OptimizeFunctionOnNextCall(unaryMinusTest);
unaryMinusTest(31);
assertOptimized(unaryMinusTest);
