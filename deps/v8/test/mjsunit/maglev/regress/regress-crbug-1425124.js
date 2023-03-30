// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --maglev --allow-natives-syntax

function f() {
  let v8 = 0;
  let v9 = -1024;
  for (let i = 0; i < 5; i++) {
    const v23 = v8 - 55598 | 0;
    const v24 = v8 + 2147483647;
    v8 = v24 | f;
    v9 = ((v9 & v24) && v23) | 0;
  }
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeMaglevOnNextCall(f);
f();
