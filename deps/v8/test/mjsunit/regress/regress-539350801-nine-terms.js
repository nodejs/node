// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const maxFeedback = 1125899906842623;  // 2^50 - 1

// Nine terms is the shortest chain that can leave the exactly representable
// range: every operand is range checked against the feedback range, so eight
// of them sum to 2^53 - 8, which is still exactly representable, and the ninth
// crosses 2^53, above which the odd integers are no longer representable. No
// multiplication is involved, so the divergence comes purely from the chain.
// The masked seed carries no value; it only gives the chain an operand that is
// already Int32 represented.
function chain(x, a, b, c, d, e, f, g, h, i) {
  const s = x & 0x3ffffff;
  return (s + a + b + c + d + e + f + g + h + i) | 0;
}

function reference(x, a, b, c, d, e, f, g, h, i) {
  const s = x & 0x3ffffff;
  return (s + a + b + c + d + e + f + g + h + i) | 0;
}
%NeverOptimizeFunction(reference);

// Warm up with pairwise cancelling operands, so the feedback is
// AdditiveSafeInteger and no intermediate leaves the representable range. The
// seed has to be zero here: any larger value pushes the running sum past the
// feedback range, which degrades the feedback to Number and makes the test
// vacuous.
const m = maxFeedback;
%PrepareFunctionForOptimization(chain);
chain(0, m, -m, m, -m, m, -m, m, -m, m);
%OptimizeFunctionOnNextCall(chain);
chain(0, m, -m, m, -m, m, -m, m, -m, m);

assertEquals(reference(0, m, m, m, m, m, m, m, m, m),
             chain(0, m, m, m, m, m, m, m, m, m));
