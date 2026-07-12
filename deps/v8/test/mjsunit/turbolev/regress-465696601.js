// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  for (let i = 0; i < 5; i++) {
    try {
      if (i > 0) {
        throw 0;
      }
    } catch {}
    %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(foo);
foo();
