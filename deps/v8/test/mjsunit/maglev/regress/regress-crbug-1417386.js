// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function f(x) {
  x++; // Making sure {x} has a Float64 alternative
  let phi = x || 0;  // Creating a Phi whose inputs are Smi and Float64. Phi
                     // untagging will make this a Float64 phi.
  phi + 1;           // Float64 operation with {phi}, which will cause insert a
                     // CheckedNumberToFloat64(phi). After phi untagging, the
            // CheckedNumberToFloat64 will be killed, resulting in the addition
            // to take {phi} directly as input.
  phi << 4;  // Int operation with {phi}, which will produce a
             // TruncateNumberToInt32(phi). TruncateNumberToInt32 doesn't check
             // if the input is a Number, and call into runtime if necessary to
             // to the truncation, so it can't deopt. After Phi untagging, this
             // should be replaced by a Float64->Int32 truncation
             // (TruncateFloat64ToInt32), which shouldn't deopt either (because
             // we can't replace a non-deopting node by a deopting one).
}

%PrepareFunctionForOptimization(f);
f(1.5);
%OptimizeMaglevOnNextCall(f);
f(1.5);
