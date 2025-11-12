// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function v0() {
  let v7 = -4294967295;
  try {
    for (let v11 = 0; v11 < 8; v11++) {
      const v13 = Symbol.isConcatSpreadable;
      const v14 = v11 && v13;
      const v15 = v7 <= v14;
      for (var i = 0; i < 10; i++) {}
    }
  } catch (v20) {
  }
};
%PrepareFunctionForOptimization(v0);
v0();
v0();
%OptimizeFunctionOnNextCall(v0);
v0();
