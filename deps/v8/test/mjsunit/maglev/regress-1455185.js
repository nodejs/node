// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --no-turbofan

function foo(should_osr) {
  const a = [1.1, 1.2, 1.3];
  a[8] = 4.2;
  for (let i = 0; i < 2; i++) {
    let x = a[4];
    try {
      if (i > 0) {
        throw {};
      }
      --x;
      Math.abs();
    } catch {}
    if (should_osr) {
      %OptimizeOsr();
    }
    if (x != undefined) {
      x &= 1;
    }
  }
}

%PrepareFunctionForOptimization(foo);
foo(false);

// OSR
foo(true);
