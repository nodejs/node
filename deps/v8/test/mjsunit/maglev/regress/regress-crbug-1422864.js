// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f(a) {
  let not_a_smi = a ^ 1073741824; // Greater than Smi::kMaxValue (and cannot be
                                  // a Constant, because we don't untag
                                  // Phi(Constant,Constant)).
  let phi = a ? not_a_smi : 4; // During feedback collection, this is a heap
                               // number, but Phi untagging will decide that it
                               // should be a Int32 phi.
  let truncated = phi | 0; // Will insert a CheckedTruncateNumberToInt32
                           // conversion, which will become an Idendity after
                           // phi untagging, but is an input to the following
                           // deopt state, which should thus be updated.
  10 * "a"; // can lazy deopt (an operation that can eager deopt could cause a
            // similar bug, but it's a bit harder to set up the repro, because
            // the deopt state used for lazy deopts is the "current" one,
            // whereas eager deopt can use an earlier state as long as there is
            // no side-effects between the state and the current operation. Here
            // for instance, replacing `10 * "a"` by `10000000 * a` (which can
            // eager deopts) doesn't reproduce the bug, because an earlier deopt
            // state is used, which doesn't contain `truncated`).
  return truncated; // uses `truncated` so that it's part of the lazy deopt state above
}

%PrepareFunctionForOptimization(f);
f(1);
%OptimizeMaglevOnNextCall(f);
f(1);
