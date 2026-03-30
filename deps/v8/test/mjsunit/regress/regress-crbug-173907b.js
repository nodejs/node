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

var X = 1.1;
var K = 0.5;

var O = 0;
var result = new Float64Array(2);

%NeverOptimizeFunction(spill);
function spill() {
}

function buggy() {
  var v = X;
  var phi1 = v + K;
  var phi2 = v - K;

  spill();  // At this point initial values for phi1 and phi2 are spilled.

  var xmm1 = v;
  var xmm2 = v * v * v;
  var xmm3 = v * v * v * v;
  var xmm4 = v * v * v * v * v;
  var xmm5 = v * v * v * v * v * v;
  var xmm6 = v * v * v * v * v * v * v;
  var xmm7 = v * v * v * v * v * v * v * v;
  var xmm8 = v * v * v * v * v * v * v * v * v;

  // All registers are blocked and phis for phi1 and phi2 are spilled because
  // their left (incoming) value is spilled, there are no free registers,
  // and phis themselves have only ANY-policy uses.

  for (var x = 0; x < 2; x++) {
    xmm1 += xmm1 * xmm6;
    xmm2 += xmm1 * xmm5;
    xmm3 += xmm1 * xmm4;
    xmm4 += xmm1 * xmm3;
    xmm5 += xmm1 * xmm2;

    // Now swap values of phi1 and phi2 to create cycle between phis.
    var t = phi1;
    phi1 = phi2;
    phi2 = t;
  }

  // Now we want to get values of phi1 and phi2. However we would like to
  // do it in a way that does not produce any uses of phi1&phi2 that have
  // a register beneficial policy. How? We just hide these uses behind phis.
  result[0] = O === 0 ? phi1 : phi2;
  result[1] = O !== 0 ? phi1 : phi2;
};
%PrepareFunctionForOptimization(buggy);
function test() {
  buggy();
  assertArrayEquals([X + K, X - K], result);
}

test();
test();
%OptimizeFunctionOnNextCall(buggy);
test();
