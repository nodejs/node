// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f(a) {
  let v = 3.5 / a;
  let x = 4.5 + v;
  if (x) {
    return 42;
  } else {
    return 12;
  }
}

%PrepareFunctionForOptimization(f);
assertEquals(f(), 12);
%OptimizeMaglevOnNextCall(f);
assertEquals(f(), 12);
