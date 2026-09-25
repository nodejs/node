// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --no-maglev --allow-natives-syntax
// Flags: --maglev-truncated-int32-phis --turbofan --turbolev

// In this example, during feedback collection, we provide inputs that cause
// `phi` to always be a Smi, which means that `phi | 0` will have Smi
// feedback. The graph will thus be:
//
//    1: Phi(d, #42)
//    2: CheckedSmiUntag(1)
//    3: Return(1)
//
// Except that Phi untagging will realize that `phi` could be a Float64, but
// since it's only use is a TruncatedInt32. The representation selector
// will thus decide that it should be a TruncatedInt32 Phi. In this case, the
// conversion will be dropped, and we will emit a TruncateFloat64ToInt32.

function unused_required_CheckedSmiUntag(x) {
  x = x + 0.5; // ensuring Float64 alternative
  let d = x + 2.53;
  let phi = x ? d : 42;
  return phi | 0;
}

%PrepareFunctionForOptimization(unused_required_CheckedSmiUntag);
unused_required_CheckedSmiUntag(-0.5);
%OptimizeFunctionOnNextCall(unused_required_CheckedSmiUntag);
assertEquals(42, unused_required_CheckedSmiUntag(-0.5));
assertOptimized(unused_required_CheckedSmiUntag);
// Even if we need to truncate 3.53, we don't deopt.
assertEquals(3, unused_required_CheckedSmiUntag(0.5));
assertOptimized(unused_required_CheckedSmiUntag);
