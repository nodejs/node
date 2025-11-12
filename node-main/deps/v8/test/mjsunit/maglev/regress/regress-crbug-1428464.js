// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax --maglev-untagged-phis

function f(a) {
  let heap_number = 2000000000;
  let phi = a ? 42 : heap_number;
  let float64_use = phi + 1.5;
  let truncated_int32_use = phi ^ 25;
}

%PrepareFunctionForOptimization(f);
f(0);
%OptimizeMaglevOnNextCall(f);
f(1);
