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

var cases = [
  [0.0, 0.0, 0.0, 0, 0], [undefined, 0.0, NaN, NaN], [0.0, undefined, NaN, NaN],
  [NaN, 0.0, NaN, NaN], [0.0, NaN, NaN, NaN], [-NaN, 0.0, NaN, NaN],
  [0.0, -NaN, NaN, NaN], [Infinity, 0.0, Infinity, 0.0],
  [0.0, Infinity, Infinity, 0.0], [-Infinity, 0.0, 0.0, -Infinity],
  [0.0, -Infinity, 0.0, -Infinity]
];

function do_min(a, b) {
  return Math.min(a, b);
};
%PrepareFunctionForOptimization(do_min);
function do_max(a, b) {
  return Math.max(a, b);
}

// Make sure that non-crankshaft results match expectations.
;
%PrepareFunctionForOptimization(do_max);
for (i = 0; i < cases.length; ++i) {
  var c = cases[i];
  assertEquals(c[3], do_min(c[0], c[1]));
  assertEquals(c[2], do_max(c[0], c[1]));
}

// Make sure that crankshaft results match expectations.
for (i = 0; i < cases.length; ++i) {
  var c = cases[i];
  %OptimizeFunctionOnNextCall(do_min);
  %OptimizeFunctionOnNextCall(do_max);
  assertEquals(c[3], do_min(c[0], c[1]));
  assertEquals(c[2], do_max(c[0], c[1]));
  %PrepareFunctionForOptimization(do_min);
  %PrepareFunctionForOptimization(do_max);
}
