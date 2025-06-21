// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function opt(opt_param){
  const v5 = [8];
  Object.defineProperty(v5, "c", { writable: true, value: -4.1595484640962705e+307 });
  let v7 = false;
  let v8 = 0;

  for (let i = 0; i < 5; i++) {
    v5.length = v8;
    v8++;
    for (let v10 = 0; v10 < 32; v10++) {
      v5["p" + v10] = v10;
    }
  }
  return v5;
}
opt(false);
%PrepareFunctionForOptimization(opt);
opt(true);
%OptimizeFunctionOnNextCall(opt);
opt(false);
