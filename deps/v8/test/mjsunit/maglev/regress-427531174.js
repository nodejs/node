// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const v24 = new Int32Array(3433);
function f27() {
  return v24;
}
%PrepareFunctionForOptimization(f27);
function f54() {
  const v56 = Symbol.constructor;
  let v57 = new f27();
  v57[2952] = v57;
  v56 == v57;
}
%PrepareFunctionForOptimization(f54);
f54();
%OptimizeMaglevOnNextCall(f54);
f54();
