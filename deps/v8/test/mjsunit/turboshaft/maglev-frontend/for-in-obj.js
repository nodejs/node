// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbofan

function foreign(o) {
  let s = 0;
  for (let i in o) {
    s += o[i];
  }
  return s;
}

let o = { x : 42, y : 19, z: 5 };

%PrepareFunctionForOptimization(foreign);
assertEquals(66, foreign(o));
%OptimizeFunctionOnNextCall(foreign);
assertEquals(66, foreign(o));
assertOptimized(foreign);
