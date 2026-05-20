// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f8(a10) {
  for (let v11 = 0; v11 < 5; v11++) {
    const t7 = {};
    t7.f = v11;
    ({"f":v11, "g":a10} = "c");
    Math.atanh(a10);
    function f15() {
    }
  }
}
%PrepareFunctionForOptimization(f8);
f8();
%OptimizeMaglevOnNextCall(f8);
f8();
