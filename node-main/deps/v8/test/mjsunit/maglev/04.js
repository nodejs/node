// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev
function f(x) {
  while (true) {
    if (x) return 10;
  }
}

%PrepareFunctionForOptimization(f);
assertEquals(10, f(true));

%OptimizeMaglevOnNextCall(f);
assertEquals(10, f(true));
