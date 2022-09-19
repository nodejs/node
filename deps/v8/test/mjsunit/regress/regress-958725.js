// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(v3) {
  Symbol[Symbol.replace] = Object;
  const v8 = {};
  let i = 0;
  do {
    const v12 = v3[3];
    for (let v17 = 0; v17 < 100000; v17++) {
    }
    const v18 = Object();
    function v19(v20, v21, v22) {}
    i++;;
  } while (i < 1);
  const v25 = Object.freeze(v8);
};
%PrepareFunctionForOptimization(f);
f(Object);
%OptimizeFunctionOnNextCall(f);
f(Object);
