// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/502439789. Turboshaft BuildGraph must not
// fail the dominating_frame_state.valid() DCHECK when OSR-compiling a loop
// that contains Array.prototype.sort.

function test() {
  const a = [[1, 2, 3]];
  for (let i = 0; i < 5; i++) {
    a.sort(Array);
    %OptimizeOsr();
  }
}
%PrepareFunctionForOptimization(test);
test();
