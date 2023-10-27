// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --jit-fuzzing

const o13 = {
  "maxByteLength": 5368789,
};
const v14 = new ArrayBuffer(129, o13);
const v16 = new Uint16Array(v14);

function f3(param) {
  for (let i = 0; i < 5; i++) {
    try {"resize".includes(v14); } catch (e) {}
    v14.resize(3.0, ..."resize", ...v16);
  }

  let f = function() { return param; }
}

%PrepareFunctionForOptimization(f3);
f3();
%OptimizeFunctionOnNextCall(f3);
f3();
