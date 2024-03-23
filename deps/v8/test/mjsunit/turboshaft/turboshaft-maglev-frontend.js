// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-from-maglev --turbofan
// Flags: --no-always-turbofan

function add_smi(x) {
  return x + x;
}

%PrepareFunctionForOptimization(add_smi);
assertEquals(6, add_smi(3));
%OptimizeFunctionOnNextCall(add_smi);
assertEquals(6, add_smi(3));
assertOptimized(add_smi);
assertEquals("aa", add_smi("a"));
assertUnoptimized(add_smi);
