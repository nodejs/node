// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis --single-threaded

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

for (let i = 0; i < 5; i++) {
  for (let j = 0; j < 5; j++) {
    var a = j ** j;
  }
  for (let k = 0; k < 5; k++) {
    k.exports;
    %OptimizeOsr();
  }
}
