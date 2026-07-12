// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f0() {
  try {
    ([f0,f0]).forEach(undefined);
    class C4 {
      [undefined];
    }
  } catch(e5) {
  }
  return f0;
}
const v6 = %PrepareFunctionForOptimization(f0);
f0();
const v8 = %OptimizeMaglevOnNextCall(f0);
f0();
