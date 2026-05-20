// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function trigger(cond) {
  let o = {};
  let mul = (cond ? 1 : 0x80000000) | 0;
  print(mul);
  let idx = (mul * 2) | 0;
  print(idx);
  o[0] = 1.1;
  if (cond) o[1] = 2.2;
  return o[idx];
}

%PrepareFunctionForOptimization(trigger);
trigger(true);
trigger(false);
%OptimizeMaglevOnNextCall(trigger);
trigger(false);
