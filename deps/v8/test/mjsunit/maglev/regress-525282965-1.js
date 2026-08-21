// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --fuzzing --allow-natives-syntax --maglev-assert

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

%OptimizeOsr();

for (let i = 0, j = 100000; i / i, j; --j) {
  i = j;
}
try {} catch (e) {}
