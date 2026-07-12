// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax --maglev-untagged-phis

function f(a) {
  let phi = a ? 0.5 : 1.5;
  let truncated_int32_use = phi ^ 2;
  let float64_use = phi + 2.5;
}

%PrepareFunctionForOptimization(f);
f(1);
%OptimizeMaglevOnNextCall(f);
f(1);
