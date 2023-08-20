// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(x) {
  if (x) return 1;
  return 2;
}

%PrepareFunctionForOptimization(f);
assertEquals(1, f(true));
assertEquals(2, f(false));

%OptimizeMaglevOnNextCall(f);
assertEquals(1, f(true));
assertEquals(2, f(false));
