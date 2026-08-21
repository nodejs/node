// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --turboshaft --no-lazy-feedback-allocation --allow-natives-syntax

function f1(a3) {
  function f4() {
    for (let [,v10] of "-1328336618") {
      Math.acosh(Math);
      return 3;
    }
  }
  let v13;
  try { v13 = f4(); } catch (e) {}
  [a3].findIndex(f4, v13);
  return f1;
}
%PrepareFunctionForOptimization(f1);
const v16 = f1();
v16();
%OptimizeFunctionOnNextCall(f1);
f1();
