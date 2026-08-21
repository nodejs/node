// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function victim(x, a) {
  let shl_amt = Math.clz32(0) * 2; // 64
  let shr_amt = (Math.clz32(0) * -1) | 0; // -32 (0xFFFFFFE0)
  let shl = x << shl_amt;
  let shr = x >>> shr_amt;
  return shl | shr | a | 0;
}

%PrepareFunctionForOptimization(victim);
victim(1, 2);
%OptimizeFunctionOnNextCall(victim);
victim(1, 2);
