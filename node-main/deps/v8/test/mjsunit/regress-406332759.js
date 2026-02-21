// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --single-threaded --no-testing-d8-test-runner

for (let v0 = 0; v0 < 5; v0++) {
  function f1() {
      return v0;
  }
  for (let v2 = 0; v2 < 2; v2++) {
      const v3 = %OptimizeOsr();
      let v4 = -43045;
      for (let v5 = 0; v5 < 5; v5++) {
          const v7 = v4 + 4294967295;
          v4 = v7 * v7;
      }
  }
}
