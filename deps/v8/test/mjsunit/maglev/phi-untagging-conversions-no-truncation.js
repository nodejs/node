// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax
// Flags: --no-optimize-maglev-optimizes-to-turbofan
// Flags: --no-maglev-truncated-int32-phis

// In this example, during feedback collection, we provide inputs that cause
// `phi` to always be a Smi, which means that `phi | 0` will have Smi
// feedback. The graph will thus be:
//
//    1: Phi(d, #42)
//    2: CheckedSmiUntag(1)
//    3: Return(1)
//
// Except that Phi untagging will realize that `phi` could be a Float64, and
// will thus decide that it should be a Float64 Phi. In this case, the
// conversion should not be dropped, because if the Phi is not an Int32, then it
// should not be returned directly, but instead be truncated. In practice
// though, we'll rather replace it by a CheckedHoleyFloat64ToInt32, which
// will fail if the Float64 isn't an Int32, causing a deopt, and the reoptimized
// code will have a better feedback (this is easier than to try to patch the
// `Return(1)` to return something else).
function unused_required_CheckedSmiUntag(x) {
  x = x + 0.5; // ensuring Float64 alternative
  let d = x + 2.53;
  let phi = x ? d : 42;
  return phi | 0;
}

%PrepareFunctionForOptimization(unused_required_CheckedSmiUntag);
unused_required_CheckedSmiUntag(-0.5);
%OptimizeMaglevOnNextCall(unused_required_CheckedSmiUntag);
assertEquals(42, unused_required_CheckedSmiUntag(-0.5));
assertOptimized(unused_required_CheckedSmiUntag);
// If the CheckedSmiUntag is dropped, then the truncation won't be done, and the
// non-truncated float (3.53) will be returned. Instead, if the conversion is
// changed to CheckedHoleyFloat64ToInt32, then it will deopt, and we'll get
// the correct result of 3.
assertEquals(3, unused_required_CheckedSmiUntag(0.5));
assertUnoptimized(unused_required_CheckedSmiUntag);
