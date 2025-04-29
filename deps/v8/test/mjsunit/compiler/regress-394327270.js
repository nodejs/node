// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-always-turbofan --turbofan --js-staging
// Flags: --allow-natives-syntax

function storeAndLoadConstant() {
  let v = new Float16Array(1);
  v[0] = 1;
  return v[0];
}

%PrepareFunctionForOptimization(storeAndLoadConstant);
const x = storeAndLoadConstant();
%OptimizeFunctionOnNextCall(storeAndLoadConstant);
assertEquals(x, storeAndLoadConstant());
