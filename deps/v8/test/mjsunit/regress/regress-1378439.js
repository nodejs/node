// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(dv) {
  try {
    for (let i = -1; i < 1; ++i) {
      try {
        dv.setUint16(i % 1);
      } catch (e) {}
    }
  } catch (e) {}
}
const data_view = new DataView(new ArrayBuffer());

%PrepareFunctionForOptimization(f);
f(data_view);
%OptimizeFunctionOnNextCall(f);
f();
