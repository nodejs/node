// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev


function f(x) {
  return x | 0;
}

%PrepareFunctionForOptimization(f);
f(42);
%OptimizeMaglevOnNextCall(f);
f(42);

// `f` only has Smi feedback for `x | 0`, and should thus generate a
// `CheckedSmiTag` to make sure that `x` is indeed a Smi, but `| 0` being the
// identity, it won't be emitted, and instead, `x` will be returned. Thus, the
// `CheckedSmiTag` is unused, but still shouldn't be optimized out.

assertEquals(4, f(4.5));
