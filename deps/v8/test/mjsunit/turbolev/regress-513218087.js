// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev --no-lazy-feedback-allocation
// Flags: --single-threaded

const topLevel = %GetFunctionForCurrentFrame();
%PrepareFunctionForOptimization(topLevel);

for (let v1 = 0; v1 < 5; v1++) {
  try {
    typeof globalThis;
    v1 = NaN;
    (() => {
      function f7() {
        for (let v9 = 0; v9 < 2; v9++) {
          globalThis & globalThis;
        }
        return f7;
      }
      f7().call(1, f7);
      %OptimizeOsr(1);
      throw 1;
    })();
  } catch (e) {
  }
}
