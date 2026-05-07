// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test creates a loop phi that can be untagged and has itself as
// backedge. When we untag it and visit its input, we can thus see an untagged
// Phi on the backedge when performing the untagging even though this was a
// tagged phi when we were processing the inputs, because we first mark the phi
// as untagged before untagging its inputs.

function foo(c) {
  let i = 0.5;
  let s = 7.3;
  if (c == 0) {
    s = 9.3;
  }
  while (i < 42.3) {
    if (c == 25.7) {
      // No feedback so we'll insert a deopt, but it'll trick Maglev to insert a
      // loop phi for {s}.
      s++;
    }
    i += s;
  }
  return s;
}


%PrepareFunctionForOptimization(foo);
assertEquals(9.3, foo(0));
assertEquals(9.3, foo(0));

%OptimizeMaglevOnNextCall(foo);
assertEquals(9.3, foo(0));
