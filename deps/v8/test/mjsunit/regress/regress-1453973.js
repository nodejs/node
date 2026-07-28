// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function opt(){
  const v0 = [];
  let v1 = Infinity;
  const v2 = v1.E;
  for (let v5 = -1; v5 > v0; v5 = v5 & -1) {
    for (let v6 = v5; v6 <= -1; v6 = v6 + v2) {
      const v7 = v1++;
      for (let v9 = v6; v9 <= v7; v9 = v9 + v5) {
      }
    }
  }
}

// We want to optimize without feedback. We still need to
// %PrepareFunctionForOptimization though to make bots happy.
%PrepareFunctionForOptimization(opt);
%OptimizeFunctionOnNextCall(opt);
let jit_a2 = opt();
