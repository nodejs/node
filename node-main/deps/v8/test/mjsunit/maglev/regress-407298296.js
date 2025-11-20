// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f0() {
  return f0;
}

function test() {
  for (let v1 = 0; v1 < 5; v1++) {
    for (const v2 in f0) {
      v1[v2]();
    }
    const v4 = %OptimizeOsr();
  }
}
%PrepareFunctionForOptimization(test);
test();
