// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbofan --turboshaft-loop-optimization
// Flags: --no-turbolev


// TODO(nicohartmann): This is currently testing a dummy optimization behind
// the experimental --turboshaft-loop-optimization flag with the sole purpose to
// verify that the speculation bit attached to the JumpLoop/PrepareForLoop codes
// is properly read and invalidated on deopt.

let g = 0;

function test(x) {
  let count = 0;
  while(x < 1000) {
    ++g;
    ++count;
    ++x;
  }
  return count;
}

%PrepareFunctionForOptimization(test);
assertEquals(295, test(1000-295));
assertEquals(295, g);
%OptimizeFunctionOnNextCall(test);
assertEquals(731, test(1000-731));
assertEquals(295+731, g);
// Currently --turboshaft-loop-optimization triggers unconditional deopt, so we
// assume that we are back into interpreted code here.
assertUnoptimized(test);
%OptimizeFunctionOnNextCall(test);
assertEquals(412, test(1000-412));
assertEquals(295+731+412, g);
// This time, loop speculation should be disabled and we should end with
// optimized code.
assertOptimized(test);
