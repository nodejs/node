// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f() {
  for (let v6 = 0; v6 < 10; v6++) {
    ~v6;
    let obj = { enumerable: true, value: v6 };
    Object.defineProperty(Float64Array, 2, obj);
    v6 %= v6;
  }
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeMaglevOnNextCall(f);
f();
