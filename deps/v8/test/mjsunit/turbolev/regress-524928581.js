// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-use-ic --no-concurrent-osr

function f() {
  for (let i = 0; i < 5; i++) {
    for (let j = 0; j < 5; j++) {
      i >> (null & j);
    }
    %OptimizeOsr();
    i.p;
  }
}

%PrepareFunctionForOptimization(f);
f();
